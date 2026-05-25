#include "hardware.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define MAX_BUF 64

#define IOCTL_START_BUZZER _IOW('b', 0x07, int)
#define IOCTL_END_BUZZER   _IOW('b', 0x09, int)
#define IOCTL_SET_TONE     _IOW('b', 0x0b, int)
#define IOCTL_SET_VOLUME   _IOW('b', 0x0c, int)

static int buzzer_fd = -1;
static int button_fd = -1;
static const int q6_led_pins[] = { Q6_LED_BACK, Q6_LED_HOME, Q6_LED_MENU };
static int q6_led_exported[3];

static int gpio_export(unsigned int gpio) {
    int fd;
    int len;
    char buf[MAX_BUF];

    fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
    if (fd < 0) {
        return -1;
    }

    len = snprintf(buf, sizeof(buf), "%u", gpio);
    if (write(fd, buf, len) < 0) {
        int saved_errno = errno;

        close(fd);
        if (saved_errno == EBUSY) {
            return 0;
        }
        errno = saved_errno;
        return -1;
    }

    close(fd);
    return 1;
}

static int gpio_unexport(unsigned int gpio) {
    int fd;
    int len;
    char buf[MAX_BUF];

    fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
    if (fd < 0) {
        return -1;
    }

    len = snprintf(buf, sizeof(buf), "%u", gpio);
    if (write(fd, buf, len) < 0) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int gpio_set_dir(unsigned int gpio, int output) {
    int fd;
    char path[MAX_BUF];
    const char *value = output ? "out\n" : "in\n";

    snprintf(path, sizeof(path), SYSFS_GPIO_DIR "/gpio%u/direction", gpio);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        return -1;
    }

    if (write(fd, value, strlen(value)) < 0) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int gpio_set_value(unsigned int gpio, int value) {
    int fd;
    char path[MAX_BUF];
    const char *out = value ? "1\n" : "0\n";

    snprintf(path, sizeof(path), SYSFS_GPIO_DIR "/gpio%u/value", gpio);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        return -1;
    }

    if (write(fd, out, strlen(out)) < 0) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static void init_q6_leds(void) {
    int i;

    for (i = 0; i < (int)(sizeof(q6_led_pins) / sizeof(q6_led_pins[0])); i++) {
        q6_led_exported[i] = gpio_export((unsigned int)q6_led_pins[i]) > 0;
        (void)gpio_set_dir((unsigned int)q6_led_pins[i], 1);
        (void)gpio_set_value((unsigned int)q6_led_pins[i], HW_LED_OFF);
    }
}

static int led_from_q6_key(int key_code) {
    switch (key_code) {
        case Q6_KEY_BACK:
            return Q6_LED_BACK;
        case Q6_KEY_HOME:
            return Q6_LED_HOME;
        case Q6_KEY_MENU:
            return Q6_LED_MENU;
        default:
            return -1;
    }
}

static void update_q6_key_led(const struct input_event *ev) {
    int led_pin;

    if (ev->type != EV_KEY || (ev->value != 0 && ev->value != 1)) {
        return;
    }

    led_pin = led_from_q6_key(ev->code);
    if (led_pin >= 0) {
        led_control(led_pin, ev->value ? HW_LED_ON : HW_LED_OFF);
    }
}

int hw_init(void) {
    buzzer_fd = open("/dev/buzzer", O_RDWR);
    if (buzzer_fd < 0) {
        printf("Buzzer open failed. Will retry when sound plays.\n");
        buzzer_fd = -1;
    }

    button_fd = open("/dev/input/event1", O_RDONLY);
    if (button_fd < 0) {
        printf("Button open failed!\n");
        hw_close();
        return -1;
    }

    init_q6_leds();

    printf("Hardware Initialized.\n");
    return 0;
}

void hw_close(void) {
    int i;

    for (i = 0; i < (int)(sizeof(q6_led_pins) / sizeof(q6_led_pins[0])); i++) {
        (void)gpio_set_value((unsigned int)q6_led_pins[i], HW_LED_OFF);
        if (q6_led_exported[i]) {
            (void)gpio_unexport((unsigned int)q6_led_pins[i]);
            q6_led_exported[i] = 0;
        }
    }

    if (buzzer_fd >= 0) {
        close(buzzer_fd);
        buzzer_fd = -1;
    }

    if (button_fd >= 0) {
        close(button_fd);
        button_fd = -1;
    }

    printf("Hardware Closed.\n");
}

void buzzer_play(double freq, int time_us) {
    long tone;

    if (freq <= 0.0 || time_us <= 0) {
        return;
    }

    if (buzzer_fd < 0) {
        buzzer_fd = open("/dev/buzzer", O_RDWR);
        if (buzzer_fd < 0) {
            return;
        }
    }

    tone = (long)((1.0f / freq) * 1000000000);
    if (ioctl(buzzer_fd, IOCTL_SET_TONE, tone) < 0 ||
        ioctl(buzzer_fd, IOCTL_SET_VOLUME, 25000) < 0 ||
        ioctl(buzzer_fd, IOCTL_START_BUZZER, 0) < 0) {
        close(buzzer_fd);
        buzzer_fd = -1;
        return;
    }

    usleep(time_us);
    (void)ioctl(buzzer_fd, IOCTL_END_BUZZER, 0);
}

void led_control(int led_pin, int state) {
    (void)gpio_set_value((unsigned int)led_pin, state ? HW_LED_ON : HW_LED_OFF);
}

static int map_q6_key_event(const struct input_event *ev, GameEvent *event) {
    if (ev->type != EV_KEY || ev->value != 1) {
        return 0;
    }

    event->value = 1;
    event->item_type = ITEM_NONE;

    switch (ev->code) {
        case Q6_KEY_BACK:
            event->type = EV_P1_SKILL;
            return 1;

        case Q6_KEY_HOME:
            event->type = EV_PAUSE;
            return 1;

        case Q6_KEY_MENU:
            event->type = EV_P2_SKILL;
            return 1;

        default:
            event->type = EV_NONE;
            return 0;
    }
}

int hw_next_event(GameEvent *event) {
    struct input_event ev;
    ssize_t n;

    if (event == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (button_fd < 0) {
        errno = EBADF;
        return -1;
    }

    n = read(button_fd, &ev, sizeof(ev));
    if (n < 0) {
        return errno == EINTR ? 0 : -1;
    }
    if (n != sizeof(ev)) {
        return 0;
    }

    update_q6_key_led(&ev);

    return map_q6_key_event(&ev, event);
}

int button_read_event(int *code, int *value) {
    struct input_event ev;
    ssize_t n;

    if (code == NULL || value == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (button_fd < 0) {
        errno = EBADF;
        return -1;
    }

    n = read(button_fd, &ev, sizeof(ev));
    if (n < 0) {
        return errno == EINTR ? 0 : -1;
    }

    if (n != sizeof(ev) || ev.type != EV_KEY) {
        return 0;
    }

    *code = ev.code;
    *value = ev.value;
    return 1;
}
