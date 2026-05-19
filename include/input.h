#ifndef INPUT_H
#define INPUT_H

#include <pthread.h>
#include "game.h"

/* 기존 main.c 호출명에 맞춘 입력 API */
/* handle_input는 main 루프에서 사용되거나 input 모듈에서 제공될 수 있음 */
void handle_input(Player *player, GameState *gs);

/* optional: 스레드 기반 input 초기화/종료 */
int input_init(void);
void input_shutdown(void);

extern pthread_t input_thread;

#endif // INPUT_H
