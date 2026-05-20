#include "debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *debug_fp;

static const char *event_name(EventType type) {
    switch (type) {
        case EV_NONE: return "EV_NONE";
        case EV_TICK: return "EV_TICK";
        case EV_START: return "EV_START";
        case EV_PAUSE: return "EV_PAUSE";
        case EV_QUIT: return "EV_QUIT";
        case EV_P1_LEFT: return "EV_P1_LEFT";
        case EV_P1_RIGHT: return "EV_P1_RIGHT";
        case EV_P1_SKILL: return "EV_P1_SKILL";
        case EV_P2_LEFT: return "EV_P2_LEFT";
        case EV_P2_RIGHT: return "EV_P2_RIGHT";
        case EV_P2_SKILL: return "EV_P2_SKILL";
        default: return "EV_UNKNOWN";
    }
}

void debug_init(const char *enabled) {
#ifdef ENABLE_DEBUG_LOG
    if (enabled != NULL && enabled[0] == '0') {
        return;
    }

    debug_fp = fopen("game_debug.log", "a");
    if (debug_fp != NULL) {
        debug_log("debug logging started");
    }
#else
    (void)enabled;
#endif
}

void debug_close(void) {
#ifdef ENABLE_DEBUG_LOG
    if (debug_fp != NULL) {
        debug_log("debug logging stopped");
        fclose(debug_fp);
        debug_fp = NULL;
    }
#endif
}

void debug_log(const char *fmt, ...) {
#ifdef ENABLE_DEBUG_LOG
    va_list args;
    time_t now;

    if (debug_fp == NULL) {
        return;
    }

    now = time(NULL);
    fprintf(debug_fp, "[%ld] ", (long)now);

    va_start(args, fmt);
    vfprintf(debug_fp, fmt, args);
    va_end(args);

    fputc('\n', debug_fp);
    fflush(debug_fp);
#else
    (void)fmt;
#endif
}

void debug_log_event(const char *tag, GameEvent event) {
#ifdef ENABLE_DEBUG_LOG
    debug_log("%s type=%s value=%d item=%d", tag, event_name(event.type), event.value, event.item_type);
#else
    (void)tag;
    (void)event;
#endif
}
