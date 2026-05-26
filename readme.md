# Racing Game

임베디드 보드에서 실행하는 2인용 레인 회피 게임입니다.
Q6 보드 버튼과 M4 UART 버튼 입력을 이벤트 큐로 모으고, 메인 루프가 게임 상태를 갱신합니다. 터미널 화면은 `/dev/tty0`에 렌더링하고, LCD/FND/LED/부저는 각각의 디바이스 출력으로 제어합니다.

## 핵심 방향

이 코드는 보드에서 안정적으로 도는 것을 우선합니다.

- 입력은 모두 이벤트로 변환해서 `EventQueue`에 넣습니다.
- 게임 로직은 `GameState` 하나를 중심으로 갱신합니다.
- 메인 루프는 오래 막히면 안 됩니다.
- 느린 출력인 터미널 렌더링은 낮은 주기로 제한합니다.
- 부저는 별도 사운드 스레드에서 재생해 게임 루프를 멈추지 않습니다.
- 디버그 파일 로그는 실행 환경변수로 끌 수 있습니다.

## 파일 구조

```text
include/
  debug.h      디버그 로그 인터페이스
  event.h      이벤트 타입과 이벤트 큐
  game.h       게임 상태, 규칙 상수, 사운드/LCD 타입
  hardware.h   Q6 버튼, GPIO LED, 부저 제어
  render.h     터미널 렌더링
  serial.h     M4 UART 통신

src/
  debug.c      game_debug.log 기록
  event.c      pthread mutex/cond 기반 이벤트 큐
  game.c       게임 규칙과 상태 갱신
  hardware.c   /dev/input/event1, /dev/buzzer, /sys/class/gpio
  main.c       스레드 생성, 메인 루프, 보드 출력 갱신
  render.c     ANSI 터미널 렌더링
  serial.c     /dev/ttymxc1 UART 패킷 송수신

Makefile       빌드, 실행, 정리
```

## 빌드

```sh
make
```

기본 결과물은 `racing_game`입니다.

주요 Make 변수:

```sh
make CC=arm-linux-gcc
make TARGET=my_game
```

기본 컴파일 플래그:

```sh
-Wall -Wextra -std=c99 -D_DEFAULT_SOURCE -DENABLE_DEBUG_LOG
```

`-DENABLE_DEBUG_LOG`가 켜져 있으므로 디버그 로그 코드는 빌드에 포함됩니다. 실제 로그를 쓸지는 실행 시 `GAME_DEBUG`가 결정합니다.

## 실행

기본 실행:

```sh
make run
```

`make run`은 내부적으로 다음 명령을 실행합니다.

```sh
GAME_DEBUG=0 ./racing_game > /dev/tty0
```

즉, 디버그 파일 로그를 끄고 터미널 렌더링을 `/dev/tty0`로 보냅니다.

다른 TTY에 출력하고 싶으면:

```sh
make run RUN_TTY=/dev/tty1
```

직접 실행할 수도 있습니다.

```sh
GAME_DEBUG=0 ./racing_game > /dev/tty0
GAME_DEBUG=1 ./racing_game > /dev/tty0
```

현재 게임 자체는 명령행 옵션을 사용하지 않습니다.

## `2>&1` 의미

리눅스/셸에서 숫자 파일 디스크립터는 보통 다음 뜻입니다.

- `0`: 표준입력(stdin)
- `1`: 표준출력(stdout)
- `2`: 표준에러(stderr)

`2>&1`은 **표준에러 2번을 표준출력 1번이 향하는 곳으로 합친다**는 뜻입니다.

예:

```sh
./racing_game > run.log 2>&1
```

위 명령은 일반 출력과 에러 출력을 모두 `run.log`에 저장합니다. 순서가 중요합니다. `> run.log`로 stdout을 먼저 파일로 보낸 뒤, `2>&1`로 stderr도 같은 파일에 붙입니다.

현재 `make run`에는 `2>&1`을 붙이지 않았습니다. 게임 화면만 `/dev/tty0`로 보내고, 에러는 기존 stderr로 남깁니다. 에러까지 TTY로 합치고 싶다면 Makefile의 run 명령을 다음처럼 바꿀 수 있습니다.

```sh
GAME_DEBUG=0 ./$(TARGET) > $(RUN_TTY) 2>&1
```

## 디버그 로그

