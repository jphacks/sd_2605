#include "cybsp.h"
#include "mtb_hal.h"
#include <inttypes.h>
#include <stdio.h>

static void halt(void)
{
    __disable_irq();
    for (;;) { }
}

int main(void)
{
    if (cybsp_init() != CY_RSLT_SUCCESS) halt();
    if (Cy_SCB_UART_Init(USER_UART_HW, &USER_UART_config, NULL)
            != CY_SCB_UART_SUCCESS) halt();
    Cy_SCB_UART_Enable(USER_UART_HW);
    __enable_irq();

    uint32_t sequence = 1;
    for (;;) {
        /* Wait first: HELLO,1 is sent one second after initialization. */
        Cy_SysLib_Delay(1000u);
        char line[24];
        (void)snprintf(line, sizeof(line), "HELLO,%" PRIu32 "\n", sequence);
        Cy_SCB_UART_PutString(USER_UART_HW, line);
        /* RX is routed and enabled, but no received data drives behavior. */
        Cy_SCB_UART_ClearRxFifo(USER_UART_HW);
        if (sequence == UINT32_MAX) halt();
        ++sequence;
    }
}
