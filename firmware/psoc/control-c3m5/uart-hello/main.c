#include "cybsp.h"
#include "mtb_hal.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static volatile uint32_t milliseconds;
void SysTick_Handler(void) { ++milliseconds; }
static void halt(void) { __disable_irq(); for (;;) { } }

static bool parse_ack(const char *line, uint32_t *value)
{
    if (strncmp(line, "ACK,", 4) != 0 || line[4] == '\0') return false;
    uint32_t result = 0;
    for (const char *p = line + 4; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        uint32_t digit = (uint32_t)(*p - '0');
        if (result > (UINT32_MAX - digit) / 10u) return false;
        result = result * 10u + digit;
    }
    *value = result;
    return true;
}

int main(void)
{
    if (cybsp_init() != CY_RSLT_SUCCESS) halt();
    if (Cy_SCB_UART_Init(USER_UART_HW, &USER_UART_config, NULL) != CY_SCB_UART_SUCCESS) halt();
    if (Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, NULL) != CY_SCB_UART_SUCCESS) halt();
    Cy_SCB_UART_Enable(USER_UART_HW);
    Cy_SCB_UART_Enable(DEBUG_UART_HW);
    if (SysTick_Config(SystemCoreClock / 1000u) != 0u) halt();
    __enable_irq();
    Cy_SCB_UART_PutString(DEBUG_UART_HW,
        "[START] external=SCB4 TX=P3.3 RX=P3.2 debug=SCB3 115200 8N1 timeout=500ms\r\n");
    uint32_t sequence = 1, last_send = milliseconds, sent_at = 0;
    bool waiting = false;
    char line[32];
    size_t length = 0;
    bool discard = false;
    for (;;) {
        uint32_t now = milliseconds;
        if ((uint32_t)(now - last_send) >= 1000u) {
            last_send += 1000u;
            /* Discard stale/partial input before opening a new transaction. */
            Cy_SCB_UART_ClearRxFifo(USER_UART_HW);
            length = 0;
            discard = false;
            char hello[24];
            (void)snprintf(hello, sizeof(hello), "HELLO,%" PRIu32 "\n", sequence);
            Cy_SCB_UART_PutString(USER_UART_HW, hello);
            sent_at = milliseconds;
            waiting = true;
        }
        /* Bound each pass even if the peer streams continuously. */
        for (unsigned count = 0; count < 64u; ++count) {
            uint32_t byte = Cy_SCB_UART_Get(USER_UART_HW);
            if (byte == CY_SCB_UART_RX_NO_DATA) break;
            if (byte == '\n') {
                line[length] = '\0';
                uint32_t received;
                char log[112];
                if (!discard && parse_ack(line, &received)) {
                    if (!waiting || (uint32_t)(milliseconds - sent_at) >= 500u) {
                        (void)snprintf(log, sizeof(log), "[LATE/UNEXPECTED] ACK,%" PRIu32 "\r\n", received);
                    } else if (received != sequence) {
                        (void)snprintf(log, sizeof(log), "[MISMATCH] expected=%" PRIu32 " received=%" PRIu32 "\r\n", sequence, received);
                    } else {
                        (void)snprintf(log, sizeof(log), "[OK] ACK,%" PRIu32 " elapsed=%" PRIu32 "ms\r\n", received, (uint32_t)(milliseconds - sent_at));
                        waiting = false;
                        ++sequence; /* Defined uint32 wrap; reset starts at 1. */
                    }
                } else {
                    (void)snprintf(log, sizeof(log), "[INVALID] malformed/overlong ACK\r\n");
                }
                Cy_SCB_UART_PutString(DEBUG_UART_HW, log);
                length = 0;
                discard = false;
            } else if (byte < 0x20u || byte > 0x7eu || length == sizeof(line) - 1u) {
                discard = true;
            } else if (!discard) {
                line[length++] = (char)byte;
            }
        }
        if (waiting && (uint32_t)(milliseconds - sent_at) >= 500u) {
            char log[64];
            (void)snprintf(log, sizeof(log), "[TIMEOUT] expected=%" PRIu32 " after=500ms\r\n", sequence);
            Cy_SCB_UART_PutString(DEBUG_UART_HW, log);
            waiting = false;
            ++sequence;
        }
    }
}
