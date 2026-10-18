#ifndef EDGE_E84_I2C_TARGET_H
#define EDGE_E84_I2C_TARGET_H

#include "protocol_v1.h"

/* Board glue supplies the generated EZI2C SCB, configuration, and context.
 * Configure the pins to P8[1]/P8[0] (J3.9/J3.10), address 0x31, and 100 kHz
 * in Device Configurator for the selected KIT_PSE84_EVAL_* BSP. */
typedef struct {
    void *scb;
    const void *config;
    void *context;
    protocol_snapshot_t *snapshot;
    uint8_t tx_buffer[VISION_PROTOCOL_SIZE];
} i2c_target_t;

int i2c_target_init(i2c_target_t *target, void *scb, const void *config,
                    void *context, protocol_snapshot_t *snapshot);
void i2c_target_refresh(i2c_target_t *target);

#endif
