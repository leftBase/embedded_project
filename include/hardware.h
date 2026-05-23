#ifndef HARDWARE_H
#define HARDWARE_H

#define SERIAL_DEFAULT_Q6_BUTTON_DEVICE "/dev/input/event1"

// Q6 버튼 이벤트 코드 (input_event.code)
#define Q6_KEY_BACK 158
#define Q6_KEY_HOME 172
#define Q6_KEY_MENU 139

// 시스템 시작/종료 시 한 번씩만 호출
int hw_init(void);
void hw_close(void);

// 장치 제어 함수들
void led_control(int led_pin, int state);
void buzzer_play(double freq, int time_us);
int button_read_event(int *code, int *value);

#endif