#include "game.h"
#include "device.h"

#include <stdlib.h>
#include <string.h>

static void reset_player(Player *player) {
    player->lane = 1;
    player->life = INITIAL_LIFE;
    player->score = 0;
    player->item = ITEM_NONE;
    player->alive = 1;

    player->mash_active = 0;
    player->mash_count = 0;
    player->mash_start_ms = 0;
    player->mash_item = ITEM_NONE;
}

static void clear_obstacles(Game *game, int player_index) {
    int i;

    for (i = 0; i < MAX_OBSTACLES; i++) {
        game->obstacles[player_index][i].active = 0;
    }
}

static void clear_all_obstacles(Game *game) {
    clear_obstacles(game, PLAYER_1);
    clear_obstacles(game, PLAYER_2);
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

static void spawn_obstacle_for_player(Game *game, int player_index, int lane) {
    int i;

    for (i = 0; i < MAX_OBSTACLES; i++) {
        if (!game->obstacles[player_index][i].active) {
            game->obstacles[player_index][i].active = 1;
            game->obstacles[player_index][i].lane = lane;
            game->obstacles[player_index][i].y = 0;
            game->obstacles[player_index][i].type = rand() % 3;
            return;
        }
    }
}

static void spawn_random_obstacle(Game *game, int player_index) {
    int lane = rand() % LANE_COUNT;
    spawn_obstacle_for_player(game, player_index, lane);
}

static void update_difficulty(Game *game, long now_ms) {
    long elapsed;
    int step;
    int interval;

    if (game->state != STATE_PLAYING) {
        return;
    }

    elapsed = now_ms - game->start_time_ms;
    step = (int)(elapsed / DIFFICULTY_STEP_MS);

    interval = OBSTACLE_START_INTERVAL_MS - (step * 80);

    if (interval < OBSTACLE_MIN_INTERVAL_MS) {
        interval = OBSTACLE_MIN_INTERVAL_MS;
    }

    game->obstacle_interval_ms = interval;

    game->spawn_chance = 25 + (step * 7);
    if (game->spawn_chance > 70) {
        game->spawn_chance = 70;
    }
}

static void spawn_obstacles(Game *game, long now_ms) {
    if (now_ms - game->last_obstacle_spawn_ms < game->obstacle_interval_ms) {
        return;
    }

    game->last_obstacle_spawn_ms = now_ms;

    if ((rand() % 100) < game->spawn_chance) {
        spawn_random_obstacle(game, PLAYER_1);
    }

    if ((rand() % 100) < game->spawn_chance) {
        spawn_random_obstacle(game, PLAYER_2);
    }
}

static void move_obstacles(Game *game, long now_ms) {
    int p;
    int i;

    if (now_ms - game->last_obstacle_move_ms < game->obstacle_interval_ms) {
        return;
    }

    game->last_obstacle_move_ms = now_ms;

    for (p = 0; p < PLAYER_COUNT; p++) {
        for (i = 0; i < MAX_OBSTACLES; i++) {
            if (game->obstacles[p][i].active) {
                game->obstacles[p][i].y++;

                if (game->obstacles[p][i].y >= ROAD_HEIGHT) {
                    game->obstacles[p][i].active = 0;
                    game->players[p].score += SCORE_AVOID_OBSTACLE;
                }
            }
        }
    }
}

static void check_collisions(Game *game) {
    int p;
    int i;
    Player *player;
    Obstacle *obs;

    for (p = 0; p < PLAYER_COUNT; p++) {
        player = &game->players[p];

        if (!player->alive) {
            continue;
        }

        for (i = 0; i < MAX_OBSTACLES; i++) {
            obs = &game->obstacles[p][i];

            if (!obs->active) {
                continue;
            }

            if (obs->y >= ROAD_HEIGHT - 2 && obs->lane == player->lane) {
                obs->active = 0;

                player->life--;
                player->score -= SCORE_COLLISION_PENALTY;

                if (player->score < 0) {
                    player->score = 0;
                }

                device_play_sound(SOUND_CRASH);
                device_show_led(p, LED_CRASH);

                if (player->life <= 0) {
                    player->life = 0;
                    player->alive = 0;
                }
            }
        }
    }
}

static void update_survival_score(Game *game, long now_ms) {
    int p;

    if (now_ms - game->last_score_update_ms < 1000) {
        return;
    }

    game->last_score_update_ms = now_ms;

    for (p = 0; p < PLAYER_COUNT; p++) {
        if (game->players[p].alive) {
            game->players[p].score += SCORE_SURVIVE_PER_SEC;
        }
    }

    if (game->players[PLAYER_1].score >= game->players[PLAYER_2].score) {
        device_show_score(game->players[PLAYER_1].score);
    } else {
        device_show_score(game->players[PLAYER_2].score);
    }
}

static void spawn_items(Game *game, long now_ms) {
    int p;

    if (now_ms - game->last_item_spawn_ms < ITEM_SPAWN_INTERVAL_MS) {
        return;
    }

    game->last_item_spawn_ms = now_ms;

    for (p = 0; p < PLAYER_COUNT; p++) {
        if (game->players[p].item == ITEM_NONE && (rand() % 100) < ITEM_SPAWN_CHANCE) {
            game->players[p].item = random_item();
            device_show_item(p, game->players[p].item);
            device_play_sound(SOUND_ITEM);
        }
    }
}

static void start_mash(Player *player, long now_ms) {
    player->mash_active = 1;
    player->mash_count = 1;
    player->mash_start_ms = now_ms;
    player->mash_item = player->item;
}

static void finish_mash_success(Game *game, int player_index) {
    Player *player = &game->players[player_index];

    if (player->mash_item == ITEM_GREEN) {
        if (player->life < MAX_LIFE) {
            player->life++;
        }

        player->score += SCORE_ITEM_SUCCESS;
        device_show_led(player_index, LED_HEAL);
        device_play_sound(SOUND_HEAL);
    } else if (player->mash_item == ITEM_BLUE) {
        clear_obstacles(game, player_index);

        player->score += SCORE_ITEM_SUCCESS;
        device_show_led(player_index, LED_CLEAR);
        device_play_sound(SOUND_CLEAR);
    }

    player->item = ITEM_NONE;
    player->mash_active = 0;
    player->mash_count = 0;
    player->mash_item = ITEM_NONE;

    device_show_item(player_index, ITEM_NONE);
}

static void fail_mash(Player *player) {
    player->item = ITEM_NONE;
    player->mash_active = 0;
    player->mash_count = 0;
    player->mash_item = ITEM_NONE;
}

static void update_mash_timeout(Game *game, long now_ms) {
    int p;
    Player *player;

    for (p = 0; p < PLAYER_COUNT; p++) {
        player = &game->players[p];

        if (player->mash_active) {
            if (now_ms - player->mash_start_ms > ITEM_MASH_TIME_MS) {
                fail_mash(player);
                device_show_item(p, ITEM_NONE);
            }
        }
    }
}

static void check_game_over(Game *game) {
    int p1_alive = game->players[PLAYER_1].alive;
    int p2_alive = game->players[PLAYER_2].alive;

    if (p1_alive && p2_alive) {
        return;
    }

    game->state = STATE_GAME_OVER;

    if (!p1_alive && !p2_alive) {
        game->winner = -1;
    } else if (p1_alive) {
        game->winner = PLAYER_1;
        device_show_led(PLAYER_1, LED_WIN);
    } else {
        game->winner = PLAYER_2;
        device_show_led(PLAYER_2, LED_WIN);
    }

    device_play_sound(SOUND_GAME_OVER);
}

void game_init(Game *game, long now_ms) {
    memset(game, 0, sizeof(Game));

    game->state = STATE_READY;
    game->winner = -2;

    reset_player(&game->players[PLAYER_1]);
    reset_player(&game->players[PLAYER_2]);

    clear_all_obstacles(game);

    game->start_time_ms = now_ms;
    game->last_obstacle_move_ms = now_ms;
    game->last_obstacle_spawn_ms = now_ms;
    game->last_score_update_ms = now_ms;
    game->last_item_spawn_ms = now_ms;

    game->obstacle_interval_ms = OBSTACLE_START_INTERVAL_MS;
    game->spawn_chance = 25;
}

void game_start(Game *game, long now_ms) {
    game_init(game, now_ms);

    game->state = STATE_PLAYING;
    game->start_time_ms = now_ms;
    game->last_obstacle_move_ms = now_ms;
    game->last_obstacle_spawn_ms = now_ms;
    game->last_score_update_ms = now_ms;
    game->last_item_spawn_ms = now_ms;
}

void game_toggle_pause(Game *game) {
    if (game->state == STATE_PLAYING) {
        game->state = STATE_PAUSED;
    } else if (game->state == STATE_PAUSED) {
        game->state = STATE_PLAYING;
    }
}

void game_move_player(Game *game, int player_index, int direction) {
    Player *player = &game->players[player_index];

    if (!player->alive) {
        return;
    }

    player->lane += direction;

    if (player->lane < 0) {
        player->lane = 0;
    }

    if (player->lane >= LANE_COUNT) {
        player->lane = LANE_COUNT - 1;
    }
}

void game_use_item(Game *game, int player_index, long now_ms) {
    Player *player = &game->players[player_index];
    int opponent = (player_index == PLAYER_1) ? PLAYER_2 : PLAYER_1;

    if (!player->alive) {
        return;
    }

    if (player->mash_active) {
        player->mash_count++;

        if (player->mash_count >= ITEM_MASH_LIMIT) {
            finish_mash_success(game, player_index);
        }

        return;
    }

    if (player->item == ITEM_NONE) {
        return;
    }

    if (player->item == ITEM_RED) {
        spawn_random_obstacle(game, opponent);

        player->score += SCORE_ITEM_SUCCESS;
        player->item = ITEM_NONE;

        device_show_item(player_index, ITEM_NONE);
        device_play_sound(SOUND_ATTACK);
        return;
    }

    if (player->item == ITEM_GREEN || player->item == ITEM_BLUE) {
        start_mash(player, now_ms);
    }
}

void game_update(Game *game, const InputState *input, long now_ms) {
    if (game->state == STATE_READY) {
        if (input->start) {
            game_start(game, now_ms);
        }
        return;
    }

    if (game->state == STATE_GAME_OVER) {
        if (input->start) {
            game_start(game, now_ms);
        }
        return;
    }

    if (input->pause) {
        game_toggle_pause(game);
        return;
    }

    if (game->state == STATE_PAUSED) {
        return;
    }

    if (game->state != STATE_PLAYING) {
        return;
    }

    if (input->p1_left) {
        game_move_player(game, PLAYER_1, -1);
    }

    if (input->p1_right) {
        game_move_player(game, PLAYER_1, 1);
    }

    if (input->p2_left) {
        game_move_player(game, PLAYER_2, -1);
    }

    if (input->p2_right) {
        game_move_player(game, PLAYER_2, 1);
    }

    if (input->p1_func) {
        game_use_item(game, PLAYER_1, now_ms);
    }

    if (input->p2_func) {
        game_use_item(game, PLAYER_2, now_ms);
    }

    update_difficulty(game, now_ms);
    update_mash_timeout(game, now_ms);
    spawn_items(game, now_ms);
    spawn_obstacles(game, now_ms);
    move_obstacles(game, now_ms);
    check_collisions(game);
    update_survival_score(game, now_ms);
    check_game_over(game);
}