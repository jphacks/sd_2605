#include "protocol_v1.h"

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint8_t protocol_v1_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0u;
    for (size_t i = 0u; i < length; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0u; bit < 8u; ++bit) {
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static bool code_is_valid(uint8_t kind, uint8_t code)
{
    switch (kind) {
    case EVENT_NONE: return code == 0u;
    case EVENT_RADAR_GESTURE: return code == CODE_PUSH || code == CODE_CIRCLE;
    case EVENT_RADAR_MOTION: return code == CODE_MOTION_DETECTED;
    case EVENT_VISION_PASSAGE: return code >= CODE_LEFT_TO_RIGHT && code <= CODE_UNKNOWN_DIRECTION;
    default: return false;
    }
}

bool protocol_v1_decode(const uint8_t frame[PROTOCOL_V1_FRAME_SIZE], sensor_source_t expected_source,
                        sensor_event_t *event)
{
    if (frame == NULL || event == NULL || frame[0] != PROTOCOL_V1_MAGIC || frame[1] != PROTOCOL_V1_VERSION ||
        frame[2] != (uint8_t)expected_source || frame[6] > 1u || frame[7] != 0u ||
        frame[20] != 0u || frame[21] != 0u || frame[22] != 0u ||
        protocol_v1_crc8(frame, PROTOCOL_V1_FRAME_SIZE - 1u) != frame[23]) {
        return false;
    }

    if (!code_is_valid(frame[3], frame[4]) || (frame[5] > 100u && frame[5] != 255u)) {
        return false;
    }
    if ((expected_source == SOURCE_RADAR && frame[3] == EVENT_VISION_PASSAGE) ||
        (expected_source == SOURCE_VISION && frame[3] != EVENT_NONE && frame[3] != EVENT_VISION_PASSAGE)) {
        return false;
    }

    event->source = frame[2];
    event->kind = frame[3];
    event->code = frame[4];
    event->confidence = frame[5];
    event->flags = frame[6];
    event->boot_id = read_u32_le(&frame[8]);
    event->seq = read_u32_le(&frame[12]);
    event->uptime_ms = read_u32_le(&frame[16]);
    return true;
}
