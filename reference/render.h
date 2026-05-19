#ifndef RENDER_H
#define RENDER_H

#include "types.h"

void render_init(void);
void render_draw(const Game *game);
void render_close(void);

#endif