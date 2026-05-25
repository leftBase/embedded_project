#include "simulator.h"
#include "hardware.h"
#include "serial.h"
#include "debug.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define SIM_QUEUE_SIZE 64

typedef struct {
    GameEvent buffer[SIM_QUEUE_SIZE];
    int head;
    int tail;
    int closed;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} SimEventQueue;

static FILE *sim_log_fp;
static SimEventQueue sim_serial_queue = {
    { { EV_NONE, 0, ITEM_NONE } }, 0, 0, 0,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_COND_INITIALIZER
};
static SimEventQueue sim_hw_queue = {
    { { EV_NONE, 0, ITEM_NONE } }, 0, 0, 0,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_COND_INITIALIZER
};

static FILE *sim_log_open(void) {
    const char *log_path;

    if (sim_log_fp != NULL) {
        return sim_log_fp;
    }

    log_path = getenv("SIM_LOG_PATH");
    if (log_path == NULL || log_path[0] == '\0') {
        log_path = "sim_serial.log";
    }

    sim_log_fp = fopen(log_path, "a");
    if (sim_log_fp != NULL) {
        setvbuf(sim_log_fp, NULL, _IOLBF, 0);
    }

    return sim_log_fp;
}

static void sim_log_printf(const char *fmt, ...) {
    FILE *fp = sim_log_open();
    va_list args;

    if (fp == NULL) {
        return;
    }

    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);

    fputc('\n', fp);
    fflush(fp);
}

static int queue_next_index(int index) {
    return (index + 1) % SIM_QUEUE_SIZE;
}

static int queue_is_empty(const SimEventQueue *q) {
    return q->head == q->tail;
}

static int queue_is_full(const SimEventQueue *q) {
    return queue_next_index(q->tail) == q->head;
}

static void sim_queue_reset(SimEventQueue *q) {
    pthread_mutex_lock(&q->lock);
    q->head = 0;
    q->tail = 0;
    q->closed = 0;
    pthread_mutex_unlock(&q->lock);
}

