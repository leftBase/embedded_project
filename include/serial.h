#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "event.h"

#define SERIAL_DEFAULT_Q6_BUTTON_DEVICE "/dev/input/event1"
#define SERIAL_DEFAULT_M4_UART_DEVICE "/dev/ttymxc1"

int serial_init(const char *device, int baudrate);
int serial_open_devices(const char *q6_button_device, const char *m4_uart_device, int baudrate);
void serial_close(void);
ssize_t serial_read(void *buf, size_t len);
ssize_t serial_write(const void *buf, size_t len);
int serial_next_event(GameEvent *event);
int serial_send_lcd(int preset);
int serial_send_fnd_digit(int position, int value);
int serial_send_fnd_number(int number);

#endif // SERIAL_H
