#include "cyhal.h"
#include "cybsp.h"
#include "i2c_controller.h"
#include "protocol_v1.h"

#define I2C_TIMEOUT_MS 10u
#define I2C_ATTEMPTS 3u

static cyhal_i2c_t i2c;
static bool initialized;

cy_rslt_t i2c_controller_init(void)
{
    cy_rslt_t result = cyhal_i2c_init(&i2c, CYBSP_I2C_SDA, CYBSP_I2C_SCL, NULL);
    if (result != CY_RSLT_SUCCESS) return result;
    const cyhal_i2c_cfg_t config = { .is_slave = false, .address = 0u, .frequencyhal_hz = 100000u };
    result = cyhal_i2c_configure(&i2c, &config);
    initialized = result == CY_RSLT_SUCCESS;
    return result;
}

i2c_poll_result_t i2c_controller_poll(sensor_record_t *sensor, sensor_event_t *event)
{
    uint8_t frame[PROTOCOL_V1_FRAME_SIZE];
    bool saw_bad_frame = false;
    bool saw_bus_error = false;
    if (!initialized || sensor == NULL || event == NULL) return I2C_POLL_BUS_ERROR;

    for (uint8_t attempt = 0u; attempt < I2C_ATTEMPTS; ++attempt) {
        const cy_rslt_t result = cyhal_i2c_master_read(&i2c, sensor->address, frame, sizeof(frame), I2C_TIMEOUT_MS, true);
        if (result != CY_RSLT_SUCCESS) {
            saw_bus_error = true;
            continue;
        }
        if (protocol_v1_decode(frame, sensor->source, event)) return I2C_POLL_OK;
        saw_bad_frame = true;
        if (protocol_v1_crc8(frame, PROTOCOL_V1_FRAME_SIZE - 1u) != frame[PROTOCOL_V1_FRAME_SIZE - 1u]) {
            ++sensor->crc_errors;
        } else {
            ++sensor->malformed_frames;
        }
    }
    if (saw_bus_error) ++sensor->nack_timeouts;
    if (saw_bad_frame) return I2C_POLL_BAD_FRAME;
    return I2C_POLL_BUS_ERROR;
}
