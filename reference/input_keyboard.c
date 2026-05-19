#include "input.h"

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>

static struct termios original_termios;
static int original_flags;

static void set_raw_mode(void) {
    struct termios raw;

    tcgetattr(STDIN_FILENO, &original_termios);
    raw = original_termios;

    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    original_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, original_flags | O_NONBLOCK);
}

void input_init(void) {
    set_raw_mode();
}

InputState input_read(void) {
    InputState input;
    int ch;

    memset(&input, 0, sizeof(InputState));

    while ((ch = getchar()) != EOF) {
        switch (ch) {
            case 'a':
            case 'A':
                input.p1_left = 1;
                break;

            case 'd':
            case 'D':
                input.p1_right = 1;
                break;

            case 'q':
            case 'Q':
                input.p1_func = 1;
                break;

            case 's':
            case 'S':
                input.start = 1;
                break;

            case 'p':
            case 'P':
                input.pause = 1;
                break;

            case 'j':
            case 'J':
                input.p2_left = 1;
                break;

            case 'l':
            case 'L':
                input.p2_right = 1;
                break;

            case 'o':
            case 'O':
                input.p2_func = 1;
                break;

            case 'x':
            case 'X':
                input.quit = 1;
                break;

            default:
                break;
        }
    }

    clearerr(stdin);
    return input;
}

void input_close(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
    fcntl(STDIN_FILENO, F_SETFL, original_flags);
}