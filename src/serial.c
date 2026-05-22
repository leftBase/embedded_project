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

// Q6 버튼 이벤트 코드 (input_event.code)
#define Q6_KEY_BACK 158
#define Q6_KEY_HOME 172
#define Q6_KEY_MENU 139

// fd 선언
static int q6_button_fd = -1;
static int m4_uart_fd = -1;

//보드레이트를 스피드로 바꾸는 함수. 타입은 speed_t
static speed_t baud_to_speed(int baudrate) {
    switch (baudrate) {
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
        default:
            return B115200;
    }
}

//fd, 보드레이트 넣으면 터미오스객체 tio로 uart통신을 설정함.
static int configure_uart(int fd, int baudrate) {
    struct termios tio;

    memset(&tio, 0, sizeof(tio));

    tio.c_cflag = baud_to_speed(baudrate) | CS8 | CLOCAL | CREAD;
#ifdef CRTSCTS
    tio.c_cflag |= CRTSCTS;
#endif
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VTIME] = 0;
    tio.c_cc[VMIN] = 5;

    return tcsetattr(fd, TCSANOW, &tio);
}

//언사인드 바이트를 넣으면 0-9는 숫자로, 그 외는 그대로 반환하는 함수. M4에서 버튼 이벤트를 보낼 때 숫자는 '0'-'9'로 보내지만, 그 외의 값은 그대로 보내므로 이를 처리하기 위한 함수입니다. 이게 무슨의미? M4에서 버튼 이벤트를 보낼 때, 버튼 ID와 눌림 상태를 '0'-'9'의 ASCII 문자로 보내는 경우가 있습니다. 예를 들어, 버튼 ID 1이 눌렸을 때 '1'이라는 문자를 보내고, 버튼 ID 2가 눌렸을 때 '2'라는 문자를 보낼 수 있습니다. 이 함수는 이러한 ASCII 문자들을 실제 숫자 값으로 변환하기 위해 사용됩니다. 만약 입력된 바이트가 '0'에서 '9' 사이의 ASCII 문자라면, 해당 문자를 숫자로 변환하여 반환합니다. 그렇지 않은 경우에는 입력된 바이트를 그대로 반환합니다.
static int decode_digit_or_raw(unsigned char byte) {
    if (byte >= '0' && byte <= '9') {
        return byte - '0';
    }

    return byte;
}

//인풋이 있으면 이벤트 타입으로 바꾸는 함수.
//input_event 구조체랑 GameEvent 구조체를 받아서 event=1,  item_type=ITEM_NONE으로 초기화한 후, GameEvent의 EventType enum을 매핑. 
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

//인풋이 있으면 이벤트 타입으로 바꾸는 함수
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

