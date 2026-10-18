/* SPDX-License-Identifier: Apache-2.0 */
#include "cybsp.h"
#include "cy_scb_i2c.h"
#include "retarget_io_init.h"
#include <stdio.h>

#define TARGET_HW CYBSP_I2C_CAM_CONTROLLER_HW
static cy_stc_scb_i2c_context_t context;
/* PDL requires writable pointers; contents are never changed. */
static uint8_t reply[] = {0xA5u, 0x01u, 0x30u, 0x5Au};
static uint8_t discard[4];
static volatile uint32_t reads, writes, errors, short_reads;

static void arm_read(void)
{
    (void)Cy_SCB_I2C_SlaveClearReadStatus(TARGET_HW, &context);
    Cy_SCB_I2C_SlaveConfigReadBuf(TARGET_HW, reply, sizeof(reply), &context);
}

static void arm_write(void)
{
    (void)Cy_SCB_I2C_SlaveClearWriteStatus(TARGET_HW, &context);
    Cy_SCB_I2C_SlaveConfigWriteBuf(TARGET_HW, discard, sizeof(discard), &context);
}

static void on_event(uint32_t event)
{
    /* PDL handles FIFO cleanup, STOP/repeated START and hardware error reset.
     * Collect completion information before rearming resets the counters. */
    if (event & CY_SCB_I2C_SLAVE_RD_CMPLT_EVENT)
    {
        uint32_t status = Cy_SCB_I2C_SlaveGetStatus(TARGET_HW, &context);
        if ((Cy_SCB_I2C_SlaveGetReadTransferCount(TARGET_HW, &context) == sizeof(reply)) &&
            !(status & CY_SCB_I2C_SLAVE_RD_UNDRFL) &&
            !(event & CY_SCB_I2C_SLAVE_ERR_EVENT)) ++reads;
        else ++short_reads;
        arm_read();
    }
    if (event & CY_SCB_I2C_SLAVE_WR_CMPLT_EVENT) { ++writes; arm_write(); }
    if (event & CY_SCB_I2C_SLAVE_ERR_EVENT) { ++errors; arm_read(); arm_write(); }
    /* Reset the index on every address match, including after a partial read. */
    if (event & CY_SCB_I2C_SLAVE_READ_EVENT) arm_read();
    if (event & CY_SCB_I2C_SLAVE_WRITE_EVENT) arm_write();
}

static void target_irq(void) { Cy_SCB_I2C_Interrupt(TARGET_HW, &context); }

int main(void)
{
    if (cybsp_init() != CY_RSLT_SUCCESS) handle_app_error();
    init_retarget_io();
    /* Configurator sets SCB5 to Target, address 48 decimal (0x30).
     * Do not initialize any camera/display/sensor controller on this SCB. */
    if (CYBSP_I2C_CAM_CONTROLLER_config.i2cMode != CY_SCB_I2C_SLAVE ||
        CYBSP_I2C_CAM_CONTROLLER_config.slaveAddress != 0x30u)
        handle_app_error();
    if (Cy_SCB_I2C_Init(TARGET_HW, &CYBSP_I2C_CAM_CONTROLLER_config, &context)
        != CY_SCB_I2C_SUCCESS) handle_app_error();
    const cy_stc_sysint_t irq = {
        .intrSrc = CYBSP_I2C_CAM_CONTROLLER_IRQ, .intrPriority = 3u
    };
    if (Cy_SysInt_Init(&irq, target_irq) != CY_SYSINT_SUCCESS) handle_app_error();
    arm_read(); arm_write();
    Cy_SCB_I2C_RegisterEventCallback(TARGET_HW, on_event, &context);
    NVIC_ClearPendingIRQ(irq.intrSrc);
    NVIC_EnableIRQ(irq.intrSrc);
    Cy_SCB_I2C_Enable(TARGET_HW);
    __enable_irq();
    printf("[STEP3A] ready SCB5 P17.0=SCL P17.1=SDA addr=0x30 A5 01 30 5A\r\n");
    for (;;)
    {
        /* Keep peripheral clocks active. No sleep, inference or logging in ISR. */
        Cy_SysLib_Delay(1000u);
        uint32_t key = Cy_SysLib_EnterCriticalSection();
        uint32_t r=reads, w=writes, e=errors, s=short_reads;
        Cy_SysLib_ExitCriticalSection(key);
        printf("[STEP3A] reads4=%lu writes=%lu errors=%lu other_reads=%lu\r\n",
               (unsigned long)r, (unsigned long)w, (unsigned long)e, (unsigned long)s);
    }
}
