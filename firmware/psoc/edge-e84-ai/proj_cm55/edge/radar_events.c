#include "radar_events.h"
#include <string.h>
#include <math.h>
void radar_events_init(radar_events_t *s, uint32_t boot_id)
{
    memset(s, 0, sizeof(*s)); s->last.boot_id = boot_id;
    s->last.confidence = 255; s->armed = true;
}
bool radar_boot_id(radar_entropy_fn entropy, void *context, uint32_t *out)
{
    return entropy && out && entropy(context, out);
}
bool radar_events_observe(radar_events_t *s, const char *label, float score, uint32_t now)
{
    uint8_t code = 0;
    if (label && strcmp(label, "Push") == 0) code = 1;
    if (label && strcmp(label, "Circle") == 0) code = 2;
    if (!code || !isfinite(score) || score < 0.80f || score > 1.0f) {
        s->candidate = 0; s->streak = 0;
        if (label && strcmp(label, "unlabelled") == 0 && isfinite(score)
            && score >= 0.80f && score <= 1.0f) s->armed = true;
        return false;
    }
    if (s->candidate != code) { s->candidate = code; s->streak = 0; }
    if (s->streak < 2) ++s->streak;
    if (!s->armed || s->streak < 2 ||
        (s->last.valid && (uint32_t)(now - s->last_ms) < 1000u)) return false;
    s->armed = false; s->last_ms = now;
    ++s->last.seq;
    s->last.kind = 1; s->last.code = code;
    s->last.confidence = (uint8_t)(score * 100.0f + 0.5f);
    s->last.valid = true; s->last.uptime_ms = now;
    return true;
}
