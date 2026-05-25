# 2인용 대전 레이싱 게임

## 프로젝트 개요
- hybus TKU 보드 입력 기반의 2인용 대전 레이싱 게임.
- Tera Term 터미널의 ASCII 아트 화면을 게임 GUI로 사용.
- 입력 생성 스레드와 상태 갱신 스레드를 분리한 이벤트 기반 구조.
- GameState 변경은 main thread에서만 수행하는 single-writer 설계.
- 시뮬레이터 모드에서는 보드 없이 Ubuntu VM에서 동작을 검증할 수 있다.

## 게임 규칙
- 한 플레이어의 life가 0이 되면 게임 종료.
- 종료 시 winner를 표시.
- 아이템은 빨강/초록/파랑이 동일한 랜덤 생성 경로를 사용한다.
- 아이템 유지 시간은 공통 지속 시간(`ITEM_ACTIVE_TICKS`)을 사용한다.
- 빨강은 즉시 공격, 초록은 스택 누적 후 종료 시 회복, 파랑은 해당 플레이어의 돌을 제거한다.

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

## Ubuntu VM 빌드 환경
이 프로젝트는 Ubuntu VM에서 `gcc`와 `make`로 빌드한다. 보드가 없을 때는 시뮬레이터 모드로 먼저 검증한다.

필수 패키지:
```bash
sudo apt update
sudo apt install -y build-essential make
```

## 빌드 방법
보드용 빌드:
```bash
make
```

시뮬레이터 빌드:
```bash
make sim
```

## 실행 방법
보드용 실행:
```bash
./racing_game
```

시뮬레이터 수동 실행:
```bash
./racing_game --manual
```

실행 예시에서 `2>&1 | tee run_stdout.log`를 붙이는 이유:
- `2>&1`은 표준 에러(stderr)를 표준 출력(stdout)으로 합친다.
- `tee`는 출력 내용을 화면에 보여주면서 동시에 파일(`run_stdout.log`)에도 저장한다.
- 즉, 게임 자체의 옵션이 아니라 셸에서 로그를 남기기 위한 방법이다.
- 이 부분을 빼면 게임은 정상 실행되지만, 터미널에 나온 에러/렌더 로그를 파일로 남기지 못한다.

예시:
```bash
./racing_game --manual 2>&1 | tee run_stdout.log
```

시뮬레이터 자동 검증:
```bash
./racing_game --autotest
```

자동 검증 스크립트:
```bash
bash scripts/sim_autotest.sh
```

이 스크립트는 `make sim`으로 시뮬레이터를 빌드하고 `--autotest`를 실행한 뒤 `sim_serial.log`, `game_events.log`, `game_debug.log`, `run_stdout.log`를 grep으로 검증한다.

## 시뮬레이터 조작 키
- `s`: 시작
- `p`: 일시정지
- `x`: 종료
- `a` / `d` / `q`: P1 좌/우/스킬
- `j` / `l` / `o`: P2 좌/우/스킬

## 로그 파일 스키마
시뮬레이터/게임 실행 후 아래 4가지 로그를 확인하면 된다.

### 1) `game_events.log`
게임 로직 이벤트를 시간순으로 저장한다.

형식:
```text
timestamp,event,value
```

예시 이벤트:
- `start,0`: 게임 시작
- `item_spawn,<item>`: 아이템 스폰
- `red_attack,<player>`: 빨강 아이템 사용
- `green_stack,<player>`: 초록 스택 증가
- `green_heal,<player>`: 초록 회복 성공
- `blue_clear,<player>`: 파랑 돌 제거
- `crash,<player>`: 충돌
- `game_over,<winner>`: 게임 종료

읽는 법:
- `timestamp`는 내부 틱 카운트 기준이다.
- `event`는 발생한 게임 이벤트 이름이다.
- `value`는 해당 이벤트의 부가값이다.
	- `item_spawn`에서는 아이템 종류(`ITEM_RED=1`, `ITEM_GREEN=2`, `ITEM_BLUE=3`)를 뜻한다.
	- `red_attack`, `green_stack`, `green_heal`, `blue_clear`, `crash`에서는 플레이어 인덱스(`0=P1`, `1=P2`)를 뜻한다.
	- `game_over`에서는 승자 인덱스(`0=P1`, `1=P2`, `-1=무승부`)를 뜻한다.

### 2) `game_debug.log`
디버그 메시지와 상태 변화를 남긴다.

형식:
```text
[unix_time] message
```

읽는 법:
- `main_event` 라인은 입력 이벤트가 게임 상태에 반영된 시점이다.
- `item spawn`, `red attack`, `green stack`, `green heal`, `blue clear`, `crash`, `game over` 메시지로 주요 판정을 확인한다.
- 시뮬레이터에서는 `sim_lcd=...`, `sim_fnd=...`도 함께 기록된다.

