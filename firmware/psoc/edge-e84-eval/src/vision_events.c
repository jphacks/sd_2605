#include "vision_events.h"

void vision_events_init(vision_events_t *e, protocol_snapshot_t *snapshot, uint32_t boot_id,
                        protocol_lock_fn lock, protocol_unlock_fn unlock, void *lock_context)
{
    e->snapshot = snapshot; e->boot_id = boot_id; e->seq = 0u;
    protocol_snapshot_init(snapshot, lock, unlock, lock_context, boot_id);
}

void vision_events_publish_passage(vision_events_t *e, direction_t direction,
                                   uint8_t confidence, uint32_t uptime_ms)
{
    if (direction == DIRECTION_NONE) return;
    uint8_t code = (direction == DIRECTION_LEFT_TO_RIGHT) ? VISION_DIR_LEFT_TO_RIGHT :
                   (direction == DIRECTION_RIGHT_TO_LEFT) ? VISION_DIR_RIGHT_TO_LEFT :
                   VISION_DIR_UNKNOWN;
    vision_event_t event = { VISION_KIND_PASSAGE, code, confidence, 1u,
                             e->boot_id, ++e->seq, uptime_ms };
    protocol_snapshot_publish(e->snapshot, &event);
}

void vision_events_publish_idle(vision_events_t *e, uint32_t uptime_ms)
{
    vision_event_t event = { VISION_KIND_NONE, 0u, 255u, 0u,
                             e->boot_id, e->seq, uptime_ms };
    protocol_snapshot_publish(e->snapshot, &event);
}
