#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>
#include <sys/types.h>
#include "event.h"

/* --- 초기화 및 해제 --- */
// 매개변수가 없음을 명확히 하기 위해 void 추가
int m4_uart_init(void);
void serial_close(void);

/* --- 로컬 및 통신 I/O --- */
// 구현 파일(.c)의 스펙과 일치하도록 정리 (현재 .c에서 static이 아니거나 노출된 인터페이스용)
ssize_t serial_write(const void *buf, size_t len);

/* --- 이벤트 제어 및 수신 --- */
// 하드웨어 신호를 감시하여 GameEvent를 채워 반환하는 핵심 루프
int serial_next_event(GameEvent *event);

/* --- M4 보드 출력 제어 (TX 패킷 송신) --- */
int serial_send_lcd(int preset);
int serial_send_fnd_digit(int position, int value);
int serial_send_fnd_number(int number);
int serial_send_led(int led_index, int state);

#endif // SERIAL_H
