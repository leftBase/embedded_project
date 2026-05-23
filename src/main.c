#include "event.h"
#include "debug.h"
#include "game.h"
#include "render.h"
#include "serial.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

//이벤트큐, 게임상태 은닉화.
static EventQueue g_queue;
static GameState g_game;
static volatile int running = 1;

//GUI, fnd 틱 10
#define RENDER_INTERVAL_TICKS 10
#define FND_INTERVAL_TICKS 10

//터미오스 tio, 키보드 초기화 여부, 원래 터미오스 플래그 저장
static struct termios tio;
static int original_stdin_flags = -1;
static int keyboard_ready;

//없어질 키보드 입력
static void keyboard_init(void) {
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &tio) != 0) {
        DBG("tcgetattr failed errno=%d", errno);
        return;
    }

    // 원래 터미오스 설정을 raw 모드로 변경하여 키보드 입력을 비차단(non-blocking)으로 읽을 수 있도록 설정합니다. 이렇게 하면 getchar()나 read()가 키 입력이 없을 때 블록되지 않고 즉시 반환됩니다.
    raw = tio;
    raw.c_iflag &= ~(ICRNL | IXON);
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        DBG("tcsetattr failed errno=%d", errno);
        return;
    }

    tcflush(STDIN_FILENO, TCIFLUSH);

    //fcntl 함수를 사용하여 표준 입력(STDIN_FILENO)의 파일 상태 플래그를 가져오고, O_NONBLOCK 플래그를 추가하여 키보드 입력을 비차단 모드로 설정합니다. 이렇게 하면 키보드에서 입력이 없을 때 read() 함수가 즉시 반환되어 프로그램이 멈추지 않고 계속 실행될 수 있습니다.
    original_stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (original_stdin_flags >= 0) {
        fcntl(STDIN_FILENO, F_SETFL, original_stdin_flags | O_NONBLOCK);
    }

    keyboard_ready = 1;
}

//키보드 입력 원상복구
static void keyboard_close(void) {
    if (!keyboard_ready) {
        return;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &tio);

    if (original_stdin_flags >= 0) {
        fcntl(STDIN_FILENO, F_SETFL, original_stdin_flags);
    }
}


// ####타이머스레드####

//tick ms는 50. 50ms에 한번(20fps) 틱푸시하는
static void* timer_thread(void* arg) {
    (void)arg;

    while (running) {
        usleep(TICK_MS * 1000); 
        queue_push(&g_queue, EV_TICK, 0);
    }

    return NULL;
}


// ####키보드스레드####

//a, d, q는 p1의 좌우 스킬
//s는 스타트, p는 일시정지
//j, l, o는 p2의 좌우 스킬, x는 종료
static void* keyboard_thread(void* arg) {
    (void)arg;

    while (running) {
        unsigned char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);

        if (n <= 0) {
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                DBG("keyboard read error errno=%d", errno);
            }
            usleep(5000);
            continue;
        }

        switch (c) {
            case 'a':
                queue_push(&g_queue, EV_P1_LEFT, 1);
                break;
            case 'd':
                queue_push(&g_queue, EV_P1_RIGHT, 1);
                break;
            case 'q':
                queue_push(&g_queue, EV_P1_SKILL, 1);
                break;
            case 's':
                queue_push(&g_queue, EV_START, 1);
                break;
            case 'p':
                queue_push(&g_queue, EV_PAUSE, 1);
                break;
            case 'j':
                queue_push(&g_queue, EV_P2_LEFT, 1);
                break;
            case 'l':
                queue_push(&g_queue, EV_P2_RIGHT, 1);
                break;
            case 'o':
                queue_push(&g_queue, EV_P2_SKILL, 1);
                break;
            case 'x':
            case 'X':
                queue_push(&g_queue, EV_QUIT, 1);
                break;
            default:
                break;
        }
    }

    return NULL;
}


// ####시리얼스레드####

// 다음 GameEvent를 EventQueue에 푸시
//serial_next_event가 q6, m4의 입력을 감시한걸 사용
static void* serial_thread(void* arg) {
    (void)arg;

    while (running) {
        GameEvent event;
        int result = serial_next_event(&event);

        if (result > 0) {
            debug_log_event("serial_event", event);
            queue_push_event(&g_queue, event);
        } else if (result < 0) {
            DBG("serial_next_event failed errno=%d", errno);
            usleep(20000);
        }
    }

    return NULL;
}


//fnd, lcd 업데이트
//근데 이거 전에 안됐던거 같음
//todo: buzzer, led업데이트
static void update_board_outputs(int serial_enabled, const GameState *game) {
    static int last_lcd = -1;
    static int fnd_ticks;
    static int fnd_player;

    if (!serial_enabled) {
        return;
    }

    if (game->lcd != last_lcd) {
        if (serial_send_lcd(game->lcd) == 0) {
            last_lcd = game->lcd;
        } else {
            DBG("lcd send failed lcd=%d errno=%d", game->lcd, errno);
        }
    }

    fnd_ticks++;
    if (fnd_ticks < FND_INTERVAL_TICKS) {
        return;
    }

    fnd_ticks = 0;
    fnd_player = 1 - fnd_player;

    if (serial_send_fnd_number(game->players[fnd_player].score) != 0) {
        DBG("fnd send failed p=%d score=%d errno=%d",
            fnd_player + 1,
            game->players[fnd_player].score,
            errno);
    }
}


int main(int argc, char **argv) {
    pthread_t timer;
    pthread_t keyboard;
    pthread_t serial;
    int serial_enabled;
    int render_tick_count = 0;
    int need_render = 1;
    int use_serial = argc > 1 && strcmp(argv[1], "--serial") == 0;

    srand((unsigned int)time(NULL));
    debug_init(getenv("GAME_DEBUG"));
    DBG("program start use_serial=%d", use_serial);

    queue_init(&g_queue);
    game_init(&g_game);
    keyboard_init();
    render_init();

    pthread_create(&timer, NULL, timer_thread, NULL);
    pthread_create(&keyboard, NULL, keyboard_thread, NULL);

    serial_enabled = use_serial && serial_init(NULL, 115200) == 0;
    if (serial_enabled) {
        pthread_create(&serial, NULL, serial_thread, NULL);
        DBG("serial thread enabled");
    } else if (use_serial) {
        DBG("serial init failed errno=%d", errno);
    }

    while (running) {
        GameEvent ev = queue_pop(&g_queue);

        if (ev.type == EV_QUIT) {
            running = 0;
            break;
        }

        if (ev.type != EV_TICK) {
            debug_log_event("main_event", ev);
        }

        game_apply_event(&g_game, ev);

        if (ev.type == EV_TICK) {
            update_board_outputs(serial_enabled, &g_game);
            render_tick_count++;
            if (render_tick_count >= RENDER_INTERVAL_TICKS) {
                render_tick_count = 0;
                if (need_render || g_game.state == GAME_RUNNING) {
                    render_game(&g_game);
                    need_render = 0;
                }
            }
        } else {
            need_render = 1;
        }
    }

    save_game_log(g_game.logs);
    serial_close();
    render_shutdown();
    keyboard_close();
    debug_close();

    return 0;
}