//q6 버튼 이벤트 버퍼에 읽어서 event type매핑까지 갈겨 리턴하는 함수.
//패킷똑바로 들어오면(이거 예제랑 다른데? 뭐가나은지 확인해봄. 그리고 부저도 해야할거임) 현재 GameEvent 구조체랑 인풋때 생성한 input_event를 입력-이벤트타입 매핑하는 함수의 인자로 줌
static int read_q6_event(GameEvent *event) {
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

//m4  버튼 이벤트 버퍼에 읽어서 event type매핑까지 갈겨 리턴하는 함수.
//패킷똑바로 들어오면(이거 예제랑 다른데? 뭐가나은지 확인해봄. 그리고 fnd lcd led도 해야할거임) 현재 GameEvent 구조체랑 인풋때 생성한 패킷 배열에서  인덱스 변수로몇개 빼서 입력-이벤트타입 매핑하는 함수의 인자로 줌
static int read_m4_event(GameEvent *event) {
    unsigned char packet[5];
    ssize_t n = read(m4_uart_fd, packet, sizeof(packet));
    int button_id;
    int pressed;

    if (n < 0) {
        return errno == EINTR ? 0 : -1;
    }

    if (n != sizeof(packet)) {
        return 0;
    }

    if (packet[0] != PACKET_START || packet[4] != PACKET_END) {
        return 0;
    }

    if (packet[1] != CMD_M4_BUTTON) {
        return 0;
    }

    button_id = decode_digit_or_raw(packet[2]);
    pressed = decode_digit_or_raw(packet[3]);

    return map_m4_button_event(button_id, pressed, event);
}

//실제 통신초기화해서 통신열기 하는 함수
//fd랑 보드레이트 받아서 fd가 null이면 m4 uart 통신하는 fd를 디폴트를 주고 아니면 걍 받은 fd를줌. 그리고 uart fd랑 보드레이트랑 q6 버튼 디바이스 디폴트를 인자로 줘서 시리얼 오픈 함수에 넣어서 통신시작
// 근데 q6는 부저도 열어야됨 m4는 fnd lcd led도 열어야됨
int serial_init(const char *device, int baudrate) {
    const char *m4_uart_device = device != NULL ? device : SERIAL_DEFAULT_M4_UART_DEVICE;

    return serial_open_devices(SERIAL_DEFAULT_Q6_BUTTON_DEVICE, m4_uart_device, baudrate);
}

//통신열기 함수. 초기화해서 호출함.
//이딴식으로 포인터로 fd를 받으면 메모리줄줄안새나? codex가 한짓임. 
//q6버튼, m4 uart, 보드레이트넣으면 버튼은 읽기전용, uart는 읽기쓰기로 열음. 
int serial_open_devices(const char *q6_button_device, const char *m4_uart_device, int baudrate) {
    const char *q6_device = q6_button_device != NULL ? q6_button_device : SERIAL_DEFAULT_Q6_BUTTON_DEVICE;
    const char *m4_device = m4_uart_device != NULL ? m4_uart_device : SERIAL_DEFAULT_M4_UART_DEVICE;

    serial_close(); //먼저 기존에 열려있는 디바이스가 있으면 닫음

    q6_button_fd = open(q6_device, O_RDONLY);
    if (q6_button_fd < 0) {
        return -1;
    }

    m4_uart_fd = open(m4_device, O_RDWR);
    if (m4_uart_fd < 0) {
        serial_close(); // 실패하면 q6 버튼 디바이스는 이미 열렸으므로 닫아줌
        return -1;
    }

    if (configure_uart(m4_uart_fd, baudrate) != 0) {
        serial_close(); // UART 설정에 실패하면 열린 디바이스들을 닫아줌
        return -1;
    }

    return 0;
}

// 통신 닫음.
// fd가 0이상이면 닫고 초기화
void serial_close(void) {
    if (q6_button_fd >= 0) {
        close(q6_button_fd);
        q6_button_fd = -1;
    }

    if (m4_uart_fd >= 0) {
        close(m4_uart_fd);
        m4_uart_fd = -1;
    }
}

//통신 읽음.
//ssize_t는 바이트수 타입. 버퍼랑 길이를 주면 uart fd가 열려있으면 커널이 읽어서 반환. 
ssize_t serial_read(void *buf, size_t len) {
    if (m4_uart_fd < 0) {
        errno = EBADF;
        return -1;
    }

    return read(m4_uart_fd, buf, len);
}

//통신에 씀.
//버퍼(패킷)랑 길이주면 uart fd가 열려있으면 커널이 전역변수fd도 가지고 씀.
ssize_t serial_write(const void *buf, size_t len) {
    if (m4_uart_fd < 0) {
        errno = EBADF;
        return -1;
    }

    return write(m4_uart_fd, buf, len);
}

//패킷에 씀.
//패킷과 패킷길이를 위 함수에 호출하며 인자로 넘겨 씀. 패킷이 제대로 길이맞춰 쓰이면 0 반환.
static int write_packet(unsigned char command, unsigned char arg1, unsigned char arg2) {
    unsigned char packet[5];
    ssize_t n;

    packet[0] = PACKET_START;
    packet[1] = command;
    packet[2] = arg1;
    packet[3] = arg2;
    packet[4] = PACKET_END;

    n = serial_write(packet, sizeof(packet));
    return n == (ssize_t)sizeof(packet) ? 0 : -1;
}

int serial_send_lcd(int preset) {
    if (preset < 0) {
        preset = 0;
    }

    if (preset > 3) {
        preset = 3;
    }

    return write_packet(CMD_LCD_SET, '0', (unsigned char)('0' + preset));
}

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

    return write_packet(CMD_FND_SET, (unsigned char)('0' + position), (unsigned char)('0' + value));
}

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

int serial_next_event(GameEvent *event) {
    int max_fd;

    if (event == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (q6_button_fd < 0 && m4_uart_fd < 0) {
        errno = EBADF;
        return -1;
    }

    for (;;) {
        fd_set read_set;
        int ready;

        FD_ZERO(&read_set);
        max_fd = -1;

        if (q6_button_fd >= 0) {
            FD_SET(q6_button_fd, &read_set);
            if (q6_button_fd > max_fd) {
                max_fd = q6_button_fd;
            }
        }

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

        if (q6_button_fd >= 0 && FD_ISSET(q6_button_fd, &read_set)) {
            int result = read_q6_event(event);
            if (result != 0) {
                return result;
            }
        }

        if (m4_uart_fd >= 0 && FD_ISSET(m4_uart_fd, &read_set)) {
            int result = read_m4_event(event);
            if (result != 0) {
                return result;
            }
        }
    }
}
