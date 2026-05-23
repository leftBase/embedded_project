# 2인용 대전 레이싱 게임

## 프로젝트 개요
- hybus TKU 보드 입력 기반의 2인용 대전 레이싱 게임.
- Tera Term 터미널의 ASCII 아트 화면을 게임 GUI로 사용.
- 입력 생성 스레드와 상태 갱신 스레드를 분리한 이벤트 기반 구조.
- GameState 변경은 main thread에서만 수행하는 single-writer 설계.

## 게임 규칙
- 한 플레이어의 life가 0이 되면 게임 종료.
- 종료 시 winner를 표시.

## 입력/출력 장치 매핑
입력:
- Q6 보드 back/home/menu 키: P1 스킬, pause, P2 스킬
- M4 보드 LED 버튼: P1 좌우 이동, 시작 버튼, P2 좌우 이동
- 키보드 입력: PC 테스트용 디버그 입력(최종 보드 제출 시 제외 가능)

출력:
- LCD: 스킬 발동 아이템 모드 표시
- FND: P1/P2 점수 교차 표시
- Buzzer: 게임 상태별 사운드 출력
- 터미널: ASCII 아트 게임 화면 렌더링

## 아키텍처
핵심 키워드:
- Event-driven architecture
- Producer-Consumer pattern
- Single-writer principle
- Fixed timestep game loop
- Hardware abstraction
- Deterministic update order

설계 요약:
- 모든 입력/주기 신호를 GameEvent로 정규화.
- 생산자 스레드가 EventQueue에 push.
- main thread만 queue_pop 후 game_apply_event를 실행.
- EV_TICK 처리 시 game_tick으로 게임 규칙 순서를 고정 실행.

## 스레드 구조
실행 모드에 따라 구성이 다르다.

보드 실행(make):
- main thread: queue_pop, game_apply_event, render_game
- timer_thread: 50ms 주기 EV_TICK 생성
- serial_thread: M4 UART 이벤트 수집 후 큐 push
- hw_thread: Q6 버튼 이벤트 수집 후 큐 push
- keyboard_thread: 테스트용 키보드 입력 수집 후 큐 push

시뮬레이터 실행(make sim):
- main thread
- timer_thread
- keyboard_thread(autotest 비활성 시)
- autotest_thread(autotest 활성 시, 테스트 이벤트 자동 주입)
- serial/hardware는 simulator stub로 동작

## 이벤트 처리 흐름
Q6 버튼 / M4 UART / 키보드(또는 autotest)
-> 입력 스레드에서 GameEvent 생성
-> EventQueue push
-> main thread queue_pop
-> game_apply_event
-> EV_TICK이면 game_tick
-> 아이템 타이머 -> rock 생성 -> rock 이동 -> 충돌 판정 -> 점수 갱신 -> 게임오버 판정
-> render_game

## 실행 방법
기본(보드 대상):
```bash
make
./racing_game
```

시뮬레이터 빌드:
```bash
make sim
./racing_game
```

자동 테스트 모드:
```bash
./racing_game --autotest
```

수동 입력 모드:
```bash
./racing_game --manual
```

키보드 테스트 키:
- a/d/q: P1 좌/우/스킬
- s/p: 시작/일시정지
- j/l/o: P2 좌/우/스킬
- x: 종료

## 발표용 한 줄 요약
본 프로젝트는 멀티스레드 환경에서 입력 생성과 상태 갱신을 분리하고, GameState를 main thread 단일 작성자로 제한해 race condition을 줄인 이벤트 기반 대전 레이싱 게임이다.