static void sim_queue_close(SimEventQueue *q) {
    pthread_mutex_lock(&q->lock);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

static void sim_queue_push(SimEventQueue *q, GameEvent event, const char *source) {
    pthread_mutex_lock(&q->lock);

    if (!q->closed) {
        if (queue_is_full(q)) {
            q->head = queue_next_index(q->head);
            sim_log_printf("[SIM][WARN] queue=%s overflow oldest_dropped=1", source);
        }

        q->buffer[q->tail] = event;
        q->tail = queue_next_index(q->tail);
        pthread_cond_signal(&q->not_empty);
    }

    pthread_mutex_unlock(&q->lock);
}

static int sim_queue_pop(SimEventQueue *q, GameEvent *event) {
    pthread_mutex_lock(&q->lock);

    while (queue_is_empty(q) && !q->closed) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    if (queue_is_empty(q) && q->closed) {
        pthread_mutex_unlock(&q->lock);
        errno = EBADF;
        return -1;
    }

    *event = q->buffer[q->head];
    q->head = queue_next_index(q->head);

    pthread_mutex_unlock(&q->lock);
    return 1;
}

static const char *event_name(EventType type) {
    switch (type) {
        case EV_NONE: return "EV_NONE";
        case EV_TICK: return "EV_TICK";
        case EV_START: return "EV_START";
        case EV_PAUSE: return "EV_PAUSE";
        case EV_QUIT: return "EV_QUIT";
        case EV_P1_LEFT: return "EV_P1_LEFT";
        case EV_P1_RIGHT: return "EV_P1_RIGHT";
        case EV_P1_SKILL: return "EV_P1_SKILL";
        case EV_P2_LEFT: return "EV_P2_LEFT";
        case EV_P2_RIGHT: return "EV_P2_RIGHT";
        case EV_P2_SKILL: return "EV_P2_SKILL";
        case EV_SIM_FORCE_ITEM: return "EV_SIM_FORCE_ITEM";
        default: return "EV_UNKNOWN";
    }
}

static EventType event_from_m4_button(int button_id) {
    switch (button_id) {
        case 0: return EV_P1_LEFT;
        case 1: return EV_P1_RIGHT;
        case 2: return EV_START;
        case 3: return EV_P2_LEFT;
        case 4: return EV_P2_RIGHT;
        default: return EV_NONE;
    }
}

static EventType event_from_q6_key(int key_code) {
    switch (key_code) {
        case Q6_KEY_BACK: return EV_P1_SKILL;
        case Q6_KEY_HOME: return EV_PAUSE;
        case Q6_KEY_MENU: return EV_P2_SKILL;
        default: return EV_NONE;
    }
}

static int led_from_q6_key(int key_code) {
    switch (key_code) {
        case Q6_KEY_BACK: return Q6_LED_BACK;
        case Q6_KEY_HOME: return Q6_LED_HOME;
        case Q6_KEY_MENU: return Q6_LED_MENU;
        default: return -1;
    }
}

static const char *lcd_name(int preset) {
    switch (preset) {
        case 0: return "LOGO";
        case 1: return "RED";
        case 2: return "GREEN";
        case 3: return "BLUE";
        default: return "UNKNOWN";
    }
}

void sim_autotest_note(const char *message) {
    sim_log_printf("[SIM][AUTOTEST] %s", message);
}

void sim_push_m4_button(int button_id, int pressed) {
    EventType type;
    GameEvent event;
    int state = pressed ? 1 : 0;

    sim_log_printf("[SIM][M4_BUTTON_RX] packet=12 22 %02X %02X 13 id=%d pressed=%d",
                   0x30 + button_id,
                   0x30 + state,
                   button_id,
                   state);

    if (button_id < 0 || button_id > 4) {
        return;
    }

    (void)serial_send_led(button_id, state);

    if (!state) {
        return;
    }

    type = event_from_m4_button(button_id);
    if (type == EV_NONE) {
        return;
    }

    event.type = type;
    event.value = 1;
    event.item_type = ITEM_NONE;

    sim_log_printf("[SIM][M4_EVENT] id=%d type=%s queued=1", button_id, event_name(type));
    sim_queue_push(&sim_serial_queue, event, "serial");
}

void sim_push_q6_key(int key_code, int pressed) {
    EventType type;
    GameEvent event;
    int state = pressed ? 1 : 0;
    int led_pin = led_from_q6_key(key_code);

    sim_log_printf("[SIM][Q6_KEY_RX] code=%d pressed=%d", key_code, state);

    if (led_pin >= 0) {
        led_control(led_pin, state);
    }

    if (!state) {
        return;
    }

    type = event_from_q6_key(key_code);
    if (type == EV_NONE) {
        return;
    }

    event.type = type;
    event.value = 1;
    event.item_type = ITEM_NONE;

    sim_log_printf("[SIM][Q6_EVENT] code=%d type=%s queued=1", key_code, event_name(type));
    sim_queue_push(&sim_hw_queue, event, "hardware");
}

int hw_init(void) {
    sim_queue_reset(&sim_hw_queue);
    printf("[SIM] hardware init\n");
    fflush(stdout);
    sim_log_printf("[SIM] hardware init");
    return 0;
}

void hw_close(void) {
    int pins[] = { Q6_LED_BACK, Q6_LED_HOME, Q6_LED_MENU };
    int i;

    for (i = 0; i < 3; i++) {
        led_control(pins[i], 0);
    }

    sim_queue_close(&sim_hw_queue);
    printf("[SIM] hardware close\n");
    fflush(stdout);
    sim_log_printf("[SIM] hardware close");
}

int hw_next_event(GameEvent *event) {
    return sim_queue_pop(&sim_hw_queue, event);
}

void led_control(int led_pin, int state) {
    printf("[SIM] q6 led pin=%d state=%d\n", led_pin, state);
    fflush(stdout);
    sim_log_printf("[SIM][Q6_LED] pin=%d state=%s", led_pin, state ? "ON" : "OFF");
}

void buzzer_play(double freq, int time_us) {
    printf("[SIM] buzzer freq=%.2f time_us=%d\n", freq, time_us);
    fflush(stdout);
    sim_log_printf("[SIM][BUZZER] freq=%.2f time_us=%d", freq, time_us);
}

int button_read_event(int *code, int *value) {
    if (code != NULL) {
        *code = 0;
    }

    if (value != NULL) {
        *value = 0;
    }

    return 0;
}

int m4_uart_init(void) {
    sim_queue_reset(&sim_serial_queue);
    sim_log_printf("[SIM] serial init");
    return 0;
}

void serial_close(void) {
    int i;

    for (i = 0; i < 5; i++) {
        (void)serial_send_led(i, 0);
    }

    sim_queue_close(&sim_serial_queue);
    sim_log_printf("[SIM] serial close");
}

ssize_t serial_write(const void *buf, size_t len) {
    static int verbose = -1;
    const unsigned char *bytes = (const unsigned char *)buf;
    size_t i;

    if (verbose == -1) {
        verbose = getenv("SIM_SERIAL_VERBOSE") ? 1 : 0;
    }

    if (verbose) {
        FILE *fp = sim_log_open();

        if (fp == NULL) {
            return (ssize_t)len;
        }

        fprintf(fp, "[SIM][RAW_TX] len=%zu", len);
        for (i = 0; i < len; i++) {
            fprintf(fp, " %02X", bytes[i]);
        }
        fputc('\n', fp);
        fflush(fp);
    }

    return (ssize_t)len;
}

int serial_next_event(GameEvent *event) {
    return sim_queue_pop(&sim_serial_queue, event);
}

int serial_send_lcd(int preset) {
    static int last_lcd = -1;

    if (preset < 0) {
        preset = 0;
    }

    if (preset > 3) {
        preset = 3;
    }

    if (preset != last_lcd) {
        last_lcd = preset;
        sim_log_printf("[SIM][LCD] preset=%d label=%s packet=12 24 30 %02X 13",
                       preset,
                       lcd_name(preset),
                       0x30 + preset);
        DBG("sim_lcd=%d", preset);
    }

    return 0;
}

int serial_send_fnd_digit(int position, int value) {
    if (position < 0) {
        position = 0;
    }
    if (position > 2) {
        position = 2;
    }
    if (value < 0) {
        value = 0;
    }
    if (value > 9) {
        value = 9;
    }

    sim_log_printf("[SIM][FND_DIGIT] pos=%d value=%d packet=12 23 %02X %02X 13",
                   position,
                   value,
                   0x30 + position,
                   0x30 + value);
    return 0;
}

int serial_send_fnd_number(int number) {
    int hundreds;
    int tens;
    int ones;

    if (number < 0) {
        number = 0;
    }

    number %= 1000;
    hundreds = number / 100;
    tens = (number / 10) % 10;
    ones = number % 10;

    sim_log_printf("[SIM][FND] number=%d digits=%d%d%d",
                   number,
                   hundreds,
                   tens,
                   ones);
    DBG("sim_fnd=%d", number);

    (void)serial_send_fnd_digit(0, hundreds);
    (void)serial_send_fnd_digit(1, tens);
    (void)serial_send_fnd_digit(2, ones);
    return 0;
}

int serial_send_led(int led_index, int state) {
    int packet_state;

    if (led_index < 0) {
        led_index = 0;
    }
    if (led_index > 4) {
        led_index = 4;
    }

    packet_state = state ? 1 : 0;
    sim_log_printf("[SIM][M4_LED] index=%d state=%s packet=12 21 %02X %02X 13",
                   led_index,
                   state ? "ON" : "OFF",
                   0x30 + led_index,
                   0x30 + packet_state);
    return 0;
}
