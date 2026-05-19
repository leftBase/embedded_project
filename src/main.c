/*
프로그램 시작하면
화면띄우고
player객체 만들고
게임엔진 돌려서
player상태(lane speed rock 어쩌구), 게임상태(lcd, fnd, 실행중, 끝낫음, 게임로그, 사운드) 반복 갱신
끝나면 로그파일저장, 신기록 갱신
*/
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "input.h"
#include "game.h"
#include "render.h"
#include "serial.h"
// 표준 입출력 라이브러리
#include <stdio.h>
// 문자열 처리 라이브러리
#include <string.h>
// 파일 제어 라이브러리
#include <fcntl.h>
// 에러 처리 라이브러리
#include <errno.h>
// 시리얼 설정 라이브러리
#include <termios.h>
// POSIX 함수 라이브러리
#include <unistd.h>



int main() {
    // 초기화
    GameState game_state = {0};
    Player player = {1, 0, 3, 0}; // 중앙 레인, 정지, 3 라이프, 0 점수

    // 게임 루프
    while (game_state.state != GAME_OVER) {
        // 입력 처리
        handle_input(&player, &game_state);

        // 게임 상태 업데이트
        update_game(&player, &game_state);

        // 화면 렌더링
        render_game(&player, &game_state);
    }

    // 게임 종료 후 처리 (로그 저장 등)
    save_game_log(game_state.logs);
    return 0;
}