#ifndef RENDER_H
#define RENDER_H

#include <pthread.h>
#include "game.h"

/* main.c에서는 render_game(Player*, GameState*)를 호출하므로
	render 모듈 구현에서 이 시그니처를 제공해야 합니다. */
int render_init(void);
void render_shutdown(void);
void render_game(Player *player, GameState *gs);

extern pthread_t render_thread;

#endif // RENDER_H
