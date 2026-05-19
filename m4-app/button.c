// 표준 입출력 라이브러리
#include <stdio.h>
// 문자열 처리 라이브러리
#include <string.h>
// 파일 제어 라이브러리
#include <fcntl.h>
// 에러 처리 라이브러리
#include <errno.h>
// 시리얼 설정 라이브러리
#include <termios.h>
// POSIX 함수 라이브러리
#include <unistd.h>

// 시리얼 장치 파일 디스크립터 변수 선언
static int serial_fd = -1;

// 프로그램 오류 발생 시 디버깅 함수
int panic() {
    printf("[Panic] %i: %s\n", errno, strerror(errno));
    return 1;
}

int main(void) {
    // 시리얼 설정 관련 구조체 선언
    struct termios tio;

    // 시리얼 장치 열기
    serial_fd = open("/dev/ttymxc1", O_RDWR);
    if (serial_fd == -1) return panic();

    // termios 구조체 초기화
    bzero(&tio, sizeof(tio));
    
    // 통신 속도 115200, RTS/CTS 활성화, 8비트, 모뎀 제어 라인 무시, 수신 활성화
    tio.c_cflag = B115200 | CRTSCTS | CS8 | CLOCAL | CREAD;
    // 프레임, 페리티 에러 무시
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    // 타임 아웃 설정 (데이터가 수신될 때 까지 대기)
    tio.c_cc[VTIME] = 0;
    // 최소 읽기 바이트 수
    tio.c_cc[VMIN] = 5;

    // 시리얼 통신 설정
    if (tcsetattr(serial_fd, TCSANOW, &tio) != 0) return panic();

    // 데이터 수신 버퍼 선언 및 초기화
    char read_buf[5];
    memset(&read_buf, '\0', sizeof(read_buf));

    // 무한 루프 (데이터를 지속적으로 수신하기 위해 사용)
    while (1) {
        // 데이터 수신
        int num_bytes = read(serial_fd, &read_buf, sizeof(read_buf));
        if (num_bytes < 0) continue;

        if (read_buf[1] != 0x22) continue;
        printf("Button [%d] click! [%d]\n", read_buf[2] - 0x30, read_buf[3] - 0x30);
    }

    // 시리얼 장치 닫기
    close(serial_fd);
    return 0;
}