#ifndef I2C_CONTROLLER_H
#define I2C_CONTROLLER_H

#include "cy_result.h"
#include "sensor_registry.h"
#include <stdint.h>

typedef enum {
    I2C_POLL_OK,
    I2C_POLL_BAD_FRAME,
    I2C_POLL_BUS_ERROR
} i2c_poll_result_t;

cy_rslt_t i2c_controller_init(void);
i2c_poll_result_t i2c_controller_poll(sensor_record_t *sensor, sensor_event_t *event);

#endif
