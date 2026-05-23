#ifndef HARDWARE_H
#define HARDWARE_H

#include "event.h"

/* --- Q6 보드 고유 하드웨어 매크로 상수 선언 --- */
// 입력 이벤트 코드 매핑 (linux/input.h 시스템 규격 호환)
#define Q6_KEY_BACK    158
#define Q6_KEY_HOME    172
#define Q6_KEY_MENU    139

// GPIO 제어 가독성을 위한 기본 상태 매크로
#define HW_LED_OFF     0
#define HW_LED_ON      1

/* --- 로컬 시스템 생명주기 (Lifecycle) --- */
// 시스템 시작 및 종료 시 각각 최초/최종 1회씩만 호출하여 전역 FD 자원 관리
int hw_init(void);
void hw_close(void);

/* --- 비동기 입력 이벤트 채널 --- */
// Q6 로컬 입력 디바이스들을 select/read 하여 GameEvent 큐 패킷으로 변환
// @return: 1 = 유효 이벤트 성공, 0 = 무효 신호(인터럽트/뗀 키 무시), -1 = 시스템 오류
int hw_next_event(GameEvent *event);

/* --- 로컬 하드웨어 컴포넌트 직접 제어 (디바이스 래퍼) --- */
void led_control(int led_pin, int state);
void buzzer_play(double freq, int time_us);

// 내부 로우레벨 입력 테스트나 특수 디버깅용 인터페이스
int button_read_event(int *code, int *value);

#endif // HARDWARE_H