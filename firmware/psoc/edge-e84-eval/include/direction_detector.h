#ifndef EDGE_E84_DIRECTION_DETECTOR_H
#define EDGE_E84_DIRECTION_DETECTOR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { TRACK_IDLE, TRACK_PERSON_DETECTED, TRACK_TRACKING,
               TRACK_DIRECTION_CONFIRMED, TRACK_COOLDOWN } tracker_state_t;
typedef enum { DIRECTION_NONE = 0, DIRECTION_LEFT_TO_RIGHT = 1,
               DIRECTION_RIGHT_TO_LEFT = 2, DIRECTION_UNKNOWN = 3 } direction_t;

typedef struct {
    float x0, y0, x1, y1;
    float confidence;
} person_box_t;

typedef struct {
    uint16_t frame_width;
    uint16_t frame_height;
    uint16_t min_frames;
    uint16_t max_missed_frames;
    uint16_t cooldown_frames;
    float min_area_ratio;
    float min_displacement_ratio;
    float max_step_ratio;
    float reversal_ratio;
    bool mirrored_input;
} direction_config_t;

typedef struct {
    tracker_state_t state;
    direction_config_t config;
    float start_x, last_x, min_x, max_x;
    float signed_motion;
    uint16_t frame_count, missed_frames, cooldown_left;
    bool ambiguous, reversed;
} direction_tracker_t;

void direction_tracker_init(direction_tracker_t *tracker, const direction_config_t *config);
/* detections must contain person-segmentation boxes, not generic motion blobs.
 * Returns a direction once per terminated track; otherwise DIRECTION_NONE. */
direction_t direction_tracker_update(direction_tracker_t *tracker,
                                     const person_box_t *detections,
                                     uint8_t detection_count);

#endif
