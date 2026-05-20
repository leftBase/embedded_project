#ifndef GAME_H
#define GAME_H

#include "event.h"

typedef enum {
    GAME_RUNNING,
	GAME_READY,
    GAME_PAUSED,
    GAME_OVER
} GameMode;
typedef struct {
	int lane;   // 0..2	
	int speed;  // 현재 속도
	int life;
	int score;
	int fkey;   // fkey 상태
} Player;

typedef struct {
	int lane;   // 장애물 레인
	int type;   // 0=바위,1=아이템 등
	int life;   // 거리 등으로 활용
} Rock;

typedef struct {
	long timestamp;      // milliseconds
	const char *event;   // 이벤트 문자열
	int value;
} GameLog;

typedef struct {
	Player players[2];
	Rock rock[64];
	int lcd;        // 0-4
	int fnd;        // 0-2
	GameMode state;
	GameLog logs[100];
	int music;      // 0/1
} GameState;

void game_init(GameState *game);
void game_apply_event(GameState *game, GameEvent ev);

/* main.c에서 호출하는 함수들 (선언만) */
void handle_input(Player *player, GameState *gs);
void update_game(Player *player, GameState *gs);
void render_game(Player *player, GameState *gs);
void save_game_log(GameLog logs[]);


#endif