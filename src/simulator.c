#include "hardware.h"
#include "serial.h"
#include "debug.h"

#include <stdio.h>

int hw_init(void) {
    printf("[SIM] hardware init\n");
    fflush(stdout);
    return 0;
}

void hw_close(void) {
    printf("[SIM] hardware close\n");
    fflush(stdout);
}

int hw_next_event(GameEvent *event) {
    (void)event;
    return 0;
}

void led_control(int led_pin, int state) {
    printf("[SIM] led pin=%d state=%d\n", led_pin, state);
    fflush(stdout);
}

void buzzer_play(double freq, int time_us) {
    printf("[SIM] buzzer freq=%.2f time_us=%d\n", freq, time_us);
    fflush(stdout);
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
    printf("[SIM] serial init\n");
    fflush(stdout);
    return 0;
}

void serial_close(void) {
    printf("[SIM] serial close\n");
    fflush(stdout);
}

ssize_t serial_write(const void *buf, size_t len) {
    /* By default avoid printing every raw packet to prevent flooding the
     * terminal and interfering with full-screen rendering. Enable verbose
     * raw output by setting environment variable SIM_SERIAL_VERBOSE=1. */
    static int verbose = -1;
    const unsigned char *bytes = (const unsigned char *)buf;
    size_t i;

    if (verbose == -1) {
        verbose = getenv("SIM_SERIAL_VERBOSE") ? 1 : 0;
    }

    if (verbose) {
        printf("[SIM] serial write len=%zu", len);
        for (i = 0; i < len; i++) {
            printf(" %02X", bytes[i]);
        }
        printf("\n");
        fflush(stdout);
    }

    return (ssize_t)len;
}

int serial_next_event(GameEvent *event) {
    (void)event;
    return 0;
}

int serial_send_lcd(int preset) {
    static int last_lcd = -1;

    if (preset != last_lcd) {
        last_lcd = preset;
        printf("[SIM] LCD preset=%d\n", preset);
        fflush(stdout);
        DBG("sim_lcd=%d", preset);
    }

    return 0;
}

int serial_send_fnd_digit(int position, int value) {
    /* fnd digit writes are noisy; prefer higher-level number logging in
     * serial_send_fnd_number. Keep this as a no-op in simulator to avoid
     * excessive output. */
    (void)position;
    (void)value;
    return 0;
}

int serial_send_fnd_number(int number) {
    static int last_number = -1;

    if (number != last_number) {
        last_number = number;
        printf("[SIM] FND number=%d\n", number);
        fflush(stdout);
        DBG("sim_fnd=%d", number);
    }

    return 0;
}