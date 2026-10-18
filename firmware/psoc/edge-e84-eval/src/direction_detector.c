#include "direction_detector.h"
#include <string.h>

static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static void reset_track(direction_tracker_t *t)
{
    t->frame_count = t->missed_frames = 0u;
    t->start_x = t->last_x = t->min_x = t->max_x = t->signed_motion = 0.0f;
    t->ambiguous = t->reversed = false;
}

void direction_tracker_init(direction_tracker_t *t, const direction_config_t *c)
{
    memset(t, 0, sizeof(*t)); t->config = *c; t->state = TRACK_IDLE;
}

static bool usable_box(const direction_tracker_t *t, person_box_t b)
{
    const float w = clampf(b.x1, 0, t->config.frame_width) - clampf(b.x0, 0, t->config.frame_width);
    const float h = clampf(b.y1, 0, t->config.frame_height) - clampf(b.y0, 0, t->config.frame_height);
    return w > 0 && h > 0 && (w * h) / ((float)t->config.frame_width * t->config.frame_height) >= t->config.min_area_ratio;
}

static float center_x(const direction_tracker_t *t, person_box_t b)
{
    float x = (b.x0 + b.x1) * 0.5f;
    if (t->config.mirrored_input) x = t->config.frame_width - x;
    return clampf(x, 0, t->config.frame_width);
}

static direction_t finish_track(direction_tracker_t *t)
{
    direction_t result = DIRECTION_UNKNOWN;
    const float travel = t->max_x - t->min_x;
    const bool enough_travel = travel >= t->config.frame_width * t->config.min_displacement_ratio;
    if (!t->ambiguous && !t->reversed && t->frame_count >= t->config.min_frames &&
        enough_travel && t->signed_motion != 0.0f) {
        result = t->signed_motion > 0 ? DIRECTION_LEFT_TO_RIGHT : DIRECTION_RIGHT_TO_LEFT;
    }
    t->state = enough_travel ? TRACK_COOLDOWN : TRACK_IDLE;
    t->cooldown_left = enough_travel ? t->config.cooldown_frames : 0u;
    reset_track(t);
    return enough_travel ? result : DIRECTION_NONE;
}

direction_t direction_tracker_update(direction_tracker_t *t, const person_box_t *boxes, uint8_t count)
{
    if (t->state == TRACK_COOLDOWN) {
        if (t->cooldown_left) --t->cooldown_left;
        if (!t->cooldown_left) t->state = TRACK_IDLE;
        return DIRECTION_NONE;
    }
    if (count > 1u && (t->state == TRACK_PERSON_DETECTED || t->state == TRACK_TRACKING)) {
        t->ambiguous = true;
        if (++t->missed_frames > t->config.max_missed_frames) return finish_track(t);
        return DIRECTION_NONE;
    }
    if (count == 0u || !usable_box(t, boxes[0])) {
        if (t->state == TRACK_PERSON_DETECTED || t->state == TRACK_TRACKING) {
            if (++t->missed_frames > t->config.max_missed_frames) return finish_track(t);
        }
        return DIRECTION_NONE;
    }
    const float x = center_x(t, boxes[0]);
    if (t->state == TRACK_IDLE) {
        reset_track(t); t->start_x = t->last_x = t->min_x = t->max_x = x;
        t->frame_count = 1u; t->state = TRACK_PERSON_DETECTED;
        return DIRECTION_NONE;
    }
    t->missed_frames = 0u;
    const float step = x - t->last_x;
    if (step > t->config.frame_width * t->config.max_step_ratio ||
        step < -t->config.frame_width * t->config.max_step_ratio) {
        t->ambiguous = true;
    } else if (step != 0.0f) {
        if (t->signed_motion != 0.0f && step * t->signed_motion < 0.0f &&
            (step < 0 ? -step : step) >= t->config.frame_width * t->config.reversal_ratio)
            t->reversed = true;
        t->signed_motion += step;
    }
    t->last_x = x; if (x < t->min_x) t->min_x = x; if (x > t->max_x) t->max_x = x;
    if (++t->frame_count >= 2u) t->state = TRACK_TRACKING;
    /* A passage ends at a frame edge. Publish once, then require a quiet cooldown. */
    if (t->frame_count >= t->config.min_frames &&
        ((x <= 1.0f) || (x >= (float)t->config.frame_width - 1.0f))) return finish_track(t);
    return DIRECTION_NONE;
}
