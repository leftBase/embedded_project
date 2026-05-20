#include "event.h"
#include "game.h"
#include "render.h"
#include "serial.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static EventQueue g_queue;
static GameState g_game;
static volatile int running = 1;

#define RENDER_INTERVAL_TICKS 4

static void* timer_thread(void* arg) {
    (void)arg;

    while (running) {
        usleep(TICK_MS * 1000);
        queue_push(&g_queue, EV_TICK, 0);
    }

    return NULL;
}

static void* keyboard_thread(void* arg) {
    (void)arg;

    while (running) {
        int c = getchar();

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

static void* serial_thread(void* arg) {
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

int main(void) {
    pthread_t timer;
    pthread_t keyboard;
    pthread_t serial;
    int serial_enabled;
    int render_tick_count = 0;

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
            render_tick_count++;
            if (render_tick_count >= RENDER_INTERVAL_TICKS) {
                render_tick_count = 0;
                render_game(&g_game);
            }
        } else {
            render_game(&g_game);
        }
    }

    serial_close();
    render_shutdown();

    return 0;
}
