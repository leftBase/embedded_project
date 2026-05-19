//입력신호 처리한거 가져다가 player 객체값의 갱신하고 수학연산을 하고 충돌판정 게임상태 어쩌구 저쩌구
typedef struct {
    int lane;           // l r 따라 ++ -- [0-2]
    int speed;          // 현재 속도
    int life;     
    int score;
    int fkey;       // 거리+공격+(점수)

} Player;

typedef struct {
    Player players[2];
    Rock rock[64];
    int lcd;        // 0-4 
    int fnd;        // 0-2 0-9 점수표시
   enum { RUNNING, PAUSED, GAME_OVER } state; // 게임 상태
    GameLog logs[100]; // 게임 이벤트
    int music;      // 배경음악 끄기 켜기 0 1
} GameState;

typedef struct {
    int lane;       // 장애물 위치
    int type;       // 0: 바위, 1: 아이템
    int life;       // 아님 far 이거 언제 player.y 오는지 
} Rock;

typedef struct {
    long timestamp;      // milliseconds
    const char *event;   // "collision", "score_up", "lane_change"
    int value;           // 관련 값 (예: 점수 증가량)
} GameLog;
