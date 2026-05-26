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
static int simulator_autotest = 0; // 시뮬레이터에서 autotest 모드인지 여부. --autotest 커맨드라인 옵션으로 제어.

//GUI, fnd 틱 10
#define RENDER_INTERVAL_TICKS 4
#define FND_INTERVAL_TICKS 10
#define SOUND_QUEUE_SIZE 8

typedef struct {
    double freq;
    int time_us;
} SoundRequest;

typedef struct {
    SoundRequest buffer[SOUND_QUEUE_SIZE];
    int head;
    int tail;
    int closed;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} SoundQueue;

static SoundQueue g_sound_queue;

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

static int sound_queue_next_index(int index) {
    return (index + 1) % SOUND_QUEUE_SIZE;
}

static int sound_queue_is_empty(const SoundQueue *q) {
    return q->head == q->tail;
}

static int sound_queue_is_full(const SoundQueue *q) {
    return sound_queue_next_index(q->tail) == q->head;
}

static void sound_queue_init(SoundQueue *q) {
    q->head = 0;
    q->tail = 0;
    q->closed = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

static void sound_queue_close(SoundQueue *q) {
    pthread_mutex_lock(&q->lock);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

static void sound_queue_destroy(SoundQueue *q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
}

static void sound_queue_push(SoundQueue *q, double freq, int time_us) {
    pthread_mutex_lock(&q->lock);

    if (!q->closed) {
        if (sound_queue_is_full(q)) {
            q->head = sound_queue_next_index(q->head);
        }

        q->buffer[q->tail].freq = freq;
        q->buffer[q->tail].time_us = time_us;
        q->tail = sound_queue_next_index(q->tail);
        pthread_cond_signal(&q->not_empty);
    }

    pthread_mutex_unlock(&q->lock);
}

static int sound_queue_pop(SoundQueue *q, SoundRequest *request) {
    pthread_mutex_lock(&q->lock);

    while (sound_queue_is_empty(q) && !q->closed) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    if (sound_queue_is_empty(q) && q->closed) {
        pthread_mutex_unlock(&q->lock);
        return 0;
    }

    *request = q->buffer[q->head];
    q->head = sound_queue_next_index(q->head);

    pthread_mutex_unlock(&q->lock);
    return 1;
}

static void queue_sound(SoundType sound) {
    switch (sound) {
        case SOUND_CRASH:
            sound_queue_push(&g_sound_queue, 196.0, 70000);
            break;
        case SOUND_ITEM:
            sound_queue_push(&g_sound_queue, 659.0, 60000);
            break;
        case SOUND_ATTACK:
            sound_queue_push(&g_sound_queue, 523.0, 60000);
            break;
        case SOUND_HEAL:
            sound_queue_push(&g_sound_queue, 784.0, 70000);
            break;
        case SOUND_CLEAR:
            sound_queue_push(&g_sound_queue, 880.0, 70000);
            break;
        case SOUND_GAME_OVER:
            sound_queue_push(&g_sound_queue, 262.0, 120000);
            break;
        case SOUND_NONE:
        default:
            break;
    }
}

static void* sound_thread(void* arg) {
    SoundQueue *q = (SoundQueue *)arg;
    SoundRequest request;

    while (sound_queue_pop(q, &request)) {
        buzzer_play(request.freq, request.time_us);
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
        queue_sound(game->sound);
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
    pthread_t sound;
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
    sound_queue_init(&g_sound_queue);

    //타이머 스레드 생성
    pthread_create(&timer, NULL, timer_thread, NULL);
    pthread_create(&sound, NULL, sound_thread, &g_sound_queue);

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
    sound_queue_close(&g_sound_queue);

    // 3. 사운드 쓰레드 먼저 정리한다. 버저 fd가 닫히는 중에 재생하지 않게 하기 위함.
    pthread_join(timer, NULL);
    pthread_join(sound, NULL);

    // 4. 디바이스 파일 디스크립터들 닫기 (블로킹 I/O 강제 리턴 유도)
    serial_close();
    if (hw_enabled) hw_close();

    // 5. 모든 보조 쓰레드가 깔끔하게 정돈되어 죽을 때까지 대기 및 자원 수거
#ifdef SIMULATOR
    if (autotest_started) {
        pthread_join(autotest, NULL);
    }
#endif
    if (hw_enabled) pthread_join(hw, NULL);
    if (serial_enabled) pthread_join(serial, NULL);

    // 6. 최종 자원 정리
    sound_queue_destroy(&g_sound_queue);
    render_shutdown();
    debug_close();

    return 0;
}
