#include "game.h"
#include <stdlib.h>
#include <string.h>

static int clamp_lane(int lane) {
    if (lane < 0) {
        return 0;
    }

    if (lane >= LANE_COUNT) {
        return LANE_COUNT - 1;
    }

    return lane;
}

static int opponent_of(int player_index) {
    return player_index == PLAYER_1 ? PLAYER_2 : PLAYER_1;
}

static void reset_player(Player *player) {
    player->lane = 1;
    player->speed = 1;
    player->life = INITIAL_LIFE;
    player->score = 0;
    player->fkey = 0;
    player->alive = 1;
    player->item = ITEM_NONE;
    player->item_timer = 0;
    player->green_stack = 0;
}

static void clear_rocks(GameState *game, int player_index) {
    int i;

    for (i = 0; i < MAX_ROCKS; i++) {
        game->rocks[player_index][i].active = 0;
    }
}

static void clear_all_rocks(GameState *game) {
    clear_rocks(game, PLAYER_1);
    clear_rocks(game, PLAYER_2);
}

static ItemType random_item(void) {
    int r = rand() % 3;

    if (r == 0) {
        return ITEM_RED;
    }

    if (r == 1) {
        return ITEM_GREEN;
    }

    return ITEM_BLUE;
}

static int lcd_from_item(ItemType item) {
    switch (item) {
        case ITEM_RED:
            return LCD_RED;
        case ITEM_GREEN:
            return LCD_GREEN;
        case ITEM_BLUE:
            return LCD_BLUE;
        case ITEM_NONE:
        default:
            return LCD_LOGO;
    }
}

static void refresh_lcd(GameState *game) {
    if (game->players[PLAYER_1].item != ITEM_NONE) {
        game->lcd = lcd_from_item(game->players[PLAYER_1].item);
        return;
    }

    if (game->players[PLAYER_2].item != ITEM_NONE) {
        game->lcd = lcd_from_item(game->players[PLAYER_2].item);
        return;
    }

    game->lcd = LCD_LOGO;
}

static void spawn_rock(GameState *game, int player_index, int lane) {
    int i;

    for (i = 0; i < MAX_ROCKS; i++) {
        Rock *rock = &game->rocks[player_index][i];

        if (!rock->active) {
            rock->active = 1;
            rock->lane = clamp_lane(lane);
            rock->y = 0;
            rock->type = 0;
            return;
        }
    }
}

static void spawn_random_rock(GameState *game, int player_index) {
    spawn_rock(game, player_index, rand() % LANE_COUNT);
}

static void update_difficulty(GameState *game) {
    int step = game->tick_count / DIFFICULTY_STEP_TICKS;
    int move_ticks = ROCK_MOVE_TICKS - step;

    if (move_ticks < ROCK_MIN_MOVE_TICKS) {
        move_ticks = ROCK_MIN_MOVE_TICKS;
    }

    game->rock_move_ticks = move_ticks;
    game->rock_spawn_ticks = ROCK_SPAWN_TICKS;
    game->spawn_chance = 25 + step * 7;

    if (game->spawn_chance > 70) {
        game->spawn_chance = 70;
    }
}

static void update_item_timers(GameState *game) {
    int p;

    for (p = 0; p < PLAYER_COUNT; p++) {
        Player *player = &game->players[p];

        if (player->item == ITEM_NONE || player->item_timer <= 0) {
            continue;
        }

        player->item_timer--;

        if (player->item_timer == 0) {
            if (player->item == ITEM_GREEN && player->green_stack >= GREEN_HEAL_STACK) {
                if (player->life < MAX_LIFE) {
                    player->life++;
                }
                player->score += SCORE_ITEM_SUCCESS;
            }

            player->item = ITEM_NONE;
            player->green_stack = 0;
        }
    }

    refresh_lcd(game);
}

static void spawn_item_if_needed(GameState *game) {
    ItemType item;
    int timer;

    if (game->tick_count == 0 || game->tick_count % ITEM_SPAWN_TICKS != 0) {
        return;
    }

    if (game->players[PLAYER_1].item != ITEM_NONE || game->players[PLAYER_2].item != ITEM_NONE) {
        return;
    }

    item = random_item();
    timer = item == ITEM_BLUE ? BLUE_ACTIVE_TICKS : ITEM_ACTIVE_TICKS;

    game->players[PLAYER_1].item = item;
    game->players[PLAYER_1].item_timer = timer;
    game->players[PLAYER_1].green_stack = 0;

    game->players[PLAYER_2].item = item;
    game->players[PLAYER_2].item_timer = timer;
    game->players[PLAYER_2].green_stack = 0;

    refresh_lcd(game);
}

static void spawn_rocks_if_needed(GameState *game) {
    if (game->tick_count == 0 || game->tick_count % game->rock_spawn_ticks != 0) {
        return;
    }

    if ((rand() % 100) < game->spawn_chance) {
        spawn_random_rock(game, PLAYER_1);
    }

    if ((rand() % 100) < game->spawn_chance) {
        spawn_random_rock(game, PLAYER_2);
    }
}

static void move_rocks(GameState *game) {
    int p;
    int i;

    if (game->tick_count == 0 || game->tick_count % game->rock_move_ticks != 0) {
        return;
    }

    for (p = 0; p < PLAYER_COUNT; p++) {
        for (i = 0; i < MAX_ROCKS; i++) {
            Rock *rock = &game->rocks[p][i];

            if (!rock->active) {
                continue;
            }

            rock->y++;

            if (rock->y >= ROAD_HEIGHT) {
                rock->active = 0;
                game->players[p].score += SCORE_AVOID_ROCK;
            }
        }
    }
}

