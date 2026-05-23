#include "event.h"
#include "debug.h"
#include "game.h"
#include "render.h"
#include "serial.h"
#include "hardware.h"

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
#define RENDER_INTERVAL_TICKS 1
#define FND_INTERVAL_TICKS 10

//터미오스 tio, 키보드 초기화 여부, 원래 터미오스 플래그 저장
static struct termios tio;
static int original_stdin_flags = -1;
static int keyboard_ready;
static int simulator_autotest;

//없어질 키보드 입력 초기화
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

#ifdef SIMULATOR
static void* autotest_thread(void* arg) {
    (void)arg;

    usleep(200000);
    queue_push(&g_queue, EV_START, 1);

    usleep(200000);
    queue_push(&g_queue, EV_P1_LEFT, 1);

    usleep(200000);
    queue_push(&g_queue, EV_P1_RIGHT, 1);

    usleep(200000);
    queue_push(&g_queue, EV_P1_SKILL, 1);

    usleep(200000);
    queue_push(&g_queue, EV_P2_LEFT, 1);

    usleep(200000);
    queue_push(&g_queue, EV_P2_RIGHT, 1);

    usleep(200000);
    queue_push(&g_queue, EV_P2_SKILL, 1);

    usleep(200000);
    queue_push(&g_queue, EV_PAUSE, 1);

    usleep(200000);
    queue_push(&g_queue, EV_PAUSE, 1);

    usleep(6000000);
    queue_push(&g_queue, EV_QUIT, 1);

    return NULL;
}
#endif


// ####타이머스레드####

//tick_ms는 50. 50ms에 한번(20fps) 틱푸시하는 주기 스레드
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

