#include "protocol_v1.h"
#include <string.h>

static void put_u32le(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

uint8_t protocol_v1_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
    }
    return crc;
}

void protocol_v1_encode(const vision_event_t *event, uint8_t out[VISION_PROTOCOL_SIZE])
{
    memset(out, 0, VISION_PROTOCOL_SIZE);
    out[0] = 0xA5u; out[1] = 1u; out[2] = 2u;
    out[3] = event->kind;
    out[4] = (event->kind == VISION_KIND_NONE) ? 0u : event->code;
    out[5] = (event->kind == VISION_KIND_NONE) ? 255u :
             ((event->confidence <= 100u || event->confidence == 255u) ? event->confidence : 255u);
    out[6] = (event->kind == VISION_KIND_NONE) ? 0u : (event->flags & 0x01u);
    out[7] = 0u;
    put_u32le(&out[8], event->boot_id);
    put_u32le(&out[12], event->seq);
    put_u32le(&out[16], event->uptime_ms);
    out[23] = protocol_v1_crc8(out, 23u);
}

void protocol_snapshot_init(protocol_snapshot_t *s, protocol_lock_fn lock,
                            protocol_unlock_fn unlock, void *context, uint32_t boot_id)
{
    s->lock = lock; s->unlock = unlock; s->context = context;
    const vision_event_t idle = { VISION_KIND_NONE, 0u, 255u, 0u, boot_id, 0u, 0u };
    protocol_v1_encode(&idle, s->bytes);
}

void protocol_snapshot_publish(protocol_snapshot_t *s, const vision_event_t *event)
{
    uint32_t key = s->lock ? s->lock(s->context) : 0u;
    uint8_t next[VISION_PROTOCOL_SIZE];
    protocol_v1_encode(event, next);
    memcpy(s->bytes, next, sizeof(next));
    if (s->unlock) s->unlock(s->context, key);
}

void protocol_snapshot_read(protocol_snapshot_t *s, uint8_t out[VISION_PROTOCOL_SIZE])
{
    uint32_t key = s->lock ? s->lock(s->context) : 0u;
    memcpy(out, s->bytes, VISION_PROTOCOL_SIZE);
    if (s->unlock) s->unlock(s->context, key);
}
