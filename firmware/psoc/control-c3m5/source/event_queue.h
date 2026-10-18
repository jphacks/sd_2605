#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ACTION_LEFT_TO_RIGHT,
    ACTION_RIGHT_TO_LEFT,
    ACTION_REPLAY
} event_action_t;

#define EVENT_QUEUE_CAPACITY 8u

typedef struct {
    event_action_t action;
    uint32_t seq;
} queued_event_t;

typedef struct {
    queued_event_t items[EVENT_QUEUE_CAPACITY];
    uint8_t head;
    uint8_t count;
} event_queue_t;

void event_queue_init(event_queue_t *queue);
bool event_queue_push(event_queue_t *queue, event_action_t action, uint32_t seq);
bool event_queue_peek(const event_queue_t *queue, queued_event_t *event);
void event_queue_pop(event_queue_t *queue);

#endif
