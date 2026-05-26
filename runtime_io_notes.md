# Runtime I/O Notes

## Removed exit-time game log save

The old `save_game_log()` path was removed from the running game.

Reason:
- It performed file I/O at shutdown.
- It printed status text to stdout and flushed it.
- On the board, stdout is usually the same slow terminal/UART path as the renderer, so extra shutdown output can make termination look stuck.

What remains:
- `debug.c` still writes `game_debug.log` when `ENABLE_DEBUG_LOG` is enabled.
- The in-memory `GameState.logs` ring is still present, but it is no longer dumped on exit.

If game-event persistence is needed again, prefer a low-frequency debug-only dump or a separate worker thread. Do not print save progress to the terminal during shutdown on the board.

## Buzzer playback

`buzzer_play()` still contains the hardware duration wait, but it is now called only from the sound worker thread. The main loop enqueues sound requests and immediately continues processing events, rendering, LCD, and FND output.