`debug.c`는 `game_debug.log`에 이벤트와 상태 로그를 남길 수 있습니다.

동작 조건:

- 빌드에 `-DENABLE_DEBUG_LOG`가 들어가 있어야 합니다.
- 실행 시 `GAME_DEBUG=0`이면 로그를 쓰지 않습니다.
- `GAME_DEBUG`가 없거나 `0`이 아니면 `game_debug.log`를 append 모드로 엽니다.

보드 실행 중에는 파일 I/O와 flush가 부담이 될 수 있으므로 기본 실행은 `GAME_DEBUG=0`입니다.

## 사용하는 디바이스

### Q6 입력

파일:

```text
/dev/input/event1
```

키 매핑:

| Q6 키 | Linux key code | 게임 이벤트 |
| --- | ---: | --- |
| BACK | 158 | P1 skill |
| HOME | 172 | pause / game over 상태에서는 종료 |
| MENU | 139 | P2 skill |

### Q6 LED

GPIO sysfs:

```text
/sys/class/gpio
```

LED 매핑:

| Q6 키 | GPIO |
| --- | ---: |
| BACK LED | GPIO(1, 21) |
| HOME LED | GPIO(1, 16) |
| MENU LED | GPIO(1, 20) |

Q6 LED는 키 press에서 켜지고 release에서 꺼집니다.

### Q6 부저

파일:

```text
/dev/buzzer
```

사용 ioctl:

- `IOCTL_SET_TONE`
- `IOCTL_SET_VOLUME`
- `IOCTL_START_BUZZER`
- `IOCTL_END_BUZZER`

부저 재생은 `main.c`의 사운드 큐에 요청을 넣고, 별도 사운드 스레드가 `buzzer_play()`를 호출합니다.

### M4 UART

파일:

```text
/dev/ttymxc1
```

UART 설정:

- 115200 baud
- 8-bit
- `CLOCAL`
- `CREAD`
- `CRTSCTS`
- `VMIN = 5`
- `VTIME = 0`

패킷 형식:

```text
0x12, command, arg1, arg2, 0x13
```

명령:

| command | 의미 |
| ---: | --- |
| `0x21` | M4 LED set |
| `0x22` | M4 button event |
| `0x23` | FND set |
| `0x24` | LCD set |

M4 버튼 매핑:

| M4 button id | 게임 이벤트 |
| ---: | --- |
| 0 | P1 left |
| 1 | P1 right |
| 2 | start |
| 3 | P2 left |
| 4 | P2 right |

M4 LED는 같은 button id와 1:1로 매칭됩니다. press 패킷이면 켜고 release 패킷이면 끕니다. release는 게임 이벤트로는 넣지 않고 LED 상태만 갱신합니다.

LCD preset:

| 값 | 의미 |
| ---: | --- |
| 0 | RED |
| 1 | GREEN |
| 2 | BLUE |
| 3 | LOGO |

아이템과 LCD 매핑:

| 아이템 | LCD |
| --- | --- |
| RED | RED |
| GREEN | GREEN |
| BLUE | BLUE |
| NONE | LOGO |

## 스레드 구조

전체 흐름은 **여러 producer thread가 이벤트를 push하고, main thread 하나가 이벤트를 pop해서 게임 상태를 바꾸는 구조**입니다.

```text
timer thread    -> EV_TICK --------------------+
serial thread   -> M4 button GameEvent --------+-> EventQueue -> main thread -> GameState
hardware thread -> Q6 key GameEvent -----------+

main thread -> SoundQueue -> sound thread -> /dev/buzzer
main thread -> LCD/FND UART packet -> /dev/ttymxc1
main thread -> terminal render -> stdout (/dev/tty0 when make run)
```

입력 스레드들은 `GameState`를 직접 수정하지 않습니다. 입력 디바이스에서 온 신호를 `GameEvent`로 바꾸고 큐에 넣는 일만 합니다. 실제 게임 상태 변경은 main thread에서만 일어납니다.

### timer thread

함수:

```c
timer_thread()
```

역할:

1. `usleep(TICK_MS * 1000)`으로 50ms를 기다립니다.
2. `queue_push(&g_queue, EV_TICK, 0)`을 호출합니다.
3. 이 과정을 `running`이 1인 동안 반복합니다.

