#ifndef GAME_H
#define GAME_H

#include "types.h"

void game_init(Game *game, long now_ms);
void game_update(Game *game, const InputState *input, long now_ms);

void game_start(Game *game, long now_ms);
void game_toggle_pause(Game *game);

void game_move_player(Game *game, int player_index, int direction);
void game_use_item(Game *game, int player_index, long now_ms);

#endif