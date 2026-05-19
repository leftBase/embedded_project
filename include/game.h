#ifndef GAME_H
#define GAME_H

#include "event.h"

#define PLAYER_COUNT 2
#define PLAYER_1 0
#define PLAYER_2 1

#define LANE_COUNT 3
#define INITIAL_LIFE 3
#define MAX_LIFE 5

#define MAX_ROCKS 40
#define ROAD_HEIGHT 14

#define TICK_MS 50
#define ROCK_MOVE_TICKS 8
#define ROCK_SPAWN_TICKS 14
#define ROCK_MIN_MOVE_TICKS 5
#define DIFFICULTY_STEP_TICKS 200

#define ITEM_SPAWN_TICKS 100
#define ITEM_ACTIVE_TICKS 80
#define BLUE_ACTIVE_TICKS 20
#define GREEN_HEAL_STACK 5

#define SCORE_SURVIVE_TICKS 20
#define SCORE_SURVIVE 10
#define SCORE_AVOID_ROCK 20
#define SCORE_ITEM_SUCCESS 30
#define SCORE_CRASH_PENALTY 30

typedef enum {
    GAME_READY,
    GAME_RUNNING,
    GAME_PAUSED,
    GAME_OVER
} GameMode;

typedef enum {
    LCD_LOGO = 0,
    LCD_RED = 1,
    LCD_GREEN = 2,
    LCD_BLUE = 3
} LcdPreset;

typedef struct {
    int lane;
    int speed;
    int life;
    int score;
    int fkey;
    int alive;
    ItemType item;
    int item_timer;
    int green_stack;
} Player;

typedef struct {
    int active;
    int lane;
    int y;
    int type;
} Rock;

typedef struct {
    long timestamp;
    const char *event;
    int value;
} GameLog;

typedef struct {
    Player players[PLAYER_COUNT];
    Rock rocks[PLAYER_COUNT][MAX_ROCKS];
    GameMode state;
    GameLog logs[100];
    int lcd;
    int fnd;
    int music;
    int tick_count;
    int rock_move_ticks;
    int rock_spawn_ticks;
    int spawn_chance;
    int winner;
} GameState;

void game_init(GameState *game);
void game_apply_event(GameState *game, GameEvent ev);
void game_tick(GameState *game);
void game_move_player(GameState *game, int player_index, int direction);
void game_use_item(GameState *game, int player_index);
void game_check_collisions(GameState *game);
void save_game_log(GameLog logs[]);

#endif
