#ifndef EVENT_H
#define EVENT_H

#include <pthread.h>

typedef enum {
    EV_NONE = 0,
    EV_TICK,
    EV_START,
    EV_PAUSE,
    EV_QUIT,
    EV_P1_LEFT,
    EV_P1_RIGHT,
    EV_P1_SKILL,
    EV_P2_LEFT,
    EV_P2_RIGHT,
    EV_P2_SKILL
} EventType;

typedef enum {
    ITEM_NONE = 0,
    ITEM_RED,
    ITEM_GREEN,
    ITEM_BLUE
} ItemType;

typedef struct {
    EventType type;
    int value;
    ItemType item_type;
} GameEvent;

#define QUEUE_SIZE 64

typedef struct {
    GameEvent buffer[QUEUE_SIZE];
    int head;
    int tail;
    pthread_mutex_t lock;      // 큐를 보호하는 자물쇠
    pthread_cond_t not_empty;  // 큐에 데이터가 들어왔음을 알리는 알람
} EventQueue;

void queue_init(EventQueue *q);
void queue_push_event(EventQueue *q, GameEvent event);
void queue_push(EventQueue *q, EventType type, int value);
GameEvent queue_pop(EventQueue *q);

#endif
