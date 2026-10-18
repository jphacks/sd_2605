#include "event_queue.h"

void event_queue_init(event_queue_t *queue)
{
    queue->head = 0u;
    queue->count = 0u;
}

bool event_queue_push(event_queue_t *queue, event_action_t action, uint32_t seq)
{
    if (queue->count >= EVENT_QUEUE_CAPACITY) return false;
    const uint8_t tail = (uint8_t)((queue->head + queue->count) % EVENT_QUEUE_CAPACITY);
    queue->items[tail].action = action;
    queue->items[tail].seq = seq;
    ++queue->count;
    return true;
}

bool event_queue_peek(const event_queue_t *queue, queued_event_t *event)
{
    if (queue->count == 0u) return false;
    *event = queue->items[queue->head];
    return true;
}

void event_queue_pop(event_queue_t *queue)
{
    if (queue->count == 0u) return;
    queue->head = (uint8_t)((queue->head + 1u) % EVENT_QUEUE_CAPACITY);
    --queue->count;
}
