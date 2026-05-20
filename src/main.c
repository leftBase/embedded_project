/*
프로그램 시작하면
화면띄우고
player객체 만들고
게임엔진 돌려서
player상태(lane speed rock 어쩌구), 게임상태(lcd, fnd, 실행중, 끝낫음, 게임로그, 사운드) 반복 갱신
끝나면 로그파일저장, 신기록 갱신<<되면함
*/
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include "input.h"
#include "game.h"
#include "render.h"
#include "serial.h"
#include "event.h"
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


EventQueue g_queue;
GameState g_game;

// 스레드 1: 타이머 (0.05초마다 TICK 이벤트 던짐)
void* timer_thread(void* arg) {
    while (1) {
        usleep(50000); // 50ms 대기
        queue_push(&g_queue, EV_TICK, 0);
    }
    return NULL;
}

// 스레드 2: 키보드 입력 (가짜 M4/Q6 장치 역할)
void* input_thread(void* arg) {
    while (1) {
        char c = getchar();
        if (c == 'a') queue_push(&g_queue, EV_P1_LEFT, 1);
        if (c == 'd') queue_push(&g_queue, EV_P1_RIGHT, 1);
        // 엔터키 무시 등 처리
    }
    return NULL;
}


int main() {
    queue_init(&g_queue);
    game_init(&g_game);

    pthread_t t1, t2;
    // 백그라운드 스레드 생성 (이 순간부터 쟤네들은 각자 while문을 돌기 시작함)
    pthread_create(&t1, NULL, timer_thread, NULL);
    pthread_create(&t2, NULL, input_thread, NULL);

    // 스레드 3 (메인 스레드): 오직 이벤트 처리만 함
    // print_game_state(&g_game); // 초기 화면 출력
    
    while (1) {
        // 큐에 아무것도 없으면 메인 스레드는 여기서 기절함. CPU 안 씀.
        GameEvent ev = queue_pop(&g_queue);
        
        // 큐에서 꺼낸 이벤트로 게임 상태 갱신
        game_apply_event(&g_game, ev);
        
        // 상태가 바뀌었으니 화면 갱신 (보통 TICK마다 화면을 그림)
        if (ev.type == EV_TICK || ev.type == EV_P1_LEFT || ev.type == EV_P1_RIGHT) {
            // print_game_state(&g_game); 화면갱신
        }

        
    }

    return 0;
}