`EV_TICK`은 실제 시간이 게임 안으로 들어오는 통로입니다. 플레이어 입력은 "지금 뭘 눌렀는지"를 나타내고, tick은 "시간이 한 칸 지났다"를 나타냅니다.

### serial thread

함수:

```c
serial_thread()
serial_next_event()
read_m4_event()
map_m4_button_event()
```

디바이스:

```text
/dev/ttymxc1
```

흐름:

1. `serial_thread()`가 반복해서 `serial_next_event(&event)`를 호출합니다.
2. `serial_next_event()`는 `select()`로 UART fd에 읽을 데이터가 생길 때까지 기다립니다.
3. 데이터가 오면 `read_m4_event()`가 5바이트 패킷을 읽습니다.
4. 패킷 형식이 `0x12, command, arg1, arg2, 0x13`인지 검사합니다.
5. `command == CMD_M4_BUTTON(0x22)`인 버튼 패킷만 처리합니다.
6. `arg1`은 M4 button id, `arg2`는 press/release 상태입니다.
7. button id가 0~4이고 상태가 0 또는 1이면 `serial_send_led(button_id, pressed)`로 M4 LED를 바로 갱신합니다.
8. release 상태는 게임 이벤트로 만들지 않습니다. LED만 꺼지고 끝납니다.
9. press 상태면 `map_m4_button_event()`가 button id를 `GameEvent.type`으로 매핑합니다.
10. `serial_next_event()`가 1을 반환하면 `serial_thread()`가 `queue_push_event(&g_queue, event)`로 main thread에 전달합니다.

M4 입력 매핑:

| button id | press 이벤트 |
| ---: | --- |
| 0 | `EV_P1_LEFT` |
| 1 | `EV_P1_RIGHT` |
| 2 | `EV_START` |
| 3 | `EV_P2_LEFT` |
| 4 | `EV_P2_RIGHT` |

중요한 점은 **M4 LED 갱신은 serial thread에서 바로 하고, 게임 상태 변경은 main thread가 나중에 큐에서 pop한 뒤 한다**는 것입니다.

### hardware thread

함수:

```c
hw_thread()
hw_next_event()
update_q6_key_led()
map_q6_key_event()
```

디바이스:

```text
/dev/input/event1
```

흐름:

1. `hw_thread()`가 반복해서 `hw_next_event(&event)`를 호출합니다.
2. `hw_next_event()`는 `read(button_fd, &ev, sizeof(ev))`로 Linux input subsystem의 `struct input_event`를 읽습니다.
3. `input_event`에는 `type`, `code`, `value`가 들어 있습니다.
4. `type == EV_KEY`이고 `value`가 0 또는 1이면 `update_q6_key_led()`가 먼저 LED를 갱신합니다.
5. `value == 1`, 즉 press인 경우만 `map_q6_key_event()`가 게임 이벤트로 변환합니다.
6. release 상태는 게임 이벤트로 만들지 않습니다. LED만 꺼지고 끝납니다.
7. `map_q6_key_event()`가 유효한 키를 찾으면 `GameEvent.type`을 채우고 1을 반환합니다.
8. `hw_next_event()`가 1을 반환하면 `hw_thread()`가 `queue_push_event(&g_queue, event)`로 main thread에 전달합니다.

Q6 키 매핑:

| input_event code | 물리 키 | press 이벤트 |
| ---: | --- | --- |
| 158 | BACK | `EV_P1_SKILL` |
| 172 | HOME | `EV_PAUSE` |
| 139 | MENU | `EV_P2_SKILL` |

Q6 LED 매핑:

| 물리 키 | LED GPIO |
| --- | ---: |
| BACK | GPIO(1, 21) |
| HOME | GPIO(1, 16) |
| MENU | GPIO(1, 20) |

여기도 M4와 마찬가지로 **LED는 입력 스레드에서 즉시 갱신하고, 게임 상태는 큐를 통해 main thread에서 갱신**합니다.

### sound thread

함수:

```c
dispatch_pending_sound()
queue_sound()
sound_queue_push()
sound_thread()
sound_queue_pop()
buzzer_play()
```

디바이스:

```text
/dev/buzzer
```

흐름:

