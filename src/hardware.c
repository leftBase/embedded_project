#include "hardware.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

// 이 파일 안에서만 사용할 File Descriptor들 (은닉화)
static int buzzer_fd = -1;
static int button_fd = -1;
// rtc_fd 등 추가 가능...

// 1. 초기화 (모든 장치를 한 번에 연다)
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
        return -1;
    }

    // 1-3. LED GPIO export 설정 (아까 네 코드 활용)
    // gpio_export(LED_BACK); gpio_set_dir(LED_BACK, 1); 등...

    printf("Hardware Initialized.\n");
    return 0;
}

// 2. 종료 (모든 장치를 깔끔하게 닫는다)
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