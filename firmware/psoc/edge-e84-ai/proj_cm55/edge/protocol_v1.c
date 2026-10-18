#include "protocol_v1.h"
#include <string.h>
uint8_t protocol_crc8(const uint8_t *data, size_t size)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (uint8_t)((crc << 1) ^ ((crc & 0x80u) ? 0x07u : 0u));
    }
    return crc;
}
static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8u * i));
}
void protocol_serialize(const radar_event_t *e, uint8_t out[PROTOCOL_V1_SIZE])
{
    memset(out, 0, PROTOCOL_V1_SIZE);
    out[0] = 0xA5; out[1] = 1; out[2] = 1;
    out[3] = e->kind; out[4] = e->code; out[5] = e->confidence;
    out[6] = e->valid ? 1u : 0u;
    put32(out + 8, e->boot_id); put32(out + 12, e->seq);
    put32(out + 16, e->uptime_ms);
    out[23] = protocol_crc8(out, 23);
}
