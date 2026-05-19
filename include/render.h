#ifndef RENDER_H
#define RENDER_H

#include "game.h"

int render_init(void);
void render_shutdown(void);
void render_game(const GameState *game);

#endif // RENDER_H
