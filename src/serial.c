#include "serial.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#define PACKET_START 0x12
#define PACKET_END 0x13
#define CMD_M4_BUTTON 0x22

#define Q6_KEY_BACK 158
#define Q6_KEY_HOME 172
#define Q6_KEY_MENU 139

static int q6_button_fd = -1;
static int m4_uart_fd = -1;

static speed_t baud_to_speed(int baudrate) {
    switch (baudrate) {
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
        default:
            return B115200;
    }
}

static int configure_uart(int fd, int baudrate) {
    struct termios tio;

    memset(&tio, 0, sizeof(tio));

    tio.c_cflag = baud_to_speed(baudrate) | CS8 | CLOCAL | CREAD;
#ifdef CRTSCTS
    tio.c_cflag |= CRTSCTS;
#endif
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VTIME] = 0;
    tio.c_cc[VMIN] = 5;

    return tcsetattr(fd, TCSANOW, &tio);
}

static int decode_digit_or_raw(unsigned char byte) {
    if (byte >= '0' && byte <= '9') {
        return byte - '0';
    }

    return byte;
}

static int map_q6_key_event(const struct input_event *input, GameEvent *event) {
    if (input->type != EV_KEY || input->value != 1) {
        return 0;
    }

    event->value = 1;
    event->item_type = ITEM_NONE;

    switch (input->code) {
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

static int map_m4_button_event(int button_id, int pressed, GameEvent *event) {
    if (!pressed) {
        return 0;
    }

    event->value = 1;
    event->item_type = ITEM_NONE;

    switch (button_id) {
        case 0:
            event->type = EV_P1_LEFT;
            return 1;

        case 1:
            event->type = EV_P1_RIGHT;
            return 1;

        case 2:
            event->type = EV_START;
            return 1;

        case 3:
            event->type = EV_P2_LEFT;
            return 1;

        case 4:
            event->type = EV_P2_RIGHT;
            return 1;

        default:
            event->type = EV_NONE;
            return 0;
    }
}

static int read_q6_event(GameEvent *event) {
    struct input_event input;
    ssize_t n = read(q6_button_fd, &input, sizeof(input));

    if (n < 0) {
        return errno == EINTR ? 0 : -1;
    }

    if (n != sizeof(input)) {
        return 0;
    }

    return map_q6_key_event(&input, event);
}

static int read_m4_event(GameEvent *event) {
    unsigned char packet[5];
    ssize_t n = read(m4_uart_fd, packet, sizeof(packet));
    int button_id;
    int pressed;

    if (n < 0) {
        return errno == EINTR ? 0 : -1;
    }

    if (n != sizeof(packet)) {
        return 0;
    }

    if (packet[0] != PACKET_START || packet[4] != PACKET_END) {
        return 0;
    }

    if (packet[1] != CMD_M4_BUTTON) {
        return 0;
    }

    button_id = decode_digit_or_raw(packet[2]);
    pressed = decode_digit_or_raw(packet[3]);

    return map_m4_button_event(button_id, pressed, event);
}

int serial_init(const char *device, int baudrate) {
    const char *m4_uart_device = device != NULL ? device : SERIAL_DEFAULT_M4_UART_DEVICE;

    return serial_open_devices(SERIAL_DEFAULT_Q6_BUTTON_DEVICE, m4_uart_device, baudrate);
}

int serial_open_devices(const char *q6_button_device, const char *m4_uart_device, int baudrate) {
    const char *q6_device = q6_button_device != NULL ? q6_button_device : SERIAL_DEFAULT_Q6_BUTTON_DEVICE;
    const char *m4_device = m4_uart_device != NULL ? m4_uart_device : SERIAL_DEFAULT_M4_UART_DEVICE;

    serial_close();

    q6_button_fd = open(q6_device, O_RDONLY);
    if (q6_button_fd < 0) {
        return -1;
    }

    m4_uart_fd = open(m4_device, O_RDWR);
    if (m4_uart_fd < 0) {
        serial_close();
        return -1;
    }

    if (configure_uart(m4_uart_fd, baudrate) != 0) {
        serial_close();
        return -1;
    }

    return 0;
}

void serial_close(void) {
    if (q6_button_fd >= 0) {
        close(q6_button_fd);
        q6_button_fd = -1;
    }

    if (m4_uart_fd >= 0) {
        close(m4_uart_fd);
        m4_uart_fd = -1;
    }
}

ssize_t serial_read(void *buf, size_t len) {
    if (m4_uart_fd < 0) {
        errno = EBADF;
        return -1;
    }

    return read(m4_uart_fd, buf, len);
}

ssize_t serial_write(const void *buf, size_t len) {
    if (m4_uart_fd < 0) {
        errno = EBADF;
        return -1;
    }

    return write(m4_uart_fd, buf, len);
}

int serial_next_event(GameEvent *event) {
    int max_fd;

    if (event == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (q6_button_fd < 0 && m4_uart_fd < 0) {
        errno = EBADF;
        return -1;
    }

    for (;;) {
        fd_set read_set;
        int ready;

        FD_ZERO(&read_set);
        max_fd = -1;

        if (q6_button_fd >= 0) {
            FD_SET(q6_button_fd, &read_set);
            if (q6_button_fd > max_fd) {
                max_fd = q6_button_fd;
            }
        }

        if (m4_uart_fd >= 0) {
            FD_SET(m4_uart_fd, &read_set);
            if (m4_uart_fd > max_fd) {
                max_fd = m4_uart_fd;
            }
        }

        ready = select(max_fd + 1, &read_set, NULL, NULL, NULL);

        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (q6_button_fd >= 0 && FD_ISSET(q6_button_fd, &read_set)) {
            int result = read_q6_event(event);
            if (result != 0) {
                return result;
            }
        }

        if (m4_uart_fd >= 0 && FD_ISSET(m4_uart_fd, &read_set)) {
            int result = read_m4_event(event);
            if (result != 0) {
                return result;
            }
        }
    }
}