static void update_survival_score(GameState *game) {
    int p;

    if (game->tick_count == 0 || game->tick_count % SCORE_SURVIVE_TICKS != 0) {
        return;
    }

    for (p = 0; p < PLAYER_COUNT; p++) {
        if (game->players[p].alive) {
            game->players[p].score += SCORE_SURVIVE;
        }
    }
}

static void check_game_over(GameState *game) {
    int p1_alive = game->players[PLAYER_1].alive;
    int p2_alive = game->players[PLAYER_2].alive;

    if (p1_alive && p2_alive) {
        return;
    }

    game->state = GAME_OVER;

    if (!p1_alive && !p2_alive) {
        game->winner = -1;
    } else if (p1_alive) {
        game->winner = PLAYER_1;
    } else {
        game->winner = PLAYER_2;
    }
}

static void game_start(GameState *game) {
    game_init(game);
    game->state = GAME_RUNNING;
}

void game_init(GameState *game) {
    memset(game, 0, sizeof(GameState));

    game->state = GAME_READY;
    game->winner = -2;
    game->lcd = LCD_LOGO;
    game->fnd = 0;
    game->music = 1;
    game->tick_count = 0;
    game->rock_move_ticks = ROCK_MOVE_TICKS;
    game->rock_spawn_ticks = ROCK_SPAWN_TICKS;
    game->spawn_chance = 25;

    reset_player(&game->players[PLAYER_1]);
    reset_player(&game->players[PLAYER_2]);
    clear_all_rocks(game);
}

void game_move_player(GameState *game, int player_index, int direction) {
    Player *player;

    if (player_index < 0 || player_index >= PLAYER_COUNT) {
        return;
    }

    player = &game->players[player_index];

    if (!player->alive) {
        return;
    }

    player->lane = clamp_lane(player->lane + direction);
}

void game_use_item(GameState *game, int player_index) {
    Player *player;
    int opponent;

    if (player_index < 0 || player_index >= PLAYER_COUNT) {
        return;
    }

    player = &game->players[player_index];
    opponent = opponent_of(player_index);

    if (!player->alive || player->item == ITEM_NONE) {
        return;
    }

    switch (player->item) {
        case ITEM_RED:
            spawn_random_rock(game, opponent);
            player->score += SCORE_ITEM_SUCCESS;
            player->item = ITEM_NONE;
            player->item_timer = 0;
            break;

        case ITEM_GREEN:
            player->green_stack++;
            player->fkey = player->green_stack;
            break;

        case ITEM_BLUE:
            clear_rocks(game, player_index);
            player->score += SCORE_ITEM_SUCCESS;
            player->item = ITEM_NONE;
            player->item_timer = 0;
            break;

        case ITEM_NONE:
        default:
            break;
    }

    refresh_lcd(game);
}

void game_check_collisions(GameState *game) {
    int p;
    int i;

    for (p = 0; p < PLAYER_COUNT; p++) {
        Player *player = &game->players[p];

        if (!player->alive) {
            continue;
        }

        for (i = 0; i < MAX_ROCKS; i++) {
            Rock *rock = &game->rocks[p][i];

            if (!rock->active) {
                continue;
            }

            if (rock->y >= ROAD_HEIGHT - 2 && rock->lane == player->lane) {
                rock->active = 0;
                player->life--;
                player->score -= SCORE_CRASH_PENALTY;

                if (player->score < 0) {
                    player->score = 0;
                }

                if (player->life <= 0) {
                    player->life = 0;
                    player->alive = 0;
                }
            }
        }
    }
}

void game_tick(GameState *game) {
    if (game->state != GAME_RUNNING) {
        return;
    }

    game->tick_count++;
    update_difficulty(game);
    update_item_timers(game);
    spawn_item_if_needed(game);
    spawn_rocks_if_needed(game);
    move_rocks(game);
    game_check_collisions(game);
    update_survival_score(game);
    check_game_over(game);
}

void game_apply_event(GameState *game, GameEvent ev) {
    switch (ev.type) {
        case EV_START:
            if (game->state == GAME_READY || game->state == GAME_OVER) {
                game_start(game);
            }
            break;

        case EV_PAUSE:
            if (game->state == GAME_RUNNING) {
                game->state = GAME_PAUSED;
            } else if (game->state == GAME_PAUSED) {
                game->state = GAME_RUNNING;
            }
            break;

        case EV_TICK:
            game_tick(game);
            break;

        case EV_P1_LEFT:
            if (game->state == GAME_RUNNING) {
                game_move_player(game, PLAYER_1, -1);
            }
            break;

        case EV_P1_RIGHT:
            if (game->state == GAME_RUNNING) {
                game_move_player(game, PLAYER_1, 1);
            }
            break;

        case EV_P1_SKILL:
            if (game->state == GAME_RUNNING) {
                game_use_item(game, PLAYER_1);
            }
            break;

        case EV_P2_LEFT:
            if (game->state == GAME_RUNNING) {
                game_move_player(game, PLAYER_2, -1);
            }
            break;

        case EV_P2_RIGHT:
            if (game->state == GAME_RUNNING) {
                game_move_player(game, PLAYER_2, 1);
            }
            break;

        case EV_P2_SKILL:
            if (game->state == GAME_RUNNING) {
                game_use_item(game, PLAYER_2);
            }
            break;

        case EV_NONE:
        case EV_QUIT:
        default:
            break;
    }
}
