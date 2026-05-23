#include "serial.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

//패킷 프로토콜 - 시작 바이트(0x12), 명령어, 인자1, 인자2, 종료 바이트(0x13)
#define PACKET_START 0x12
#define PACKET_END 0x13
#define CMD_M4_BUTTON 0x22
#define CMD_FND_SET 0x23
#define CMD_LCD_SET 0x24

// fd 선언
static int m4_uart_fd = -1;

//버퍼 선언
char read_buf[5];

/* --- ACK 설정 및 스위치 --- 실은 필요없음. */
#define SERIAL_USE_ACK 0  // 0: 끄기 (렉 없음), 1: 켜기
#define CMD_ACK 0x7E
#define ACK_TIMEOUT_MS 50

#if SERIAL_USE_ACK

/* timeout(ms) 안에 5바이트 패킷 1개 읽기 (내부 도우미) */
static int read_packet_with_timeout(unsigned char packet[5], int timeout_ms) {
    fd_set rfds;
    struct timeval tv;
    int ready;
    ssize_t n;

    if (m4_uart_fd < 0) {
        errno = EBADF;
        return -1;
    }

    FD_ZERO(&rfds);
    FD_SET(m4_uart_fd, &rfds);

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    ready = select(m4_uart_fd + 1, &rfds, NULL, NULL, &tv);
    if (ready < 0) {
        return (errno == EINTR) ? 0 : -1;
    }
    if (ready == 0) {
        errno = ETIMEDOUT;
        return -1;
    }

    n = read(m4_uart_fd, packet, 5);
    if (n < 0) {
        return (errno == EINTR) ? 0 : -1;
    }
    if (n != 5) {
        return 0;
    }

    if (packet[0] != PACKET_START || packet[4] != PACKET_END) {
        return 0;
    }

    return 1;
}


/* ACK 패킷 검증 및 3회 재시도 함수 */
static int wait_ack_for_packet(unsigned char cmd, unsigned char arg1, unsigned char arg2) {
    unsigned char p[5];
    int retry;

    (void)arg2; /* 현재 예시는 arg1 기반 ACK 검사 */

    for (retry = 0; retry < 3; ++retry) {
        int r = read_packet_with_timeout(p, ACK_TIMEOUT_MS);
        if (r < 0) return -1;
        if (r == 0) continue;

        if (p[1] == CMD_ACK && p[2] == cmd && p[3] == (unsigned char)('0' + arg1)) {
            return 0;
        }
        /* ACK 외 패킷이면 무시하고 계속 대기 */
    }

    errno = ETIMEDOUT;
    return -1;
}

#endif


// 프로그램 오류 발생 시 디버깅 함수
int panic() {
    printf("[Panic] %i: %s\n", errno, strerror(errno));
    return -1;
}

// 1. 터미오스객체 tio로 uart통신을 설정함.
int m4_uart_init(void) {
    struct termios tio;
    bzero(&tio, sizeof(tio));

    memset(&read_buf, '\0', sizeof(read_buf));

    m4_uart_fd = open("/dev/ttymxc1", O_RDWR);
    if (m4_uart_fd == -1) {
        return panic();
    }

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
    if (tcsetattr(m4_uart_fd, TCSANOW, &tio) != 0) return panic();

    return 0;
}

// 내부) button_id, pressed, GameEvent*를 받아 이벤트타입으로 매핑해 넣고, 밸류 줌.
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

//m4 버튼 이벤트 버퍼에 읽어서 GameEvent로 매핑해서 리턴.
static int read_m4_event(GameEvent *event) {
    
    ssize_t n = read(m4_uart_fd, read_buf, sizeof(read_buf));
    int button_id;
    int pressed;

    if (n < 0) return 0;

    if (read_buf[1] != CMD_M4_BUTTON) return 0;

    if (read_buf[0] != PACKET_START || read_buf[4] != PACKET_END) {
        return 0;
    }

    button_id = read_buf[2] - 0x30; 
    // '0'의 ASCII 값을 빼서 숫자 ID로 변환
    pressed = read_buf[3] - 0x30;

    return map_m4_button_event(button_id, pressed, event);
}

// 통신 닫음.
void serial_close(void) {
    if (m4_uart_fd >= 0) {
        close(m4_uart_fd);
        m4_uart_fd = -1;
    }
}

