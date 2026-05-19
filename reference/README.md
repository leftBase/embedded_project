# H-Embed-TKU 2P Racing Battle Game

## 1. 프로젝트 개요

H-Embed-TKU 보드에서 실행할 2인용 레이싱 배틀 게임이다.  
C 언어 기반으로 게임 로직과 Linux terminal GUI를 구현하고, 이후 보드 버튼, LCD, LED, buzzer, FND와 연결할 예정이다.

### 실행방법

```
make
./racing_game

$ 변경 후 재실행은
make clean
make
./racing_game
```

## 2. 현재 구현 범위

현재 구현된 부분은 다음과 같다.

- 2인용 게임 구조
- 1P / 2P 각각 3-lane 구조
- 플레이어 좌우 이동 로직
- 장애물 생성 및 이동 로직
- 충돌 판정
- 생명 감소
- 점수 계산
- Red / Green / Blue 아이템 로직
- 키보드 입력 테스트용 모듈
- 보드 출력 연결을 위한 stub 함수
- Linux terminal 기반 ASCII GUI 초안

## 3. 파일 구조

```text
embed_racing/
├── Makefile
├── config.h
├── types.h
├── main.c
├── game.h
├── game.c
├── render.h
├── render.c
├── input.h
├── input_keyboard.c
├── device.h
└── device_stub.c