// 다음 GameEvent를 EventQueue에 푸시 serial_next_event가 m4의 입력을 감시한걸 사용
static void* serial_thread(void* arg) {
    (void)arg; // 인자 사용 안 함

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


// ####하드웨어스레드####

// 하드웨어(Q6) 전용 스레드: hw_next_event로부터 이벤트를 받아 큐로 푸시
static void* hw_thread(void* arg) {
    (void)arg; // 인자 사용 안 함

    while (running) {
        GameEvent event;
        int result = hw_next_event(&event);

        if (result > 0) {
            debug_log_event("hw_event", event);
            queue_push_event(&g_queue, event);
        } else if (result < 0) {
            DBG("hw_next_event failed errno=%d", errno);
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

    /* Only send board outputs when the game is running. This prevents the
     * simulator from flooding the terminal with FND/LCD updates after the
     * game is over or paused. */
    if (!serial_enabled || game->state != GAME_RUNNING) {
        return;
    }

    //1. lcd이 바뀌었으면 시리얼로 보내기. 실패하면 로그에 남기기.
    if (game->lcd != last_lcd) {
        if (serial_send_lcd(game->lcd) == 0) {
            last_lcd = game->lcd;
        } else {
            DBG("lcd send failed lcd=%d errno=%d", game->lcd, errno);
        }
    }

    //2. fnd는 플레이어1,2 점수 번갈아가면서 보내기. 10틱마다 보내도록. 실패하면 로그에 남기기.
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

    //3. buzzer, led 업데이트는 나중에...
}

// ####메인루프####

// 게임 초기화, 스레드 생성, 이벤트 처리 루프, 종료시 정리
int main(int argc, char **argv) {
    //chat **argv == char *argv[]: 명령행 인자 배열. argv[0]는 프로그램 이름, argv[1]부터는 실제 인자들. argc는 인자 개수.
    //타이머, 키보드, 시리얼, 하드웨어 스레드 생성. 시리얼, 하드웨어는 초기화 성공시에만.
    pthread_t keyboard;
    pthread_t serial;
    pthread_t hw;
    pthread_t timer;
    pthread_t autotest;
    int serial_enabled;
    int hw_enabled;
    int serial_thread_started = 0;
    int hw_thread_started = 0;
    int autotest_started = 0;
    int render_tick_count = 0;
    int need_render = 1;
    int i;

#ifdef SIMULATOR
    simulator_autotest = 1;
#else
    simulator_autotest = 0;
#endif

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--autotest") == 0) {
            simulator_autotest = 1;
        } else if (strcmp(argv[i], "--manual") == 0) {
            simulator_autotest = 0;
        }
    }

    //난수 시드, 디버그 초기화, 게임 초기화, 키보드 초기화, 렌더 초기화
    srand(simulator_autotest ? 1U : (unsigned int)time(NULL));
    debug_init(getenv("GAME_DEBUG"));
#ifdef SIMULATOR
    DBG("program start simulator=1 autotest=%d", simulator_autotest);
#else
    DBG("program start simulator=0 autotest=%d", simulator_autotest);
#endif

    //이벤트큐 초기화, 게임상태 초기화, 키보드 초기화, 렌더 초기화
    queue_init(&g_queue);
    game_init(&g_game);
    if (!simulator_autotest) {
        keyboard_init();
    }
    render_init();

    //타이머, 키보드 스레드 생성
    pthread_create(&timer, NULL, timer_thread, NULL);
    if (!simulator_autotest) {
        pthread_create(&keyboard, NULL, keyboard_thread, NULL);
    }

#ifdef SIMULATOR
    if (simulator_autotest) {
        pthread_create(&autotest, NULL, autotest_thread, NULL);
        autotest_started = 1;
        DBG("simulator autotest thread enabled");
    }
#endif

    // 초기화 성공시 하드웨어, 시리얼 스레드 생성
    hw_enabled = hw_init() == 0;
#ifndef SIMULATOR
    if (hw_enabled) {
        pthread_create(&hw, NULL, hw_thread, NULL);
        hw_thread_started = 1;
        DBG("hardware thread enabled");
    } else {
        DBG("hardware init failed or not present");
    }
#else
    if (hw_enabled) {
        DBG("hardware simulator enabled");
    } else {
        DBG("hardware simulator init failed");
    }
#endif

    serial_enabled = m4_uart_init() == 0;
#ifndef SIMULATOR
    if (serial_enabled) {
        pthread_create(&serial, NULL, serial_thread, NULL);
        serial_thread_started = 1;
        DBG("serial thread enabled");
    } else {
        DBG("serial init failed errno=%d", errno);
    }
#else
    if (serial_enabled) {
        DBG("serial simulator enabled");
    } else {
        DBG("serial simulator init failed errno=%d", errno);
    }
#endif

    //메인 루프: 이벤트 큐에서 이벤트 읽어서 게임에 적용. 틱 이벤트면 보드 업데이트, 렌더링 주기 체크해서 렌더링. 그 외 이벤트면 렌더링 필요 플래그 세움.
    while (running) {
        GameEvent ev = queue_pop(&g_queue);

        if (ev.type == EV_QUIT) {
            running = 0;
            break;
        }

        if (ev.type != EV_TICK) {
            debug_log_event("main_event", ev);
        }

        //EV_QUIT, EV_TICK 아니면 로그 쓰고 이벤트 적용.
        game_apply_event(&g_game, ev);

        //EV_TICK이면 보드 업데이트(fnd, lcd, led, buzzer)
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

// 1. 전역 러닝 플래그 내림
    running = 0;

    // 2. [추천 피드백] 큐 장부를 폐쇄하고 대기실에 갇힌 녀석들을 확 깨워 탈출시킴
    queue_close(&g_queue);

    // 3. 디바이스 파일 디스크립터들 먼저 닫기 (블로킹 I/O 강제 리턴 유도)
    serial_close();
    if (hw_enabled) hw_close();

    // 4. 모든 보조 쓰레드가 깔끔하게 정돈되어 죽을 때까지 대기 및 자원 수거
    pthread_join(timer, NULL);
    if (!simulator_autotest) {
        pthread_join(keyboard, NULL);
    }
#ifdef SIMULATOR
    if (autotest_started) {
        pthread_join(autotest, NULL);
    }
#else
    if (hw_thread_started) pthread_join(hw, NULL);
    if (serial_thread_started) pthread_join(serial, NULL);
#endif

    // 5. 최종 메모리 및 로그 백업 정리
    save_game_log(g_game.logs);
    render_shutdown();
    if (!simulator_autotest) {
        keyboard_close();
    }
    debug_close();

    return 0;
}