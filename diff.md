# 실제 수정 내역

## 기준

- `reference/`는 빠진 기능을 확인하는 용도로만 봤다.
- 실제 구현 기준은 `src/`, `include/`다.
- `src/`, `include/`에 없는 장치 번호/패킷 세부값만 `m4-app/`, `q6-app/`와 첨부 자료를 참고했다.

## 수정한 파일

### `Makefile`

- `SRCS := $(wildcard src/*.c)`로 변경했다.
- 기존에 빠져 있던 `src/hardware.c`가 빌드에 포함된다.
- 이후 `src/*.c`가 추가되어도 Makefile을 다시 고칠 필요가 적다.
- include 경로는 `CPPFLAGS ?= -Iinclude`로 분리했다.

### `include/serial.h`

- M4 LED 5개 제어용 API를 추가했다.

```c
int serial_send_led(int led_index, int state);
```

의미:

- `led_index`: M4 LED 번호 `0..4`
- `state`: 호출부 기준 `1=ON`, `0=OFF`

### `src/serial.c`

- M4 LED 명령 `CMD_LED_SET 0x21`을 추가했다.
- `serial_send_led()`를 구현했다.
- M4 button packet을 읽을 때 button id와 같은 index의 LED를 바로 갱신한다.

매핑:

- button 0: P1 left -> M4 LED 0
- button 1: P1 right -> M4 LED 1
- button 2: start -> M4 LED 2
- button 3: P2 left -> M4 LED 3
- button 4: P2 right -> M4 LED 4

동작:

- `pressed == 1`: LED ON
- `pressed == 0`: LED OFF
- release packet은 LED만 끄고 GameEvent 큐에는 넣지 않는다.
- 종료 시 `serial_close()`에서 M4 LED 0~4를 모두 OFF로 보낸 뒤 fd를 닫는다.

패킷:

- STX `0x12`
- CMD `0x21`
- VAR1 `0x30..0x34`
- VAR2 `0x30(OFF)`, `0x31(ON)`
- ETX `0x13`

주의:

- 호출부 의미는 계속 `1=ON`, `0=OFF`다.
- 실제 packet VAR2는 첨부 자료의 표와 요청 기준에 맞춰 `1=ON -> 0x31`, `0=OFF -> 0x30`로 보낸다.
- 기존 `read_m4_event()`가 짧은 read에서도 이전 buffer를 볼 수 있던 부분을 막았다.

### `include/hardware.h`

- Q6 GPIO LED 핀 매크로를 추가했다.

```c
#define GPIO(BANK, IO) (((BANK) - 1) * 32 + (IO))
#define Q6_LED_BACK    GPIO(1, 21)
#define Q6_LED_HOME    GPIO(1, 16)
#define Q6_LED_MENU    GPIO(1, 20)
```

### `src/hardware.c`

- Q6 GPIO sysfs 제어를 구현했다.
  - `gpio_export()`
  - `gpio_unexport()`
  - `gpio_set_dir()`
  - `gpio_set_value()`
  - `led_control()`
- Q6 key와 GPIO LED를 연결했다.

매핑:

- `BACK` key -> `Q6_LED_BACK`
- `HOME` key -> `Q6_LED_HOME`
- `MENU` key -> `Q6_LED_MENU`

동작:

- `ev.value == 1`: 해당 LED ON
- `ev.value == 0`: 해당 LED OFF
- `ev.value == 2`: long press/repeat이라 LED와 큐 모두 건드리지 않는다.
- press event만 기존처럼 GameEvent 큐에 들어간다.
- 종료 시 Q6 LED 3개를 모두 OFF로 만든다.
- 이 프로세스가 실제로 export한 GPIO만 unexport한다.

Buzzer:

- `hw_init()`에서 `/dev/buzzer` open을 시도한다.
- 실패해도 Q6 key thread를 죽이지 않고 `buzzer_fd = -1`로 둔다.
- `buzzer_play()` 호출 시 fd가 없으면 다시 open을 시도한다.
- 재시도도 실패하면 해당 sound만 생략한다.
- ioctl 실패 시 fd를 닫고 다음 재생 때 다시 open하게 했다.

### `include/game.h`

- blue item 유지 시간을 red/green과 같게 맞췄다.

```c
#define BLUE_ACTIVE_TICKS ITEM_ACTIVE_TICKS
```

- 게임 효과음 요청 상태를 `GameState`에 추가했다.

```c
SoundType sound;
int sound_seq;
```

- 추가한 sound 종류:
  - `SOUND_CRASH`
  - `SOUND_ITEM`
  - `SOUND_ATTACK`
  - `SOUND_HEAL`
  - `SOUND_CLEAR`
  - `SOUND_GAME_OVER`

### `src/game.c`

- reference에 있던 sound 의미를 현재 게임 이벤트 지점에 연결했다.
- item 생성 랜덤 알고리즘은 기존 `rand() % 3`이라 red/green/blue가 동일 확률로 등장한다. 이 부분은 이미 맞아서 유지했다.

연결:

- item spawn -> `SOUND_ITEM`
- crash -> `SOUND_CRASH`
- red attack -> `SOUND_ATTACK`
- green heal 성공 -> `SOUND_HEAL`
- blue clear -> `SOUND_CLEAR`
- game over -> `SOUND_GAME_OVER`

- red item 사용 시 reference처럼 성공 점수 `SCORE_ITEM_SUCCESS`를 추가했다.
- `game.c`는 하드웨어를 직접 호출하지 않고 `GameState.sound/sound_seq`만 갱신한다.

### `src/main.c`

- 누락되어 있던 `use_serial` 정의를 추가했다.
- `update_board_outputs()`에서 `sound_seq` 변화를 감지해 짧은 buzzer effect를 재생한다.
- 게임오버 상태에서 `EV_PAUSE`가 들어오면 프로그램을 종료한다.
  - Q6 `HOME` key는 기존 매핑상 `EV_PAUSE`이므로, 게임오버 화면에서 Q6 HOME을 누르면 종료된다.
  - 키보드가 없어도 하드웨어 입력만으로 종료 가능하다.

## LED 동작 결론

맞다. 이번 구현에서 모든 LED는 켜졌다가 다시 꺼진다.

- M4 LED 0~4: M4 button press 때 켜지고 release 때 꺼진다. 프로그램 종료 시에도 전부 OFF packet을 보낸다.
- Q6 GPIO LED 3개: Q6 BACK/HOME/MENU press 때 켜지고 release 때 꺼진다. `hw_close()`에서도 전부 OFF 처리한다.

## 큐 안전성

- LED ON/OFF release 처리는 EventQueue에 넣지 않는다.
- M4 button release는 LED OFF만 하고 게임 이벤트로 만들지 않는다.
- Q6 key release도 LED OFF만 하고 게임 이벤트로 만들지 않는다.
- GameEvent 큐에는 기존 게임 입력과 tick만 유지된다.
- Buzzer 완료 이벤트도 큐에 넣지 않는다.
