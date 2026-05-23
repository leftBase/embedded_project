#include "game.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 레인 한계넘으면 그냥 레인에 있게 함.
static int clamp_lane(int lane) {
    if (lane < 0) {
        return 0;
    }

    if (lane >= LANE_COUNT) {
        return LANE_COUNT - 1;
    }

    return lane;
}

// 플레이어 인덱스 주면 상대방 인덱스 반환 이딴건 왜있는거임? 그냥 하드코딩으로 인덱스랑 1이랑 xor하게 바꾸면 되잖아. 
static int opponent_of(int player_index) {
    return player_index == PLAYER_1 ? PLAYER_2 : PLAYER_1;
}

//플레이어 객체 초기화
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

//로그카운트 1000으로 나눈 나머지가 인덱스, 타임스탬프는 틱, 이벤트는 이벤트 타입, 밸류는 밸류, 로그카운트 1증가. 1000개까지 저장.
static void add_game_log(GameState *game, const char *event, int value) {
    int index = game->log_count % 1000;

    game->logs[index].timestamp = game->tick_count;
    game->logs[index].event = event;
    game->logs[index].value = value;
    game->log_count++;
}

//돌청소-파란템
static void clear_rocks(GameState *game, int player_index) {
    int i;

    for (i = 0; i < MAX_ROCKS; i++) {
        game->rocks[player_index][i].active = 0;
    }
}

//둘다 돌청소
static void clear_all_rocks(GameState *game) {
    clear_rocks(game, PLAYER_1);
    clear_rocks(game, PLAYER_2);
}

//아이템 랜덤반환
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

//아이템 lcd 매칭
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

//lcd 새로고침-지금보니까 p1 p2 아이템은 항상 같게 랜덤한데 고쳐야댄? item은 player에 잇으면 안되게슴
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

//호출되는 돌생성. 게임스테이트. 플레이어인덱스, 레인 받음.
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

//호출되는 랜덤한 돌생성.
static void spawn_random_rock(GameState *game, int player_index) {
    spawn_rock(game, player_index, rand() % LANE_COUNT);
}

//스텝틱이 틱카운트 채우면 스텝 올라가고 돌 이동틱 작아짐. 스텝이 올라갈수록 스폰 확률도 올라감.
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

//플레이어를 포문으로 돌필요가있나 플레이어마다 아이템 타이머가 0이고 아이템이그린이고 미리정한 스택 역치보다 스택이 높으면 맥스라이프 넘지 않게 life++
//아이템 타이머는 아이템 활성화 시간동안 1씩 감소. 아이템 타이머가 0이되면 아이템 효과 종료. 그린 아이템은 타이머가 0이 될 때 체력 회복 판정. 회복 성공하면 점수 보상.
//todo: GameState에서 item, item_timer, green_stack[2]관리함.
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
                add_game_log(game, "green_heal", p);
                DBG("green heal p=%d life=%d stack=%d", p + 1, player->life, player->green_stack);
            }

            player->item = ITEM_NONE;
            player->green_stack = 0;
        }
    }

    refresh_lcd(game);
}

//스텝틱이 아이템 스폰 틱 채우면 아이템 스폰. 아이템은 레드, 그린, 블루 중 랜덤. 아이템이 이미 있으면 안나오게. 아이템 효과 시간은 미리정한 시간으로 고정. 아이템 스폰하면 lcd 새로고침하고 로그에도 남김.
//todo: GameState에서 item, item_timer, green_stack[2]관리함.
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
    add_game_log(game, "item_spawn", item);
    DBG("item spawn item=%d timer=%d", item, timer);
}

//각각 랜덤 돌생성
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

//돌 이동하는데 포문으로하고 점수도 오름
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

//생존점수틱마다 점수
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

//한놈죽으면 겜끝. 이긴사람이 winner
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

    add_game_log(game, "game_over", game->winner);
    DBG("game over winner=%d", game->winner);
}

//게임시작
//게임 초기화, running
static void game_start(GameState *game) {
    game_init(game);
    game->state = GAME_RUNNING;
    add_game_log(game, "start", 0);
    DBG("game start");
}

//게임초기화
//todo: GameState에서 item, item_timer, green_stack[2]관리함.
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
    game->log_count = 0;

    reset_player(&game->players[PLAYER_1]);
    reset_player(&game->players[PLAYER_2]);
    clear_all_rocks(game);
}

