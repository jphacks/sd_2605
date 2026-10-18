#ifndef EDGE_E84_VISION_AI_H
#define EDGE_E84_VISION_AI_H

#include "direction_detector.h"

/* Model adapter boundary. Convert the official DEEPCRAFT/YOLACT person-mask
 * output into zero or more frame-space boxes here; tracker code does not depend
 * on a particular model or tensor layout. */
typedef struct {
    const uint8_t *pixels;
    uint16_t width, height;
    uint32_t stride_bytes;
    uint32_t captured_at_ms;
} vision_frame_t;

typedef struct {
    person_box_t *boxes;
    uint8_t capacity;
    uint8_t count;
    uint32_t inference_time_ms;
} vision_result_t;

int vision_ai_init(void);
int vision_ai_infer(const vision_frame_t *frame, vision_result_t *result);

#endif
