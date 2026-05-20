#ifndef DEBUG_H
#define DEBUG_H

#include "event.h"

void debug_init(const char *enabled);
void debug_close(void);
void debug_log(const char *fmt, ...);
void debug_log_event(const char *tag, GameEvent event);

#ifdef ENABLE_DEBUG_LOG
#define DBG(...) debug_log(__VA_ARGS__)
#else
#define DBG(...) do { } while (0)
#endif

#endif
