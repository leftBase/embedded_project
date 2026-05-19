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
// C 표준 유틸리티 함수 라이브러리
#include <stdlib.h>

// 시리얼 장치 파일 디스크립터 변수 선언
static int serial_fd = -1;

// 프로그램 오류 발생 시 디버깅 함수
int panic() {
    printf("[Panic] %i: %s\n", errno, strerror(errno));
    return 1;
}

int main(int argc, char* argv[]) {
    // 시리얼 설정 관련 구조체 선언
    struct termios tio;

    if (argc != 3) {
        errno = EINVAL;
        return panic();
    }

    // 시리얼 장치 열기
    serial_fd = open("/dev/ttymxc1", O_RDWR);
    if (serial_fd == -1) return panic();

    // termios 구조체 초기화
    bzero(&tio, sizeof(tio));
    
    // 통신 속도 115200, RTS/CTS 활성화, 8비트, 모뎀 제어 라인 무시
    tio.c_cflag = B115200 | CRTSCTS | CS8 | CLOCAL;
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

    // 데이터 송신
    unsigned char msg[] = { 0x12, 0x23, 0x30 + atoi(argv[1]), 0x30 + atoi(argv[2]), 0x13 };
    write(serial_fd, msg, sizeof(msg));

    // 시리얼 장치 닫기
    close(serial_fd);
    return 0;
}