# src/include 기준 누락 기능 정리 및 수정 계획

## 기준

- `reference/`는 기능 구현 여부만 확인하는 용도다.
- 파일 디스크립터, 패킷 규칙, UART 설정, 디바이스 경로, 기존 스레드/큐 구조는 `src/`, `include/`를 우선한다.
- `src/`, `include/`에 없는 장치 세부값만 `m4-app/`, `q6-app/` 샘플을 보조 기준으로 쓴다.
- 이번 작업에서 실제 수정한 파일은 `Makefile`, `diff.md`뿐이다. 아래의 `src/`, `include/` 변경은 다음 구현 단계에서 적용할 상세 설계다.

## 현재 장치 범위

- Q6 hardware: buzzer, GPIO LED 3개, key 3개(`BACK`, `HOME`, `MENU`)
- M4 serial/UART: FND, LCD, LED 5개, button 5개

## 현재 구현/누락 상태

| 기능 | 기준 위치 | 현재 상태 | 수정 필요 |
| --- | --- | --- | --- |
| Q6 key 3개 | `include/hardware.h`, `src/hardware.c` | `BACK/HOME/MENU`를 `EV_P1_SKILL/EV_PAUSE/EV_P2_SKILL`로 매핑함 | 유지. 단, buzzer 실패 때문에 key thread까지 죽지 않게 `hw_init()` 독립 초기화 필요 |
| Q6 buzzer | `src/hardware.c`, `q6-app/buzzer.c` | `/dev/buzzer` open, ioctl play 함수는 있음 | reference의 `SOUND_*` 의미와 게임 이벤트 연결 없음. blocking `usleep()`을 main/event queue에서 직접 호출하지 않도록 비동기화 필요 |
| Q6 GPIO LED 3개 | `include/hardware.h`, `q6-app/gpio_led.c` | `led_control()` 선언만 있고 구현 없음 | sysfs GPIO export/direction/value 구현 필요 |
| M4 button 5개 | `src/serial.c`, `m4-app/button.c` | `0x22` 패킷 수신, button id 0~4 매핑 구현됨 | 유지 |
| M4 FND | `src/serial.c`, `m4-app/fnd.c` | `0x23` 송신, 3자리 숫자 송신 구현됨 | 유지 |
| M4 LCD | `src/serial.c`, `m4-app/lcd.c` | `0x24` 송신, preset 0~3 구현됨 | 유지 |
| M4 LED 5개 | `src/serial.c` 주석, `m4-app/led.c` | 명령 주석만 있고 함수 없음 | `serial_send_led(int led_index, int state)` 추가 필요 |
| reference sound 기능 | `reference/device.h`, `reference/game.c` | `SOUND_CRASH/ITEM/ATTACK/HEAL/CLEAR/GAME_OVER` 없음 | `include/device.h`, `src/device.c`, `GameState` 출력 요청 필드 추가 필요 |
| reference LED 상태 기능 | `reference/device.h`, `reference/game.c` | `LED_CRASH/HEAL/CLEAR/WIN` 없음 | M4 LED 5개와 Q6 GPIO LED 3개 중 표시 정책을 분리해 구현 필요 |
| reference item 표시 | `reference/device_show_item()` | 현재 `game->lcd` 하나로 LCD preset만 갱신 | 전역 아이템 모델은 유지하되 LCD 출력 wrapper로 흡수 |
| reference 플레이어별 item/mash | `reference/types.h`, `reference/game.c` | 현재는 전역 item + `green_stack` 방식 | 게임 규칙 변경이 크므로 hardware 연동 수정과 분리 권장 |

## 패킷/통신 기준

`src/serial.c` 기준:

- UART device: `/dev/ttymxc1`
- baud/config: `B115200 | CRTSCTS | CS8 | CLOCAL | CREAD`, `IGNPAR`, raw mode, `VMIN = 5`
- packet: `[0x12, command, '0' + arg1, '0' + arg2, 0x13]`
- M4 button RX: `0x22`
- FND TX: `0x23`
- LCD TX: `0x24`

추가할 LED TX:

- command: `0x21`
- arg1: LED index `0..4`
- arg2: state `0..1`

충돌 사항:

