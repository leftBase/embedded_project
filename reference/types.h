#ifndef TYPES_H
#define TYPES_H

#include "config.h"

typedef enum {
    STATE_READY,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAME_OVER
} GameState;

typedef enum {
    ITEM_NONE,
    ITEM_RED,
    ITEM_GREEN,
    ITEM_BLUE
} ItemType;

typedef struct {
    int lane;
    int life;
    int score;
    ItemType item;
    int alive;

    int mash_active;
    int mash_count;
    long mash_start_ms;
    ItemType mash_item;
} Player;

typedef struct {
    int active;
    int lane;
    int y;
    int type;
} Obstacle;

typedef struct {
    int p1_left;
    int p1_right;
    int p1_func;

    int p2_left;
    int p2_right;
    int p2_func;

    int start;
    int pause;
    int quit;
} InputState;

typedef struct {
    GameState state;

    Player players[PLAYER_COUNT];
    Obstacle obstacles[PLAYER_COUNT][MAX_OBSTACLES];

    long start_time_ms;
    long last_obstacle_move_ms;
    long last_obstacle_spawn_ms;
    long last_score_update_ms;
    long last_item_spawn_ms;

    int obstacle_interval_ms;
    int spawn_chance;

    int winner;
} Game;

#endif