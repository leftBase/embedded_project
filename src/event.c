#include "event.h"

//다음 인덱스 리턴. 원형큐.
static int queue_next_index(int index) {
    return (index + 1) % QUEUE_SIZE;
}

// 큐가 가득 찼는지 여부 체크
static int queue_is_full(const EventQueue *q) {
    return queue_next_index(q->tail) == q->head;
}

//이벤트 큐 초기화
void queue_init(EventQueue *q) {
    q->head = 0; 
    q->tail = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    q->closed = 0;
}

//큐닫기위해 closed 플래그 세우고 대기중인 스레드는 깨우기
void queue_close(EventQueue *q) {
    pthread_mutex_lock(&q->lock);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->lock);
}

// 내부)뮤텍스 락 걸고 푸시
void queue_push_event(EventQueue *q, GameEvent event) {
    pthread_mutex_lock(&q->lock); // 자물쇠 채움 (다른 스레드 접근 금지)
    // 블로킹 방식: 큐가 가득차면 빈 공간이 생길 때까지 대기
    while (queue_is_full(q) && !q->closed) {
        /*
        큐가 차있으면 lock 풀고 슬립
        not_full 조건변수로 기다림. 
        누군가 pop에서 signal을 주면 깨어나서 다시 자물쇠 채움
        */
        pthread_cond_wait(&q->not_full, &q->lock);
    }

    if (q->closed) { pthread_mutex_unlock(&q->lock); return; }

    q->buffer[q->tail] = event;
    q->tail = queue_next_index(q->tail);

    // 메인스레드 또는 소비자 스레드 깨움
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock); 
}

//푸시
void queue_push(EventQueue *q, EventType type, int value) {
    GameEvent event;

    event.type = type;
    event.value = value;
    event.item_type = ITEM_NONE;

    queue_push_event(q, event);
}

//GameEvent 팝
GameEvent queue_pop(EventQueue *q) {
    pthread_mutex_lock(&q->lock);
    
    while (q->head == q->tail && !q->closed) {
        // 큐가 비어있으면 자물쇠를 풀고 여기서 잠들어버림 (CPU 0%)
        // 누군가 push에서 signal을 주면 깨어나서 다시 자물쇠를 채움
        pthread_cond_wait(&q->not_empty, &q->lock); 
    }
    
    GameEvent ev;

    if (q->head == q->tail && q->closed) {
        ev.type = EV_NONE;
        ev.value = 0;
        ev.item_type = ITEM_NONE;
        pthread_mutex_unlock(&q->lock);
        return ev;
    }
    
    ev = q->buffer[q->head];

    q->head = queue_next_index(q->head);
    // 공간이 생겼음을 생산자에게 알림
    pthread_cond_signal(&q->not_full);
    
    pthread_mutex_unlock(&q->lock);
    return ev;
}
