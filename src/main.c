/*
프로그램 시작하면
화면띄우고
player객체 만들고
게임엔진 돌려서
player상태(lane speed rock 어쩌구), 게임상태(lcd, fnd, 실행중, 끝낫음, 게임로그, 사운드) 반복 갱신
끝나면 로그파일저장, 신기록 갱신<<되면함
*/
#include "game.h"
#include "render.h"
#include "serial.h"
#include "event.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

EventQueue g_queue;
GameState g_game;
static volatile int running = 1;

void* timer_thread(void* arg) {
    (void)arg;

    while (running) {
        usleep(TICK_MS * 1000);
        queue_push(&g_queue, EV_TICK, 0);
    }

    return NULL;
}

void* keyboard_thread(void* arg) {
    (void)arg;

    while (running) {
        char c = getchar();

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
                queue_push(&g_queue, EV_QUIT, 1);
                break;
            default:
                break;
        }
    }

    return NULL;
}

void* serial_thread(void* arg) {
    (void)arg;

    while (running) {
        GameEvent event;
        int result = serial_next_event(&event);

        if (result > 0) {
            queue_push_event(&g_queue, event);
        }
    }

    return NULL;
}

int main() {
    pthread_t timer;
    pthread_t keyboard;
    pthread_t serial;
    int serial_enabled;

    srand((unsigned int)time(NULL));

    queue_init(&g_queue);
    game_init(&g_game);
    render_init();

    pthread_create(&timer, NULL, timer_thread, NULL);
    pthread_create(&keyboard, NULL, keyboard_thread, NULL);

    serial_enabled = serial_init(NULL, 115200) == 0;
    if (serial_enabled) {
        pthread_create(&serial, NULL, serial_thread, NULL);
    }
    
    while (running) {
        GameEvent ev = queue_pop(&g_queue);

        if (ev.type == EV_QUIT) {
            running = 0;
            break;
        }
        
        game_apply_event(&g_game, ev);
        
        if (ev.type == EV_TICK) {
            render_game(&g_game);
        }
    }

    serial_close();
    render_shutdown();

    return 0;
}
