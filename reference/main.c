#include "game.h"
#include "render.h"
#include "input.h"
#include "device.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

static int running = 1;

static long current_time_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

static void sleep_ms(int ms) {
    usleep((useconds_t)ms * 1000);
}

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    Game game;
    InputState input;
    long now;

    signal(SIGINT, handle_signal);
    srand((unsigned int)time(NULL));

    now = current_time_ms();

    game_init(&game, now);
    input_init();
    device_init();
    render_init();

    while (running) {
        now = current_time_ms();

        input = input_read();

        if (input.quit) {
            break;
        }

        game_update(&game, &input, now);
        render_draw(&game);

        sleep_ms(FRAME_DELAY_MS);
    }

    render_close();
    device_close();
    input_close();

    printf("Game closed.\n");

    return 0;
}