#ifndef PROTOCOL_V1_H
#define PROTOCOL_V1_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROTOCOL_V1_FRAME_SIZE 24u
#define PROTOCOL_V1_MAGIC 0xA5u
#define PROTOCOL_V1_VERSION 1u

typedef enum {
    SOURCE_RADAR = 1,
    SOURCE_VISION = 2
} sensor_source_t;

typedef enum {
    EVENT_NONE = 0,
    EVENT_RADAR_GESTURE = 1,
    EVENT_RADAR_MOTION = 2,
    EVENT_VISION_PASSAGE = 3
} event_kind_t;

typedef enum {
    CODE_PUSH = 1,
    CODE_CIRCLE = 2,
    CODE_MOTION_DETECTED = 1,
    CODE_LEFT_TO_RIGHT = 1,
    CODE_RIGHT_TO_LEFT = 2,
    CODE_UNKNOWN_DIRECTION = 3
} event_code_t;

typedef struct {
    uint8_t source;
    uint8_t kind;
    uint8_t code;
    uint8_t confidence;
    uint8_t flags;
    uint32_t boot_id;
    uint32_t seq;
    uint32_t uptime_ms;
} sensor_event_t;

uint8_t protocol_v1_crc8(const uint8_t *data, size_t length);
bool protocol_v1_decode(const uint8_t frame[PROTOCOL_V1_FRAME_SIZE], sensor_source_t expected_source,
                        sensor_event_t *event);

#endif
