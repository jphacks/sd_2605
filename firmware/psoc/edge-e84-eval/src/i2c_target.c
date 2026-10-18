#include "i2c_target.h"

/* Only this board adapter depends on ModusToolbox PDL. */
#include "cy_scb_ezi2c.h"

int i2c_target_init(i2c_target_t *t, void *scb, const void *config,
                    void *context, protocol_snapshot_t *snapshot)
{
    t->scb = scb; t->config = config; t->context = context; t->snapshot = snapshot;
    protocol_snapshot_read(snapshot, t->tx_buffer);
    if (Cy_SCB_EZI2C_Init((SCB_Type *)scb, (const cy_stc_scb_ezi2c_config_t *)config,
                          (cy_stc_scb_ezi2c_context_t *)context) != CY_SCB_EZI2C_SUCCESS)
        return -1;
    Cy_SCB_EZI2C_SetBuffer1((SCB_Type *)scb, t->tx_buffer, sizeof(t->tx_buffer),
                            0u, (cy_stc_scb_ezi2c_context_t *)context);
    Cy_SCB_EZI2C_Enable((SCB_Type *)scb);
    return 0;
}

void i2c_target_refresh(i2c_target_t *t)
{
    /* Call from the cooperative event loop/task, never from the ISR. The copy
     * is atomic with respect to the writer through protocol_snapshot hooks. */
    protocol_snapshot_read(t->snapshot, t->tx_buffer);
}
