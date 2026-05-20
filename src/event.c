#include "event.h"

static int queue_next_index(int index) {
    return (index + 1) % QUEUE_SIZE;
}

static int queue_is_full(const EventQueue *q) {
    return queue_next_index(q->tail) == q->head;
}

static int event_is_control(EventType type) {
    return type != EV_TICK && type != EV_NONE;
}

void queue_init(EventQueue *q) {
    q->head = 0; 
    q->tail = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

void queue_push_event(EventQueue *q, GameEvent event) {
    pthread_mutex_lock(&q->lock); // 자물쇠 채움 (다른 스레드 접근 금지)
    
    if (queue_is_full(q)) {
        if (event.type == EV_TICK) {
            pthread_mutex_unlock(&q->lock);
            return;
        }

        if (event_is_control(event.type)) {
            q->head = queue_next_index(q->head);
        }
    }

    if (!queue_is_full(q)) {
        q->buffer[q->tail] = event;
        q->tail = queue_next_index(q->tail);
        
        // 메인스레드 깨움
        pthread_cond_signal(&q->not_empty); 
    }
    
    pthread_mutex_unlock(&q->lock); // 자물쇠 품
}

void queue_push(EventQueue *q, EventType type, int value) {
    GameEvent event;

    event.type = type;
    event.value = value;
    event.item_type = ITEM_NONE;

    queue_push_event(q, event);
}

GameEvent queue_pop(EventQueue *q) {
    pthread_mutex_lock(&q->lock);
    
    while (q->head == q->tail) {
        // 큐가 비어있으면 자물쇠를 풀고 여기서 잠들어버림 (CPU 0%)
        // 누군가 push에서 signal을 주면 깨어나서 다시 자물쇠를 채움
        pthread_cond_wait(&q->not_empty, &q->lock); 
    }
    
    GameEvent ev = q->buffer[q->head];
    q->head = queue_next_index(q->head);
    
    pthread_mutex_unlock(&q->lock);
    return ev;
}
