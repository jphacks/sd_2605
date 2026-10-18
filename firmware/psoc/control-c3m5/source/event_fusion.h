#ifndef EVENT_FUSION_H
#define EVENT_FUSION_H

#include "event_queue.h"
#include "protocol_v1.h"

bool event_fusion_handle(const sensor_event_t *event, uint32_t receive_ms, event_queue_t *queue);

#endif