- `src/serial.c` 주석은 LED state를 `0..1` 그대로 보낸다고 적혀 있다.
- `m4-app/led.c` 실제 샘플은 `0x31 - state`로 반전해서 보낸다.
- 기준 우선순위에 따라 `src/serial.c` 주석을 우선한다. 실제 보드 테스트에서 켜짐/꺼짐이 반대로 나오면 `serial_send_led()` 내부에서만 반전 처리하고, 호출부의 의미는 `0=off`, `1=on`으로 유지한다.

## 수정/추가할 파일

### `Makefile` 수정 완료

- `SRCS`를 `$(wildcard src/*.c)`로 변경했다.
- 현재 빠져 있던 `src/hardware.c`가 자동으로 빌드에 포함된다.
- 이후 `src/device.c`를 추가해도 Makefile 누락이 생기지 않는다.
- `CPPFLAGS=-Iinclude`로 include 경로를 분리했다.

### `include/serial.h` 수정 예정

추가:

```c
int serial_send_led(int led_index, int state);
```

의미:

- M4 LED 5개 출력 전용
- `led_index`는 `0..4`로 clamp
- `state`는 `0` off, `1` on으로 clamp

### `src/serial.c` 수정 예정

추가:

```c
#define CMD_LED_SET 0x21

int serial_send_led(int led_index, int state) {
    if (led_index < 0) led_index = 0;
    if (led_index > 4) led_index = 4;
    if (state < 0) state = 0;
    if (state > 1) state = 1;
    return write_packet(CMD_LED_SET, led_index, state);
}
```

주의:

- `write_packet()` 기존 규칙을 그대로 사용한다.
- LED 반전 문제는 여기 한 곳에서만 처리한다.

### `include/hardware.h` 수정 예정

추가/정리:

```c
#define GPIO(BANK, IO) (((BANK) - 1) * 32 + (IO))
#define Q6_LED_BACK GPIO(1, 21)
#define Q6_LED_HOME GPIO(1, 16)
#define Q6_LED_MENU GPIO(1, 20)
```

유지:

```c
void led_control(int led_pin, int state);
void buzzer_play(double freq, int time_us);
```

기준:

- `led_control()`은 Q6 GPIO LED 3개용 low-level 함수다.
- M4 LED 5개는 `serial_send_led()`가 담당한다.

### `src/hardware.c` 수정 예정

구현:

- `gpio_export()`
- `gpio_unexport()`
- `gpio_set_dir()`
- `gpio_set_value()`
- `led_control()`

초기화 변경:

- 현재 `hw_init()`은 buzzer open 실패 시 바로 `-1`을 반환하므로 Q6 key까지 비활성화될 수 있다.
- 수정 후에는 buzzer, GPIO LED, key를 독립적으로 초기화한다.
- `button_fd`가 열리면 `hw_thread`는 동작할 수 있어야 한다.
- buzzer/GPIO LED 실패는 로그만 남기고 입력 큐를 죽이지 않는다.

종료 변경:

- fd close 후 `buzzer_fd = -1`, `button_fd = -1`로 초기화한다.
- export한 Q6 GPIO LED는 `hw_close()`에서 off 후 unexport한다.

### `include/device.h` 추가 예정

reference의 기능 의미만 흡수하되, lifecycle은 현재 `main.c`의 `hw_init()`/`m4_uart_init()` 구조와 충돌하지 않게 분리한다.

```c
#ifndef DEVICE_H
#define DEVICE_H

#include "event.h"

typedef enum {
    SOUND_NONE,
    SOUND_CRASH,
    SOUND_ITEM,
    SOUND_ATTACK,
    SOUND_HEAL,
    SOUND_CLEAR,
    SOUND_GAME_OVER
} SoundType;

typedef enum {
    LED_NONE,
    LED_CRASH,
    LED_HEAL,
    LED_CLEAR,
    LED_WIN
} LedStatus;

int device_init(void);
void device_close(void);
void device_show_item(int player_index, ItemType item);
void device_show_led(int player_index, LedStatus status);
void device_play_sound(SoundType sound);
void device_show_score(int score);

#endif
```

### `src/device.c` 추가 예정

역할:

- `device_show_item()` -> `serial_send_lcd()`
- `device_show_score()` -> `serial_send_fnd_number()`
- `device_show_led()` -> `serial_send_led()` 중심으로 M4 LED 5개 제어, 필요 시 Q6 GPIO LED mirror
- `device_play_sound()` -> `buzzer_play()`로 tone/duration 매핑

