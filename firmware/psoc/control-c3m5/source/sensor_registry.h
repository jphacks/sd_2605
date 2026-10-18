#ifndef SENSOR_REGISTRY_H
#define SENSOR_REGISTRY_H

#include "protocol_v1.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t address;
    sensor_source_t source;
    uint32_t last_receive_ms;
    uint32_t last_boot_id;
    uint32_t last_seq;
    uint32_t crc_errors;
    uint32_t malformed_frames;
    uint32_t nack_timeouts;
    uint8_t last_kind;
    uint8_t last_code;
    bool has_identity;
    bool available;
} sensor_record_t;

void sensor_registry_init(sensor_record_t records[2]);
bool sensor_registry_accept(sensor_record_t *record, const sensor_event_t *event, uint32_t receive_ms);

#endif
