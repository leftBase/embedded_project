#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>
#include <stdint.h>

/* serial helper - 임베디드에서 테라텀/시리얼 통신을 사용할 경우
	장치 열기/읽기/쓰기/종료를 제공합니다. */
int serial_init(const char *device, int baudrate);
void serial_close(void);
ssize_t serial_read(void *buf, size_t len);
ssize_t serial_write(const void *buf, size_t len);

#endif // SERIAL_H