사운드 매핑 예:

| SoundType | freq/duration |
| --- | --- |
| `SOUND_CRASH` | 196Hz, 120ms |
| `SOUND_ITEM` | 659Hz, 80ms |
| `SOUND_ATTACK` | 523Hz -> 392Hz 짧은 2음 |
| `SOUND_HEAL` | 659Hz -> 784Hz |
| `SOUND_CLEAR` | 784Hz, 100ms |
| `SOUND_GAME_OVER` | 330Hz -> 262Hz |

중요:

- `buzzer_play()`는 `usleep()`으로 blocking된다.
- `device_play_sound()`는 GameEvent 큐를 재사용하지 않는다.
- 별도 sound thread와 private single-slot request를 사용한다. 새 sound가 오면 이전 대기 sound를 덮어써서 큐가 밀리지 않게 한다.

### `include/game.h` 수정 예정

GameEvent 큐를 출력 이벤트로 오염시키지 않기 위해 `GameState`에 출력 요청 상태만 추가한다.

예:

```c
int sound;
int sound_seq;
int led_status[PLAYER_COUNT];
int led_seq[PLAYER_COUNT];
```

설명:

- `game.c`는 상태 전이 중 sound/LED 요청을 기록만 한다.
- 실제 장치 호출은 `main.c`의 board output 단계에서 처리한다.
- `seq`를 둬서 같은 sound/LED 값이 연속 발생해도 main이 새 이벤트로 인식할 수 있게 한다.

### `src/game.c` 수정 예정

reference 기능을 현재 게임 규칙 지점에 연결한다.

- item spawn: `SOUND_ITEM`, LCD 갱신
- crash: `SOUND_CRASH`, 해당 player `LED_CRASH`
- red attack: `SOUND_ATTACK`
- green heal 성공: `SOUND_HEAL`, 해당 player `LED_HEAL`
- blue clear: `SOUND_CLEAR`, 해당 player `LED_CLEAR`
- game over/win: `SOUND_GAME_OVER`, winner `LED_WIN`

주의:

- `game.c`에서 `device_*()`를 직접 호출하지 않는다.
- single-writer 구조를 유지하고, 장치 I/O blocking이 게임 상태 갱신을 잡아먹지 않도록 출력 요청만 기록한다.

### `src/main.c` 수정 예정

필수 정리:

- 현재 `use_serial`이 정의되어 있지 않으므로 빌드 실패 가능성이 있다.
- `int use_serial = 1;` 기본값 또는 명령행 옵션 파싱으로 명확히 정의해야 한다.

출력 처리 변경:

- `update_board_outputs()`에서 기존 LCD/FND 처리 유지
- `sound_seq`, `led_seq[]` 변화를 감지해 `device_play_sound()`, `device_show_led()` 호출
- M4 LED는 변화가 있을 때만 송신해서 UART를 과도하게 쓰지 않는다.
- 출력 I/O 실패는 `DBG()`만 남기고 EventQueue에는 push하지 않는다.

## 큐 안전성

현재 EventQueue는 입력과 tick만 담는 구조다. 이 구조를 유지해야 한다.

금지:

- buzzer 완료 이벤트, LED 완료 이벤트, FND/LCD 완료 이벤트를 `EventQueue`에 push하지 않는다.
- main thread 안에서 긴 buzzer sleep을 직접 수행하지 않는다.
- serial output 실패를 EventQueue에 재주입하지 않는다.

수정 방향:

- 게임 상태 변경은 계속 main thread만 수행한다.
- hardware/serial thread는 입력 이벤트만 생산한다.
- 장치 출력은 `update_board_outputs()`와 `device.c` 내부 private sound thread가 담당한다.
- sound thread는 GameState를 읽거나 쓰지 않는다.

## 우선순위

1. `Makefile`에 `src/hardware.c` 포함: 완료
2. `use_serial` 정의 누락 수정
3. `serial_send_led()` 추가
4. Q6 GPIO `led_control()` 구현
5. `device.h/device.c` 추가
6. `GameState` 출력 요청 필드 추가
7. `game.c` 이벤트 지점에 sound/LED 요청 기록
8. `main.c`에서 출력 요청 소비
9. 보드 테스트로 M4 LED state 반전 여부 확정
