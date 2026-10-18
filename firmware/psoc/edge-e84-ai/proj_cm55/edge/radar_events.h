#pragma once
#include "protocol_v1.h"
typedef struct {
    radar_event_t last;
    uint32_t last_ms;
    uint8_t candidate, streak;
    bool armed;
} radar_events_t;
void radar_events_init(radar_events_t *state, uint32_t boot_id);
/* Two consecutive >=80% frames; rearm after unlabelled; 1000 ms cooldown. */
bool radar_events_observe(radar_events_t *state, const char *label,
                          float score, uint32_t now_ms);
/* Entropy failure must stop startup; no deterministic fallback. */
typedef bool (*radar_entropy_fn)(void *context, uint32_t *value);
bool radar_boot_id(radar_entropy_fn entropy, void *context, uint32_t *out);