//내부) 통신에 씀.
//버퍼(패킷)랑 길이주면 uart fd가 열려있으면 커널이 전역변수fd도 가지고 씀.
ssize_t serial_write(const void *buf, size_t len) {
    if (m4_uart_fd < 0) {
        errno = EBADF;
        return -1;
    }

    return write(m4_uart_fd, buf, len);
}

/* 내부) ->serial_send_lcd(), serial_send_fnd_digit(), serial_send_fnd_number()
패킷만들어서 통신에 씀.
커맨드, 인자, 인자 채우면 패킷만듦. 패킷과 패킷길이를 위 함수에 호출하며 인자로 넘겨 씀. 패킷이 제대로 길이맞춰 쓰이면 0 반환.
lcd) 0x24, 0x30, 0x30~33(0~3프리셋)
fnd) 0x23, 0x30~32(자리), 0x30~39(0~9)
led) 0x21, 0x30~34(LED 번호), 0x30~31(0,1)
*/
static int write_packet(unsigned char command, unsigned char arg1, unsigned char arg2) {
    unsigned char packet[5];
    ssize_t n;

    packet[0] = PACKET_START;
    packet[1] = command;
    packet[2] = arg1 + 0x30;
    packet[3] = arg2 + 0x30;
    packet[4] = PACKET_END;

    n = serial_write(packet, sizeof(packet));
    return n == (ssize_t)sizeof(packet) ? 0 : -1;

#if SERIAL_USE_ACK
    return wait_ack_for_packet(command, arg1, arg2);
#else
    return 0;
#endif
}

//2. lcd에 프리셋 입력해서 패킷 통신함.
int serial_send_lcd(int preset) {
    if (preset < 0) {
        preset = 0;
    }

    if (preset > 3) {
        preset = 3;
    }

    return write_packet(CMD_LCD_SET, 0, preset);
}

//내부) fnd에 자리, 숫자 받아서ㅓ 패킷만들어서 통신함.
int serial_send_fnd_digit(int position, int value) {
    if (position < 0) {
        position = 0;
    }

    if (position > 2) {
        position = 2;
    }

    if (value < 0) {
        value = 0;
    }

    if (value > 9) {
        value = 9;
    }

    return write_packet(CMD_FND_SET, position, value);
}

// 3. 숫자를 fnd 보내는 형식으로 만들어서 통신.
int serial_send_fnd_number(int number) {
    int hundreds;
    int tens;
    int ones;

    if (number < 0) {
        number = 0;
    }

    number %= 1000;
    hundreds = number / 100;
    tens = (number / 10) % 10;
    ones = number % 10;

    if (serial_send_fnd_digit(0, hundreds) != 0) {
        return -1;
    }

    if (serial_send_fnd_digit(1, tens) != 0) {
        return -1;
    }

    return serial_send_fnd_digit(2, ones);
}


/* 4. 이벤트 큐에서 다음 이벤트를 읽어서 GameEvent 리턴. 

posix api 사용하는 select 시스템 콜. 여기서는 <sys/select.h>를 include 해서 사용.

        fd_set: 파일 디스크립터 집합 타입
        select(): 파일 디스크립터 집합에서 읽기, 쓰기, 예외 발생 여부를 감시하는 시스템 콜
        FD_ZERO(), FD_SET(), FD_ISSET() 등의 매크로를 사용하여 fd_set을 조작
            FD_ZERO(&read_set): read_set을 초기화하여 모든 파일 디스크립터를 제거
            FD_SET(fd, &read_set): fd를 read_set에 추가
            FD_ISSET(fd, &read_set): fd가 read_set에 있는지 확인 (즉, 해당 fd에서 이벤트가 발생했는지 확인)
*/
int serial_next_event(GameEvent *event) {
    int max_fd;

    if (event == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (m4_uart_fd < 0) {
        errno = EBADF;
        return -1;
    }

    

    for (;;) {
        fd_set read_set;
        int ready;

        FD_ZERO(&read_set);
        max_fd = -1;

            
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

        if (m4_uart_fd >= 0 && FD_ISSET(m4_uart_fd, &read_set)) {
            int result = read_m4_event(event);
            if (result != 0) {
                return result;
            }
        }
    }
}
