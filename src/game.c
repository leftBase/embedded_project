//입력신호 처리한거 가져다가 player 객체값의 갱신하고 수학연산을 하고 충돌판정 게임상태 어쩌구 저쩌구
#include "game.h"
#include "event.h"
#include <stdbool.h>
#include <stddef.h>

// 게임 초기화
void game_init(GameState *game) {
    game->mode = GAME_READY; 
    game->p[0].lane = 1; // 1P 중앙 레인
    game->p[0].life = 3;
    game->p[1].lane = 1; // 2P 중앙 레인
    game->p[1].life = 3;
    game->tick_count = 0;
}

// 이벤트에 따라 게임 상태 갱신
void game_apply_event(GameState *game, GameEvent ev) {
    if (game->mode > 0) return;

    switch (ev.type) {
        case EV_TICK:
            game->tick_count++;
            // 여기서 장애물 Y 좌표를 1씩 내리거나 새 장애물을 생성해
            break;
            
        case EV_P_LEFT:
            if (game->p[0].lane > 0) game->p[0].lane--;
            break;
            
        case EV_P_RIGHT:
            if (game->p[0].lane < 2) game->p[0].lane++;
            break;
        
        case EV_P_CRASH:
            game->p[0].life--;
            break;
            
        default:
            break;
    }
}

// 충돌판정, 아이템 사용 및 판정, 점수계산, 속도증가, 게임오버 판정, 배경음악 등잇어야댐

void game_update(Player *player, GameState *game, GameEvent ev) {
    if (game->mode > 0) return;

    switch (ev.type) {
        case EV_TICK:
            if 
            break;
        default:
            break;
    }
}