1. main thread가 `game_apply_event()`로 게임 상태를 바꿉니다.
2. 게임 로직 안에서 `request_sound()`가 호출되면 `GameState.sound`와 `GameState.sound_seq`가 바뀝니다.
3. main thread는 바로 `dispatch_pending_sound(&g_game, &last_sound_seq)`를 호출합니다.
4. `sound_seq`가 바뀌었으면 `queue_sound()`가 사운드 종류를 주파수와 시간으로 바꿉니다.
5. `sound_queue_push()`가 `SoundQueue`에 재생 요청을 넣습니다.
6. `sound_thread()`는 `sound_queue_pop()`으로 요청을 하나씩 꺼냅니다.
7. `buzzer_play(freq, time_us)`가 `/dev/buzzer`에 ioctl을 보내 실제 소리를 냅니다.

`buzzer_play()` 안에는 소리 길이만큼 `usleep()`이 있습니다. 이 함수가 main thread에서 직접 실행되면 게임 입력 처리가 멈추지만, 현재는 sound thread에서 실행되므로 main thread는 계속 이벤트를 처리할 수 있습니다.

중요 사운드인 충돌, 아이템 등장, 공격, 회복, blue clear는 큐에 대기 중인 이동음을 비우고 들어갑니다. 이미 재생 중인 소리는 끊지 않지만, 이동음이 뒤에 쌓여 중요한 소리를 늦추는 상황은 줄입니다.

### main thread

함수:

```c
main()
queue_pop()
game_apply_event()
dispatch_pending_sound()
update_board_outputs()
render_game()
```

흐름:

1. `queue_pop(&g_queue)`으로 이벤트를 하나 꺼냅니다.
2. `EV_QUIT`이면 종료합니다.
3. `GAME_OVER` 상태에서 `EV_PAUSE`가 오면 종료합니다.
4. 그 외 이벤트는 `game_apply_event(&g_game, ev)`로 넘깁니다.
5. `game_apply_event()`는 이벤트 종류에 따라 `GameState`를 갱신합니다.
6. 게임 로직이 사운드를 요청했을 수 있으므로 바로 `dispatch_pending_sound()`를 호출합니다.
7. pop한 이벤트가 `EV_TICK`이면 `update_board_outputs()`를 호출합니다.
8. tick 기반 렌더 카운터가 `RENDER_INTERVAL_TICKS`에 도달하면 `render_game()`을 호출합니다.
9. tick이 아닌 입력 이벤트는 `need_render = 1`로 표시해서 다음 렌더 타이밍에 화면을 갱신하게 합니다.

main thread의 핵심은 **pop -> game apply -> sound dispatch -> tick이면 board output/render**입니다.

## Tick의 역할

`EV_TICK`은 timer thread가 50ms마다 넣는 시간 이벤트입니다. main thread가 `EV_TICK`을 pop했을 때만 시간 기반 게임 처리가 일어납니다.

`EV_TICK`이 `game_apply_event()`로 들어가면 내부에서 `game_tick()`이 호출됩니다.

`game_tick()`이 담당하는 범위:

1. `tick_count` 증가
2. 난이도 갱신
3. 아이템 타이머 감소
4. 아이템 스폰
5. 돌 스폰
6. 돌 이동
7. 충돌 판정
8. 생존 점수 부여
9. 게임오버 판정

그 다음 main thread가 같은 `EV_TICK` 처리 안에서 보드 출력도 갱신합니다.

`update_board_outputs()`가 담당하는 범위:

1. LCD 값이 바뀌었으면 `serial_send_lcd()`로 M4 LCD preset 전송
2. 게임이 `GAME_RUNNING`이면 FND tick 카운터 증가
3. `FND_INTERVAL_TICKS`마다 P1/P2 점수를 번갈아 `serial_send_fnd_number()`로 전송

렌더링도 tick 기준입니다. 매 tick마다 화면을 그리지는 않고, `render_tick_count`가 `RENDER_INTERVAL_TICKS`에 도달했을 때만 `render_game()`을 호출합니다. 현재 설정은 `TICK_MS=50ms`, `RENDER_INTERVAL_TICKS=10`이므로 약 500ms마다 한 번 렌더링합니다.

정리하면:

```text
EV_TICK pop
  -> game_apply_event()
     -> game_tick()
        -> 게임 시간/돌/아이템/충돌/점수/게임오버 갱신
  -> dispatch_pending_sound()
  -> update_board_outputs()
     -> LCD/FND 갱신
  -> 필요하면 render_game()
```

