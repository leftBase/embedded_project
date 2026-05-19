#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stddef.h>

/* 기존 소스(`src/game.c`, `src/main.c`)에서 사용되는 구조체/함수명에 맞춘 헤더
   Player, Rock, GameState, GameLog 등 직접 선언합니다. */

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
	enum { RUNNING, PAUSED, GAME_OVER } state;
	GameLog logs[100];
	int music;      // 0/1
} GameState;

/* main.c에서 호출하는 함수들 (선언만) */
void handle_input(Player *player, GameState *gs);
void update_game(Player *player, GameState *gs);
void render_game(Player *player, GameState *gs);
void save_game_log(GameLog logs[]);

#endif // GAME_H

typedef 