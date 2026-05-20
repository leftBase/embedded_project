
**현재 스레드 구조**
전에 말한 “스레드 3개” 구조에서 지금은 살짝 바뀌었다. 현재 코드는 이렇게 됨.

```text
main thread
  - EventQueue에서 이벤트 소비
  - GameState 단독 수정
  - game_apply_event()
  - render_game()

timer_thread
  - 50ms마다 EV_TICK 생성

serial_thread
  - Q6 버튼 fd + M4 UART fd를 select()로 감시
  - 입력을 GameEvent로 변환해서 큐에 push

keyboard_thread
  - PC 테스트용 가짜 입력
  - 실제 제출/보드용에서는 빼도 되는 debug stub
```

즉 발표할 때는 이렇게 말하면 좋다.

> 본 프로젝트는 멀티스레드 기반이지만 공유 상태를 여러 스레드가 직접 수정하지 않는다. 모든 입력은 `GameEvent`로 정규화되고, 중앙 `EventQueue`를 통해 main thread로 전달된다. `GameState`는 main thread만 소유하는 single-writer 구조이므로 race condition을 줄인다.

**전체 구조 키워드**
```text
Event-driven architecture
Producer-Consumer pattern
Single-writer principle
Finite State Machine
Fixed timestep game loop
File descriptor multiplexing
UART fixed-length frame protocol
Hardware abstraction
Separation of concerns
Race condition avoidance
Deterministic update order
```

**로직 흐름**
```text
Q6 버튼 / M4 UART / 키보드 테스트 입력
        ↓
serial_thread / keyboard_thread
        ↓
GameEvent 생성
        ↓
EventQueue push
        ↓
main thread queue_pop
        ↓
game_apply_event()
        ↓
EV_TICK이면 game_tick()
        ↓
아이템 타이머 → rock 생성 → rock 이동 → 충돌 판정 → 점수 → 게임오버
        ↓
render_game()
```

`main` 안에 이동, 충돌, 아이템 판정을 다 나열하는 방식은 일부러 피했어. `main`은 운영체제의 작은 스케줄러처럼 이벤트만 넘기고, 게임 규칙의 순서는 `game_tick()` 안에서 고정한다. 이게 구조적으로 훨씬 발표하기 좋고, 나중에 장치가 늘어도 게임 로직이 덜 흔들린다.