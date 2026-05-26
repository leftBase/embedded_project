#ifndef GAME_H
#define GAME_H

#include "event.h"

//플레이어 수 2명, 인덱스 0, 1
#define PLAYER_COUNT 2
#define PLAYER_1 0
#define PLAYER_2 1

//레인수 3개, 몫 3개, 최대 5개
#define LANE_COUNT 3
#define INITIAL_LIFE 3
#define MAX_LIFE 5

//돌 최대 40개, 높이 16(낮은거같기도)
#define MAX_ROCKS 40
#define ROAD_HEIGHT 16

//틱 50ms, 돌이동 8틱, 스폰 14틱, 최소 이동 5틱, 난이도 상승 스텝 200틱마다.
#define TICK_MS 50
#define ROCK_MOVE_TICKS 8
#define ROCK_SPAWN_TICKS 14
#define ROCK_MIN_MOVE_TICKS 5
#define DIFFICULTY_STEP_TICKS 200

//아이템 스폰틱 100, 활성화 80, 초록색 스택 5
#define ITEM_SPAWN_TICKS 100
#define ITEM_ACTIVE_TICKS 80
#define BLUE_ACTIVE_TICKS ITEM_ACTIVE_TICKS
#define GREEN_HEAL_STACK 5

//생존 점수 20틱에 10점, 돌피하기 20점, 아이템 성공 30점, 충돌 -30점
#define SCORE_SURVIVE_TICKS 20
#define SCORE_SURVIVE 10
#define SCORE_AVOID_ROCK 20
#define SCORE_ITEM_SUCCESS 30
#define SCORE_CRASH_PENALTY 30

//게임모드: 준비, 실행, 퍼즈, 게임오버
typedef enum {
    GAME_READY,
    GAME_RUNNING,
    GAME_PAUSED,
    GAME_OVER
} GameMode;

//lcd 프리셋: M4 LCD arg2 0=RED, 1=GREEN, 2=BLUE, 3=LOGO
typedef enum {
    LCD_RED = 0,
    LCD_GREEN = 1,
    LCD_BLUE = 2,
    LCD_LOGO = 3
} LcdPreset;

typedef enum {
    SOUND_NONE = 0,
    SOUND_CRASH,
    SOUND_ITEM,
    SOUND_ATTACK,
    SOUND_HEAL,
    SOUND_CLEAR,
    SOUND_P1_LEFT,
    SOUND_P1_RIGHT,
    SOUND_P2_LEFT,
    SOUND_P2_RIGHT
} SoundType;

//플레이어 구조체
typedef struct {
    int lane;
    int speed;
    int life;
    int score;
    int fkey;
    int alive;
    int green_stack;     // 플레이어별 그린 스택은 그대로 유지
} Player;

//돌 구조체
typedef struct {
    int active;
    int lane;
    int y;
    int type; //있어야됨? 점수처리 목적으로 공격할때 만든 돌인지에 쓰면 될듯함
} Rock;

//게임 로그 구조체(나중엔 없어도 됨)
typedef struct {
    long timestamp;
    const char *event;
    int value;
} GameLog;

//게임 상태 구조체. 플레이어 두개, 돌 두개에 이중배열, state, 로그(쓰면), 음악, 게임실행시 쓰이는변수들을 담음.
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
    int log_count;
    SoundType sound;
    int sound_seq;

    ItemType item;      /* ITEM_NONE / ITEM_RED / ITEM_GREEN / ITEM_BLUE */
    int item_timer;     /* 남은 틱 동안의 타이머 */
} GameState;

//게임스테이트 초기화, 게임이벤트 구조체로 게임스테이트 변경, 게임틱 변경, 플레이어 이동, 템사용, 충돌체크, 로그저장
void game_init(GameState *game);
void game_apply_event(GameState *game, GameEvent ev);
void game_tick(GameState *game);
void game_move_player(GameState *game, int player_index, int direction);
void game_use_item(GameState *game, int player_index);
void game_check_collisions(GameState *game);

#endif