### 3) `sim_serial.log`
시뮬레이터의 보드 출력 대체 로그다.

형식:
```text
[SIM][LCD] preset=0 label=LOGO packet=12 24 30 30 13
[SIM][M4_BUTTON_RX] packet=12 22 30 31 13 id=0 pressed=1
[SIM][M4_LED] index=0 state=ON packet=12 21 30 31 13
[SIM][Q6_KEY_RX] code=158 pressed=1
[SIM][Q6_LED] pin=21 state=ON
[SIM][FND] number=120 digits=120
[SIM][BUZZER] freq=659.00 time_us=60000
```

읽는 법:
- `LCD preset=0..3`는 현재 LCD 상태를 뜻한다.
	- `0=LOGO`, `1=RED`, `2=GREEN`, `3=BLUE`
- `FND number=...`는 점수 표시값이다.
- `M4_BUTTON_RX`는 M4 버튼 수신 패킷을 뜻한다.
	- `id=0..4`: P1 left, P1 right, start, P2 left, P2 right
	- `pressed=1`은 press, `pressed=0`은 release다.
- `M4_LED`는 M4 LED 출력 패킷을 뜻한다.
	- 호출 의미는 `1=ON`, `0=OFF`다.
	- 패킷의 VAR2는 `0x31=ON`, `0x30=OFF`다.
- `Q6_KEY_RX`는 Q6 BACK/HOME/MENU 입력을 뜻한다.
	- `158=BACK`, `172=HOME`, `139=MENU`
- `Q6_LED`는 Q6 GPIO LED 출력을 뜻한다.
	- `pin=21 BACK`, `pin=16 HOME`, `pin=20 MENU`
- `BUZZER`는 게임 상황별 부저 출력 대체 로그다.
- 필요하면 `SIM_SERIAL_VERBOSE=1`을 주고 실행하면 raw serial write도 기록한다.

### 4) `run_stdout.log`
실행 중 화면 렌더와 표준 출력 전체를 저장한 파일이다.

형식:
```text
ANSI escape + ASCII UI + status line
```

읽는 법:
- 화면이 주기적으로 갱신되는지 확인한다.
- 게임 오버 이후에도 출력이 계속 갱신되는지, 특정 이벤트 직후 화면이 정상적으로 바뀌는지 확인한다.

## 권장 검증 절차
1. `make sim`
2. `./racing_game --manual 2>&1 | tee run_stdout.log`
3. 아이템 스폰을 기다린 뒤 빨강/초록/파랑을 각각 사용해 본다.
4. 종료 후 `game_events.log`, `game_debug.log`, `sim_serial.log`, `run_stdout.log`를 비교한다.

완전 자동 검증:
```bash
bash scripts/sim_autotest.sh
```

자동 검증은 다음을 확인한다.
- LCD가 `LOGO -> RED -> GREEN -> BLUE` 순서로 출력되는지
- M4 button 0~4 press/release가 각각 M4 LED 0~4를 켰다가 끄는지
- M4 button 이벤트가 `serial_thread -> EventQueue -> main thread` 경로로 들어오는지
- Q6 BACK/HOME/MENU press/release가 각각 GPIO LED를 켰다가 끄는지
- Q6 key 이벤트가 `hw_thread -> EventQueue -> main thread` 경로로 들어오는지
- red/green/blue item 처리 결과가 `game_events.log`에 남는지
- buzzer, FND, LCD, LED 패킷/대체 로그가 `sim_serial.log`에 남는지

## 로그 해석 요약
- 화면에서 아이템이 사라졌는데 `blue_clear`가 없으면, 타이머 만료로 사라진 것이다.
- `blue_clear`가 있으면 파랑 스킬로 제거된 것이다.
- `sim_lcd=3`는 파랑 아이템 표시가 실제로 전송되었음을 뜻한다.
- `green_stack`가 누적된 뒤 `green_heal`이 나오면 초록 아이템이 정상 동작한 것이다.
- `red_attack` 뒤에 상대방 돌이 추가되고 충돌하면 빨강 공격이 정상 동작한 것이다.
- `sim_force_item,0/1/2/3`은 자동 검증에서 LCD LOGO/RED/GREEN/BLUE 상태를 강제로 만든 기록이다.
- `serial_event type=EV_START` 같은 로그가 있으면 M4 입력이 큐를 거쳐 main thread까지 들어온 것이다.
- `hw_event type=EV_P1_SKILL` 같은 로그가 있으면 Q6 입력이 큐를 거쳐 main thread까지 들어온 것이다.

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
- serial_thread/hw_thread
- serial/hardware 함수는 simulator stub로 동작하지만 입력은 내부 queue를 거쳐 실제 thread 경로로 main queue에 들어간다.

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
./racing_game --manual
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
