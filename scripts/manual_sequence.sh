#!/usr/bin/env bash
# manual_sequence.sh
# 사람 입력 가이드: 게임을 한 터미널에서 `./racing_game --manual` 로 실행하고,
# 이 스크립트를 다른 터미널에서 실행해 안내에 따라 게임 터미널에 키를 입력하세요.
#
# 키 매핑 (게임 내부 `keyboard_thread` 기준):
#  - M4 buttons: id0=P1 left -> 'a' , id1=P1 right -> 'd' , id2=start -> 's' , id3=P2 left -> 'j' , id4=P2 right -> 'l'
#  - Q6 keys: BACK (P1 skill) -> 'q' , HOME (pause) -> 'p' , MENU (P2 skill) -> 'o'
#  - 종료: 'x'
#
# 사용: 다른 터미널에서 안내를 보고, 게임 터미널에 해당 키(예: 's','a','q' 등)를 직접 누르세요.
# 각 단계마다 Enter를 누르면 다음 안내로 넘어갑니다.

pause_for_user() {
  read -rp "게임 터미널에서 지시된 키를 입력한 뒤 여기로 돌아와 Enter를 누르세요..."
}

echo "수동 검사 안내 시작."

echo
echo "1) START: 게임 터미널에서 's' 누름 (게임 시작)"
pause_for_user

echo
echo "2) LOGO 확인: LCD가 LOGO로 보이는지 확인 (시리얼 로그 또는 화면)"
read -rp "확인되면 Enter를 누르세요..." dummy

echo
echo "3) M4 이동 버튼 테스트 (한 번에 하나씩 누름)"
echo "   P1 left: 'a' (id=0)   → 누른 뒤 Enter"
read -rp "" dummy
echo "   P1 right: 'd' (id=1)  → 누른 뒤 Enter"
read -rp "" dummy
echo "   P2 left: 'j' (id=3)   → 누른 뒤 Enter"
read -rp "" dummy
echo "   P2 right: 'l' (id=4)  → 누른 뒤 Enter"
read -rp "" dummy

echo
echo "4) RED 아이템: LCD가 RED일 때 P1 스킬 'q' 한 번 누름 (red_attack 확인)"
pause_for_user

echo
echo "5) GREEN 아이템: LCD가 GREEN일 때 P1 스킬 'q'를 5회 누름 (스택 누적)"
pause_for_user
echo "   스택 누른 후 아이템 만료까지 대기하여 green_heal 로그 확인"
read -rp "green_heal 확인 후 Enter" dummy

echo
echo "6) BLUE 아이템: LCD가 BLUE일 때 P1 스킬 'q' 누름 (blue_clear 확인)"
pause_for_user

echo
echo "7) P2 스킬 테스트: Q6 MENU -> 'o' 누름"
pause_for_user

echo
echo "8) PAUSE 테스트: Q6 HOME -> 'p' 두 번 눌러 토글 확인"
pause_for_user

echo
echo "9) 종료: 게임 터미널에서 'x' 눌러 종료"
pause_for_user

echo
echo "수동 검사 완료. run_stdout.log, sim_serial.log, game_events.log, game_debug.log 확인 권장."
