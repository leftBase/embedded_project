#include "hardware.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>


// 이 파일 안에서만 사용할 File Descriptor들 (은닉화)
static int buzzer_fd = -1;
static int button_fd = -1;
// rtc_fd 등 추가 가능...

// 1. 초기화 (모든 장치를 한 번에 연다) 
//부저, 버튼
//todo: led
int hw_init(void) {
    // 1-1. 부저 열기
    buzzer_fd = open("/dev/buzzer", O_RDWR);
    if (buzzer_fd < 0) {
        printf("Buzzer open failed!\n");
        return -1;
    }

    // 1-2. 버튼 열기
    button_fd = open("/dev/input/event1", O_RDONLY);
    if (button_fd < 0) {
        printf("Button open failed!\n");
        hw_close(); // 이미 열린 장치가 있을 수 있으므로 닫아줌
        return -1;
    }

    // 1-3. LED GPIO export 설정
    // gpio_export(LED_BACK); gpio_set_dir(LED_BACK, 1); 등...

    printf("Hardware Initialized.\n");
    return 0;
}

// 2. 종료 (모든 장치를 깔끔하게 닫는다)
//todo: led
void hw_close(void) {
    if (buzzer_fd >= 0) close(buzzer_fd);
    if (button_fd >= 0) close(button_fd);
    // LED unexport 로직 등 추가...
    printf("Hardware Closed.\n");
}

// 3. 부저 제어 래퍼 함수
void buzzer_play(double freq, int time_us) {
    if (buzzer_fd < 0) return;
    
    long tone = (long)((1.0f / freq) * 1000000000);
    ioctl(buzzer_fd, _IOW('b', 0x0b, int), tone); // SET_TONE
    ioctl(buzzer_fd, _IOW('b', 0x0c, int), 25000); // SET_VOLUME
    ioctl(buzzer_fd, _IOW('b', 0x07, int), 0); // START
    usleep(time_us);
    ioctl(buzzer_fd, _IOW('b', 0x09, int), 0); // END
}




// 내부) sys/input.h에 정의된 input_event 타입을 EventType으로 매핑해서 반환
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


//
int read_q6_event(int *code, int *value) {
    if (button_fd < 0) return -1;

    struct input_event ev;
    ssize_t n = read(button_fd, &ev, sizeof(struct input_event));
    if (n != sizeof(struct input_event)) {
        return -1; // 읽기 실패
    }

    if (ev.type != EV_KEY) {
        return -1; // 키 이벤트가 아님
    }

    *code = ev.code;
    *value = ev.value;
    return 0; // 성공
}

//q6버튼 이벤트 읽어서 EventType 매핑해 리턴
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