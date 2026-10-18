#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PROTOCOL_V1_SIZE 24u
#define PROTOCOL_V1_ADDRESS 0x30u
typedef struct {
    uint32_t boot_id, seq, uptime_ms;
    uint8_t kind, code, confidence;
    bool valid;
} radar_event_t;
uint8_t protocol_crc8(const uint8_t *data, size_t size);
void protocol_serialize(const radar_event_t *event, uint8_t out[PROTOCOL_V1_SIZE]);
