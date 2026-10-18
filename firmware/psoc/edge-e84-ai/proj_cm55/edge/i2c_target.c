#include "i2c_target.h"
#include "cy_scb_i2c.h"
#include <string.h>
/* BSP maps SCB5 to P17.0 SCL / P17.1 SDA, also named CAM/DISPLAY.
 * No camera/display driver may initialize this SCB in this application. */
#define TARGET_HW CYBSP_I2C_CAM_CONTROLLER_HW
static cy_stc_scb_i2c_context_t context;
static uint8_t latest[PROTOCOL_V1_SIZE], transfer[PROTOCOL_V1_SIZE];
static uint8_t discard[1];
void i2c_target_publish(const radar_event_t *event)
{
    uint8_t next[PROTOCOL_V1_SIZE];
    protocol_serialize(event, next);
    /* Only 24 bytes copied under the lock. Transfer buffer is ISR owned. */
    uint32_t key = Cy_SysLib_EnterCriticalSection();
    memcpy(latest, next, sizeof(latest));
    Cy_SysLib_ExitCriticalSection(key);
}
static void on_event(uint32_t event)
{
    if (event & CY_SCB_I2C_SLAVE_READ_EVENT) {
        memcpy(transfer, latest, sizeof(transfer));
        Cy_SCB_I2C_SlaveConfigReadBuf(TARGET_HW, transfer, sizeof(transfer), &context);
    }
    if (event & CY_SCB_I2C_SLAVE_WRITE_EVENT)
        Cy_SCB_I2C_SlaveConfigWriteBuf(TARGET_HW, discard, sizeof(discard), &context);
}
static void target_irq(void) { Cy_SCB_I2C_Interrupt(TARGET_HW, &context); }
cy_rslt_t i2c_target_init(const radar_event_t *initial)
{
    cy_stc_scb_i2c_config_t config = CYBSP_I2C_CAM_CONTROLLER_config;
    config.i2cMode = CY_SCB_I2C_SLAVE;
    config.slaveAddress = PROTOCOL_V1_ADDRESS;
    config.slaveAddressMask = 0xFE;
    config.acceptAddrInFifo = false;
    config.ackGeneralAddr = false;
    config.useTxFifo = true;
    config.useRxFifo = true;
    i2c_target_publish(initial);
    cy_en_scb_i2c_status_t status = Cy_SCB_I2C_Init(TARGET_HW, &config, &context);
    if (status != CY_SCB_I2C_SUCCESS) return (cy_rslt_t)status;
    const cy_stc_sysint_t irq = {
        .intrSrc = (IRQn_Type)CYBSP_I2C_CAM_CONTROLLER_IRQ,
        .intrPriority = 3u /* Radar GPIO 1 / radar SPI 2 have priority. */
    };
    cy_en_sysint_status_t result = Cy_SysInt_Init(&irq, target_irq);
    if (result != CY_SYSINT_SUCCESS) return (cy_rslt_t)result;
    Cy_SCB_I2C_RegisterEventCallback(TARGET_HW, on_event, &context);
    NVIC_ClearPendingIRQ(irq.intrSrc);
    NVIC_EnableIRQ(irq.intrSrc);
    Cy_SCB_I2C_Enable(TARGET_HW);
    return CY_RSLT_SUCCESS;
}
