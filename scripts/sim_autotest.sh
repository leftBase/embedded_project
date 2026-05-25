#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

rm -f game_events.log game_debug.log sim_serial.log run_stdout.log

make clean
make sim

SIM_LOG_PATH=sim_serial.log SIM_SERIAL_VERBOSE=1 GAME_DEBUG=1 ./racing_game --autotest > run_stdout.log 2>&1

fail=0

check() {
    local file="$1"
    local pattern="$2"
    local label="$3"

    if grep -qE "$pattern" "$file"; then
        printf '[PASS] %s\n' "$label"
    else
        printf '[FAIL] %s\n' "$label"
        printf '       missing pattern: %s in %s\n' "$pattern" "$file"
        fail=1
    fi
}

first_line() {
    local file="$1"
    local pattern="$2"

    grep -nE "$pattern" "$file" | head -n 1 | cut -d: -f1 || true
}

check_order() {
    local label="$1"
    shift
    local prev=0
    local ok=1
    local pattern
    local line

    for pattern in "$@"; do
        line="$(first_line sim_serial.log "$pattern")"
        if [ -z "$line" ] || [ "$line" -le "$prev" ]; then
            ok=0
            break
        fi
        prev="$line"
    done

    if [ "$ok" -eq 1 ]; then
        printf '[PASS] %s\n' "$label"
    else
        printf '[FAIL] %s\n' "$label"
        printf '       LCD order expected: LOGO -> RED -> GREEN -> BLUE\n'
        fail=1
    fi
}

check sim_serial.log '\[SIM\]\[LCD\] preset=0 label=LOGO' 'LCD LOGO output'
check sim_serial.log '\[SIM\]\[LCD\] preset=1 label=RED' 'LCD RED output'
check sim_serial.log '\[SIM\]\[LCD\] preset=2 label=GREEN' 'LCD GREEN output'
check sim_serial.log '\[SIM\]\[LCD\] preset=3 label=BLUE' 'LCD BLUE output'
check_order 'LCD order LOGO -> RED -> GREEN -> BLUE' \
    '\[SIM\]\[LCD\] preset=0 label=LOGO' \
    '\[SIM\]\[LCD\] preset=1 label=RED' \
    '\[SIM\]\[LCD\] preset=2 label=GREEN' \
    '\[SIM\]\[LCD\] preset=3 label=BLUE'

for id in 0 1 2 3 4; do
    check sim_serial.log "\\[SIM\\]\\[M4_BUTTON_RX\\].*id=${id} pressed=1" "M4 button ${id} press packet"
    check sim_serial.log "\\[SIM\\]\\[M4_BUTTON_RX\\].*id=${id} pressed=0" "M4 button ${id} release packet"
    check sim_serial.log "\\[SIM\\]\\[M4_LED\\] index=${id} state=ON" "M4 LED ${id} ON"
    check sim_serial.log "\\[SIM\\]\\[M4_LED\\] index=${id} state=OFF" "M4 LED ${id} OFF"
done

check sim_serial.log '\[SIM\]\[Q6_KEY_RX\] code=158 pressed=1' 'Q6 BACK press'
check sim_serial.log '\[SIM\]\[Q6_KEY_RX\] code=158 pressed=0' 'Q6 BACK release'
check sim_serial.log '\[SIM\]\[Q6_KEY_RX\] code=172 pressed=1' 'Q6 HOME press'
check sim_serial.log '\[SIM\]\[Q6_KEY_RX\] code=172 pressed=0' 'Q6 HOME release'
check sim_serial.log '\[SIM\]\[Q6_KEY_RX\] code=139 pressed=1' 'Q6 MENU press'
check sim_serial.log '\[SIM\]\[Q6_KEY_RX\] code=139 pressed=0' 'Q6 MENU release'
check sim_serial.log '\[SIM\]\[Q6_LED\] pin=21 state=ON' 'Q6 BACK LED ON'
check sim_serial.log '\[SIM\]\[Q6_LED\] pin=21 state=OFF' 'Q6 BACK LED OFF'
check sim_serial.log '\[SIM\]\[Q6_LED\] pin=16 state=ON' 'Q6 HOME LED ON'
check sim_serial.log '\[SIM\]\[Q6_LED\] pin=16 state=OFF' 'Q6 HOME LED OFF'
check sim_serial.log '\[SIM\]\[Q6_LED\] pin=20 state=ON' 'Q6 MENU LED ON'
check sim_serial.log '\[SIM\]\[Q6_LED\] pin=20 state=OFF' 'Q6 MENU LED OFF'
check sim_serial.log '\[SIM\]\[BUZZER\]' 'Buzzer output logged'
check sim_serial.log '\[SIM\]\[FND\] number=' 'FND output logged'
check sim_serial.log 'packet=12 21 30 31 13' 'M4 LED0 ON packet'
check sim_serial.log 'packet=12 21 30 30 13' 'M4 LED0 OFF packet'
check sim_serial.log 'packet=12 24 30 31 13' 'LCD RED packet'
check sim_serial.log 'packet=12 24 30 32 13' 'LCD GREEN packet'
check sim_serial.log 'packet=12 24 30 33 13' 'LCD BLUE packet'

check game_events.log 'sim_force_item,0' 'Forced LOGO item state logged'
check game_events.log 'sim_force_item,1' 'Forced RED item state logged'
check game_events.log 'sim_force_item,2' 'Forced GREEN item state logged'
check game_events.log 'sim_force_item,3' 'Forced BLUE item state logged'
check game_events.log 'red_attack,0' 'RED attack handled through queue'
check game_events.log 'green_stack,0' 'GREEN skill stack handled through queue'
check game_events.log 'green_heal,0' 'GREEN heal handled after timer'
check game_events.log 'blue_clear,0' 'BLUE clear handled through queue'

check game_debug.log 'serial_event type=EV_START' 'M4 start reached serial thread and main queue'
check game_debug.log 'serial_event type=EV_P1_LEFT' 'M4 P1 left reached serial thread and main queue'
check game_debug.log 'serial_event type=EV_P2_RIGHT' 'M4 P2 right reached serial thread and main queue'
check game_debug.log 'hw_event type=EV_P1_SKILL' 'Q6 BACK reached hardware thread and main queue'
check game_debug.log 'hw_event type=EV_PAUSE' 'Q6 HOME reached hardware thread and main queue'
check game_debug.log 'hw_event type=EV_P2_SKILL' 'Q6 MENU reached hardware thread and main queue'

if [ "$fail" -ne 0 ]; then
    printf '\nAutotest verification failed. See sim_serial.log, game_events.log, game_debug.log, run_stdout.log.\n'
    exit 1
fi

printf '\nAutotest verification passed. Logs: sim_serial.log, game_events.log, game_debug.log, run_stdout.log\n'