입력 이벤트는 플레이어 이동, 스킬, 시작, 일시정지 같은 즉시 동작을 처리합니다. 시간의 흐름, 돌 이동, 아이템 지속시간, 생존 점수, 렌더 주기, FND 주기는 tick이 담당합니다.

## 게임 규칙

### 기본 상태

- 플레이어 수: 2명
- 각 플레이어 레인 수: 3개
- 시작 레인: 가운데 레인
- 시작 체력: 3
- 최대 체력: 5
- 돌 최대 수: 플레이어당 40개
- 도로 높이: 16칸

### 진행

게임은 `READY` 상태에서 start 입력을 받으면 `RUNNING`으로 시작합니다.

`EV_TICK`마다 다음 처리를 합니다.

1. tick 증가
2. 난이도 갱신
3. 아이템 타이머 감소
4. 아이템 스폰
5. 돌 스폰
6. 돌 이동
7. 충돌 판정
8. 생존 점수 부여
9. 게임오버 판정

### 난이도

- 기본 돌 이동 주기: 8틱
- 최소 돌 이동 주기: 5틱
- 난이도 단계: 200틱마다 상승
- 기본 돌 스폰 주기: 14틱
- 스폰 확률: 25%에서 시작해 단계마다 증가, 최대 70%

### 충돌

돌이 플레이어 위치 근처까지 내려왔고 같은 레인에 있으면 충돌합니다.

충돌 효과:

- 체력 -1
- 점수 -30
- 점수는 0 아래로 내려가지 않음
- 체력이 0이면 해당 플레이어 사망

한 명이라도 사망하면 게임은 `GAME_OVER`가 됩니다.

### 점수

| 조건 | 점수 |
| --- | ---: |
| 생존 보너스 | 20틱마다 +10 |
| 돌 회피 | +20 |
| 아이템 성공 | +30 |
| 충돌 | -30 |

### 아이템

아이템은 100틱마다 하나씩 등장합니다. 이미 아이템이 있으면 새 아이템은 스폰하지 않습니다.

| 아이템 | 효과 |
| --- | --- |
| RED | 상대 플레이어 쪽에 랜덤 돌 생성 |
| GREEN | 스킬 버튼을 누를 때마다 개인 스택 증가 |
| BLUE | 자기 쪽 돌 전체 제거 |

GREEN은 아이템 시간이 끝날 때 판정합니다. 스택이 5 이상이면 체력을 1 회복하고 점수를 얻습니다.

## 사운드

| 이벤트 | 느낌 | 구현 |
| --- | --- | --- |
| 이동 | 플레이어/방향별 음높이 상승 | P1 left < P1 right < P2 left < P2 right |
| 아이템 등장 | 짧은 알림음 | 659Hz |
| RED 공격 | 공격음 | 523Hz |
| GREEN 회복 | 높은 회복음 | 784Hz |
| BLUE clear | 뾰롱 | 659Hz 후 988Hz |
| 충돌 | 뿌국 | 147Hz 후 98Hz |

중요 사운드인 충돌, 아이템, 공격, 회복, clear는 사운드 큐에 남아 있는 대기음을 비우고 들어갑니다. 이동음이 너무 많이 쌓여 중요한 소리를 늦추는 상황을 줄이기 위해서입니다.

## 렌더링

터미널 렌더링은 ANSI escape sequence를 사용합니다.

- 시작 시 화면 clear 및 cursor hide
- 렌더 시 cursor home
- 종료 시 cursor show 및 reset

렌더 주기:

```text
TICK_MS = 50ms
RENDER_INTERVAL_TICKS = 10
```

즉, 약 500ms마다 한 번 렌더링합니다. 보드 UART/TTY 출력 대역폭을 아끼기 위해 낮은 주기로 제한합니다.

## 종료

게임오버 상태에서 Q6 HOME, 즉 `EV_PAUSE`가 들어오면 종료합니다.

종료 순서:

1. `running = 0`
2. 이벤트 큐와 사운드 큐 close
3. timer/sound thread join
4. serial/hardware 디바이스 close
5. serial/hardware thread join
6. 사운드 큐 destroy
7. render shutdown
8. debug close

종료 시 별도의 게임 이벤트 파일 저장은 하지 않습니다. 보드에서 종료 경로가 파일 I/O 때문에 느려지는 것을 피하기 위해서입니다.

## 정리

```sh
make clean
```

오브젝트 파일과 실행 파일을 삭제합니다.
