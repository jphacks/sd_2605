#ifndef EDGE_E84_VISION_EVENTS_H
#define EDGE_E84_VISION_EVENTS_H

#include "direction_detector.h"
#include "protocol_v1.h"

typedef struct {
    protocol_snapshot_t *snapshot;
    uint32_t boot_id;
    uint32_t seq;
} vision_events_t;

void vision_events_init(vision_events_t *events, protocol_snapshot_t *snapshot,
                        uint32_t boot_id, protocol_lock_fn lock,
                        protocol_unlock_fn unlock, void *lock_context);
void vision_events_publish_passage(vision_events_t *events, direction_t direction,
                                   uint8_t confidence, uint32_t uptime_ms);
void vision_events_publish_idle(vision_events_t *events, uint32_t uptime_ms);

#endif
