#include "event.h"
#include "debug.h"
#include "game.h"
#include "render.h"
#include "serial.h"
#include "hardware.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

//이벤트큐, 게임상태 은닉화.
static EventQueue g_queue;
static GameState g_game;
static volatile int running = 1;

//GUI, fnd 틱 10
#define RENDER_INTERVAL_TICKS 1
#define FND_INTERVAL_TICKS 10

//터미오스/키보드 관련 코드는 제거했습니다. 모든 입력은 하드웨어(serial/hw) 또는
//시뮬레이터 autotest에서만 수신되도록 변경되어 있습니다.

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


// buzzer, fnd, lcd 업데이트. M4/Q6 입력 LED는 입력 수신 위치에서 처리한다.
static void update_board_outputs(int serial_enabled, const GameState *game) {
    static int last_lcd = -1;
    static int last_sound_seq;
    static int fnd_ticks;
    static int fnd_player;

    if (game->sound_seq != last_sound_seq) {
        last_sound_seq = game->sound_seq;

        switch (game->sound) {
            case SOUND_CRASH:
                buzzer_play(196.0, 70000);
                break;
            case SOUND_ITEM:
                buzzer_play(659.0, 60000);
                break;
            case SOUND_ATTACK:
                buzzer_play(523.0, 60000);
                break;
            case SOUND_HEAL:
                buzzer_play(784.0, 70000);
                break;
            case SOUND_CLEAR:
                buzzer_play(880.0, 70000);
                break;
            case SOUND_GAME_OVER:
                buzzer_play(262.0, 120000);
                break;
            case SOUND_NONE:
            default:
                break;
        }
    }

    if (!serial_enabled) {
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

    if (game->state != GAME_RUNNING) {
        return;
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

    // M4 입력 LED는 serial read 쪽에서 버튼 press/release에 맞춰 바로 갱신한다.
}


// ####메인루프####

// 게임 초기화, 스레드 생성, 이벤트 처리 루프, 종료시 정리
int main(int argc, char **argv) {
    // 타이머, 시리얼, 하드웨어 스레드 생성. 시리얼, 하드웨어는 초기화 성공시에만.
    pthread_t serial;
    pthread_t hw;
    pthread_t timer;
#ifdef SIMULATOR
    pthread_t autotest;
#endif
    int serial_enabled;
    int hw_enabled;
#ifdef SIMULATOR
    int autotest_started = 0;
#endif
    int render_tick_count = 0;
    int need_render = 1;

#ifdef SIMULATOR
    simulator_autotest = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--autotest") == 0) {
            simulator_autotest = 1;
        } else if (strcmp(argv[i], "--manual") == 0) {
            simulator_autotest = 0;
        }
    }
#else
    simulator_autotest = 0;
    (void)argc;
    (void)argv;
#endif

    //난수 시드, 디버그 초기화, 게임 초기화, 렌더 초기화
    srand((unsigned int)time(NULL));
    debug_init(getenv("GAME_DEBUG"));
#ifdef SIMULATOR
    DBG("program start simulator=1 autotest=%d", simulator_autotest);
#else
    DBG("program start simulator=0 autotest=%d", simulator_autotest);
#endif

    //이벤트큐 초기화, 게임상태 초기화, 렌더 초기화
    queue_init(&g_queue);
    game_init(&g_game);
    render_init();

    //타이머 스레드 생성
    pthread_create(&timer, NULL, timer_thread, NULL);

    // 초기화 성공시 하드웨어, 시리얼 스레드 생성
    hw_enabled = hw_init() == 0;
#ifndef SIMULATOR
    if (hw_enabled) {
        pthread_create(&hw, NULL, hw_thread, NULL);
        DBG("hardware thread enabled");
    } else {
        DBG("hardware init failed or not present");
    }
#else
    if (hw_enabled) {
        pthread_create(&hw, NULL, hw_thread, NULL);
        DBG("hardware simulator thread enabled");
    } else {
        DBG("hardware simulator init failed");
    }
#endif

    serial_enabled = m4_uart_init() == 0;
#ifndef SIMULATOR
    if (serial_enabled) {
        pthread_create(&serial, NULL, serial_thread, NULL);
        DBG("serial thread enabled");
    } else {
        DBG("serial init failed errno=%d", errno);
    }
#else
    if (serial_enabled) {
        pthread_create(&serial, NULL, serial_thread, NULL);
        DBG("serial simulator thread enabled");
    } else {
        DBG("serial simulator init failed errno=%d", errno);
    }
#endif

#ifdef SIMULATOR
    if (simulator_autotest) {
        pthread_create(&autotest, NULL, autotest_thread, NULL);
        autotest_started = 1;
        DBG("simulator autotest thread enabled");
    }
#endif

    //메인 루프: 이벤트 큐에서 이벤트 읽어서 게임에 적용. 틱 이벤트면 보드 업데이트, 렌더링 주기 체크해서 렌더링. 그 외 이벤트면 렌더링 필요 플래그 세움.
    while (running) {
        GameEvent ev = queue_pop(&g_queue);

        if (ev.type == EV_QUIT) {
            running = 0;
            break;
        }

        if (ev.type == EV_PAUSE && g_game.state == GAME_OVER) {
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

    // 2. 큐 장부를 폐쇄
    queue_close(&g_queue);

    // 3. 디바이스 파일 디스크립터들 먼저 닫기 (블로킹 I/O 강제 리턴 유도)
    serial_close();
    if (hw_enabled) hw_close();

    // 4. 모든 보조 쓰레드가 깔끔하게 정돈되어 죽을 때까지 대기 및 자원 수거
    pthread_join(timer, NULL);
#ifdef SIMULATOR
    if (autotest_started) {
        pthread_join(autotest, NULL);
    }
#endif
    if (hw_enabled) pthread_join(hw, NULL);
    if (serial_enabled) pthread_join(serial, NULL);

    // 5. 최종 메모리 및 로그 백업 정리
    save_game_log(g_game.logs);
    render_shutdown();
    debug_close();

    return 0;
}
