#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

typedef struct { int lane; int speed; int score; int life; } Player;
typedef struct { int lane; int pos; int speed; bool active; } Obstacle;
typedef struct {
    Player players[2];
    Obstacle obstacles[64];
    int obstacle_count;
    int state; // RUNNING/PAUSED/OVER
} GameState;

GameState gs;
pthread_mutex_t gs_lock = PTHREAD_MUTEX_INITIALIZER;
atomic_bool running = true;

/* 입력 스레드: 입력을 받아 간단한 명령(예: player 이동)을 gs에 반영 */
void input_handle_move(int player_id, int new_lane) {
    pthread_mutex_lock(&gs_lock);
    gs.players[player_id].lane = new_lane;
    pthread_mutex_unlock(&gs_lock);
}

/* 게임 스레드: 락 잡고 장애물 이동/충돌 처리/생성 등 모든 상태 갱신 */
void update_game_tick(void) {
    pthread_mutex_lock(&gs_lock);
    // 장애물 이동
    for (int i=0;i<gs.obstacle_count;i++){
        if (!gs.obstacles[i].active) continue;
        gs.obstacles[i].pos += gs.obstacles[i].speed;
        // 충돌 판정 (같은 레인 && pos 충돌 체크)
        for (int p=0;p<2;p++){
            if (gs.players[p].lane == gs.obstacles[i].lane &&
                gs.obstacles[i].pos >= PLAYER_POS && /* threshold */) {
                gs.players[p].life--;
                gs.obstacles[i].active = false;
            }
        }
        if (gs.obstacles[i].pos > PLAY_AREA_HEIGHT) gs.obstacles[i].active = false;
    }
    pthread_mutex_unlock(&gs_lock);
}

/* 렌더 스레드: 락으로 상태를 복사한 뒤 해제하고 ncurses로 그리기 */
void render_loop(void) {
    GameState snapshot;
    while (atomic_load(&running)) {
        pthread_mutex_lock(&gs_lock);
        snapshot = gs; // 얕은 복사(구조가 크면 필요한 부분만 복사)
        pthread_mutex_unlock(&gs_lock);
        // ncurses로 snapshot 기반 렌더링 (ncurses 호출은 render 스레드만)
    }
}