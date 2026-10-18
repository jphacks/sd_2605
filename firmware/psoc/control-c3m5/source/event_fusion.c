#include "event_fusion.h"

bool event_fusion_handle(const sensor_event_t *event, uint32_t receive_ms, event_queue_t *queue)
{
    (void)receive_ms; /* Reserved for future cross-sensor time-window fusion. */
    if (event == NULL || queue == NULL || (event->flags & 1u) == 0u) return false;

    if (event->source == SOURCE_VISION && event->kind == EVENT_VISION_PASSAGE) {
        if (event->code == CODE_LEFT_TO_RIGHT) return event_queue_push(queue, ACTION_LEFT_TO_RIGHT, event->seq);
        if (event->code == CODE_RIGHT_TO_LEFT) return event_queue_push(queue, ACTION_RIGHT_TO_LEFT, event->seq);
    }
    if (event->source == SOURCE_RADAR && event->kind == EVENT_RADAR_GESTURE && event->code == CODE_PUSH) {
        return event_queue_push(queue, ACTION_REPLAY, event->seq);
    }
    return false;
}