//플레이어 이동
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

//플레이어가 아이템 사용.
//todo: GameState에서 item, item_timer, green_stack[2]관리함.
void game_use_item(GameState *game, int player_index) {
    Player *player;
    int opponent;

    if (player_index < 0 || player_index >= PLAYER_COUNT) {
        return;
    }

    player = &game->players[player_index];
    opponent = player_index ^ 1;

    if (!player->alive || player->item == ITEM_NONE) {
        return;
    }

    switch (player->item) {
        case ITEM_RED:
            spawn_random_rock(game, opponent);
            player->score += SCORE_ITEM_SUCCESS;
            player->item = ITEM_NONE;
            player->item_timer = 0;
            add_game_log(game, "red_attack", player_index);
            DBG("red attack p=%d target=%d", player_index + 1, opponent + 1);
            break;

        case ITEM_GREEN:
            player->green_stack++;
            player->fkey = player->green_stack;
            add_game_log(game, "green_stack", player_index);
            DBG("green stack p=%d stack=%d", player_index + 1, player->green_stack);
            break;

        case ITEM_BLUE:
            clear_rocks(game, player_index);
            player->score += SCORE_ITEM_SUCCESS;
            player->item = ITEM_NONE;
            player->item_timer = 0;
            add_game_log(game, "blue_clear", player_index);
            DBG("blue clear p=%d", player_index + 1);
            break;

        case ITEM_NONE:
        default:
            break;
    }

    refresh_lcd(game);
}

//플레이어랑 돌이랑 height 2차이나면 충돌.
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

                add_game_log(game, "crash", p);
                DBG("crash p=%d lane=%d life=%d score=%d", p + 1, player->lane, player->life, player->score);
            }
        }
    }
}

//게임틱==update
//GameState의 틱, 난이도, 아이템타이머, 스폰아이템, 스폰돌, 돌움직이기, 충돌판정, 생존점수, 게임오버판정 갱신
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

//GameEvent pop한걸 GameState에 적용
//스타트, 퍼즈, 틱, 좌우,스킬, 종료
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
                DBG("move p=1 lane=%d", game->players[PLAYER_1].lane);
            }
            break;

        case EV_P1_RIGHT:
            if (game->state == GAME_RUNNING) {
                game_move_player(game, PLAYER_1, 1);
                DBG("move p=1 lane=%d", game->players[PLAYER_1].lane);
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
                DBG("move p=2 lane=%d", game->players[PLAYER_2].lane);
            }
            break;

        case EV_P2_RIGHT:
            if (game->state == GAME_RUNNING) {
                game_move_player(game, PLAYER_2, 1);
                DBG("move p=2 lane=%d", game->players[PLAYER_2].lane);
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

//게임로그 저장
/*void save_game_log(GameLog logs[]) {
    FILE *fp = fopen("game_events.log", "w");
    int i;

    if (fp == NULL) {
        return;
    }

    for (i = 0; i < 100; i++) {
        if (logs[i].event != NULL) {
            fprintf(fp, "%ld,%s,%d\n", logs[i].timestamp, logs[i].event, logs[i].value);
        }
    }

    fclose(fp);
}*/
void save_game_log(GameLog logs[]) {
    // 1. 권한 문제가 없는 /tmp 임시 메모리 폴더에 파일 생성
    FILE *fp = fopen("game_events.log", "w");
    int i;
    int saved_count = 0;

    // 테라텀에 즉시 진입 알림
    printf("\n[보드] 100개 규격 로그 저장 시작...\n");
    fflush(stdout); 

    if (fp == NULL) {
        printf("[에러] 파일 생성 실패: %s\n", strerror(errno));
        fflush(stdout);
        return;
    }

    // 원래 스펙인 100개 루프로 원상복구
    for (i = 0; i < 100; i++) {
        // 포인터가 유효한 진짜 데이터만 필터링
        if (logs[i].event != NULL) {
            fprintf(fp, "%ld,%s,%d\n", logs[i].timestamp, logs[i].event, logs[i].value);
            saved_count++;
        }
    }

    fflush(fp); 
    fclose(fp);

    // 테라텀으로 결과 전송
    printf("[성공] 총 %d개의 게임 로그가 /tmp/game_events.log 에 저장 완료!\n", saved_count);
    fflush(stdout); 
}