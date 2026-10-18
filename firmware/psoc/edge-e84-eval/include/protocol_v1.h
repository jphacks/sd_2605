#ifndef EDGE_E84_PROTOCOL_V1_H
#define EDGE_E84_PROTOCOL_V1_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VISION_PROTOCOL_SIZE 24u
#define VISION_I2C_ADDRESS 0x31u
#define VISION_I2C_BUS_HZ 100000u

enum { VISION_KIND_NONE = 0, VISION_KIND_RADAR_GESTURE = 1,
       VISION_KIND_RADAR_MOTION = 2, VISION_KIND_PASSAGE = 3 };
enum { VISION_DIR_LEFT_TO_RIGHT = 1, VISION_DIR_RIGHT_TO_LEFT = 2,
       VISION_DIR_UNKNOWN = 3 };

typedef struct {
    uint8_t kind;
    uint8_t code;
    uint8_t confidence;
    uint8_t flags;
    uint32_t boot_id;
    uint32_t seq;
    uint32_t uptime_ms;
} vision_event_t;

uint8_t protocol_v1_crc8(const uint8_t *data, size_t length);
void protocol_v1_encode(const vision_event_t *event, uint8_t out[VISION_PROTOCOL_SIZE]);

/* Call publish from one writer task; copy from the I2C read path. The caller
 * supplies critical-section hooks so the same code works on host and MCU. */
typedef uint32_t (*protocol_lock_fn)(void *context);
typedef void (*protocol_unlock_fn)(void *context, uint32_t key);
typedef struct {
    uint8_t bytes[VISION_PROTOCOL_SIZE];
    protocol_lock_fn lock;
    protocol_unlock_fn unlock;
    void *context;
} protocol_snapshot_t;

void protocol_snapshot_init(protocol_snapshot_t *snapshot,
                           protocol_lock_fn lock, protocol_unlock_fn unlock,
                           void *context, uint32_t boot_id);
void protocol_snapshot_publish(protocol_snapshot_t *snapshot,
                               const vision_event_t *event);
void protocol_snapshot_read(protocol_snapshot_t *snapshot,
                            uint8_t out[VISION_PROTOCOL_SIZE]);

#endif
