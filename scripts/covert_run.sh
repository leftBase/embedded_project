#!/usr/bin/env bash
# covert_run.sh
# 사용:
#  - 보통 실행: ./scripts/covert_run.sh            -> 게임을 기본 모드로 실행
#  - 테스트 실행: ./scripts/covert_run.sh test      -> 자동검증(--autotest) 실행 (로그 저장)
# 의도: 원격으로 파일만 전송해 "test" 인자로 실행하면 자동검증 시나리오가 돌아가고,
# 그냥 실행하면 평소처럼 게임이 실행됩니다.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [ "${1:-}" = "test" ]; then
    echo "Running covert autotest..."
    rm -f game_events.log game_debug.log sim_serial.log run_stdout.log
    make -s sim
    SIM_LOG_PATH=sim_serial.log SIM_SERIAL_VERBOSE=1 GAME_DEBUG=1 ./racing_game --autotest > run_stdout.log 2>&1
    echo "Autotest finished. Logs: sim_serial.log game_events.log game_debug.log run_stdout.log"
    exit 0
fi

# 기본: 게임 수동 실행 (원래 동작)
./racing_game "$@"
