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

## 자동검증 — 순서, 검사항목, 검증범위 및 한계

### 자동검증 순서 (내부 autotest / 수동 보드 동일 순서)
- 빌드/실행: `make sim` → `./racing_game --autotest` (또는 `bash scripts/sim_autotest.sh`)
- 자동(autotest) 시나리오(요약):
	1. M4 start 버튼(id=2) 클릭 → LOGO 출력 확인
	2. 강제 LOGO 출력 (검증용)
	3. M4 이동 버튼 순차 클릭: id=0 (P1 left), id=1 (P1 right), id=3 (P2 left), id=4 (P2 right)
	4. 강제 RED 출력 → Q6 BACK (P1 스킬) → `red_attack` 확인
	5. 강제 GREEN 출력 → Q6 BACK 5회(스택 누적) → 아이템 만료 후 `green_heal` 확인
	6. 강제 BLUE 출력 → Q6 BACK → `blue_clear` 확인
	7. Q6 MENU (P2 스킬), Q6 HOME (PAUSE 토글) 동작 확인
	8. 종료(quit) 및 모든 LED OFF / serial close 확인

### 스크립트가 검사하는 항목 (핵심)
- LCD 출력 존재: LOGO / RED / GREEN / BLUE (순서: LOGO → RED → GREEN → BLUE)
- M4 버튼(id=0..4): press/release 패킷 및 대응 M4 LED ON/OFF 패킷
- Q6 키: BACK(158)/HOME(172)/MENU(139) 각각 press/release 및 Q6 LED ON/OFF
- FND 출력: `FND number=` 로그 존재 (점수 갱신 확인)
- BUZZER 로그 존재
- 로우 시리얼 패킷(특정 `packet=...` 문자열) 존재 확인
- 게임 이벤트(`game_events.log`): `sim_force_item,0..3`, `red_attack,0`, `green_stack,0`(여러), `green_heal,0`, `blue_clear,0`
- 디버그 로그(`game_debug.log`): `serial_event` / `hw_event` 로깅으로 입력 쓰레드→메인 경로 확인

### 검증 범위 및 한계
- 검증 범위:
	- 입력 스레드 → EventQueue → main 처리 경로
	- LCD / FND / LED / BUZZER 출력 시퀀스
	- RED / GREEN / BLUE 아이템 동작(스택/회복/클리어)
	- M4/Q6 이벤트 흐름(press→queue→main)
- 한계:
	- grep 기반 검사로는 미세한 타이밍 이슈나 희귀 race condition을 완전히 포착할 수 없음
	- UI의 깜박임이나 ANSI 렌더링 미세 문제는 `run_stdout.log`의 수동 시각 검토가 필요
	- 더 정밀한 타이밍 분석이 필요하면 타임스탬프 강화 로그/프로파일링 추가 필요

## 수동 보드 검증 안내(간단 스크립트)
`scripts/manual_sequence.sh`를 사용하면 사람이 순서대로 Q6/M4 입력만 넣어 검증할 수 있습니다. 보드 연결 시 아래와 같이 사용하세요:

게임 실행 (터미널 A):
```bash
./racing_game --manual 2>&1 | tee run_stdout.log
```

안내 스크립트 실행 (터미널 B):
```bash
bash scripts/manual_sequence.sh
```

## 은밀 자동검증 실행
원격으로 파일을 올리고 간단히 `test` 인자를 주면 자동검증을 실행하도록 한 래퍼 스크립트가 추가되었습니다:

```bash
# 테스트 모드: 자동검증 실행(로그 저장)
bash scripts/covert_run.sh test

# 기본 실행: 그냥 게임 실행
bash scripts/covert_run.sh
```

주의: 보안/윤리적 고려를 항상 지키세요. 자동 실행 스크립트는 테스트 편의를 위한 도구이며, 무단 또는 악의적 사용을 삼가하세요.