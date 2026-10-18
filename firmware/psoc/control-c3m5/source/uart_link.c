#include "cyhal.h"
#include "cybsp.h"
#include "uart_link.h"
#include <stdio.h>
#include <string.h>

#define UART_BAUD 115200u
#define UART_MAX_LINE 64u
#define UART_ACK_TIMEOUT_MS 200u
#define UART_MAX_RETRIES 3u

static cyhal_uart_t uart;
static bool initialized;
static bool link_error;
static bool awaiting_ack;
static uint32_t local_boot_id;
static uint32_t next_seq = 1u;
static uint32_t pending_seq;
static uint32_t sent_at_ms;
static uint8_t retries;
static char rx_line[UART_MAX_LINE];
static size_t rx_length;
static bool discard_line;
static event_queue_t *active_queue;

static bool parse_ack(const char *line, uint32_t *boot_id, uint32_t *seq)
{
    if (strlen(line) < 16u || strncmp(line, "ACK,1,", 6u) != 0) return false;
    uint32_t boot = 0u;
    for (size_t i = 6u; i < 14u; ++i) {
        const char ch = line[i];
        uint8_t digit;
        if (ch >= '0' && ch <= '9') digit = (uint8_t)(ch - '0');
        else if (ch >= 'A' && ch <= 'F') digit = (uint8_t)(ch - 'A' + 10);
        else if (ch >= 'a' && ch <= 'f') digit = (uint8_t)(ch - 'a' + 10);
        else return false;
        boot = (boot << 4) | digit;
    }
    if (line[14] != ',') return false;
    uint32_t sequence = 0u;
    size_t digits = 0u;
    for (size_t i = 15u; line[i] != '\0'; ++i) {
        if (line[i] < '0' || line[i] > '9' || digits == 10u) return false;
        const uint32_t digit = (uint32_t)(line[i] - '0');
        if (sequence > (UINT32_MAX - digit) / 10u) return false;
        sequence = sequence * 10u + digit;
        ++digits;
    }
    if (digits == 0u) return false;
    *boot_id = boot;
    *seq = sequence;
    return true;
}

static bool send_action(event_action_t action, uint32_t seq)
{
    static const char *const names[] = { "LEFT_TO_RIGHT", "RIGHT_TO_LEFT", "REPLAY" };
    char line[UART_MAX_LINE];
    if ((unsigned int)action >= (sizeof(names) / sizeof(names[0]))) return false;
    const int length = snprintf(line, sizeof(line), "EVT,1,%08lX,%lu,%s\n",
                                (unsigned long)local_boot_id, (unsigned long)seq, names[action]);
    if (length <= 0 || (size_t)length >= sizeof(line)) return false;
    for (int i = 0; i < length; ++i) {
        if (cyhal_uart_putc(&uart, (uint8_t)line[i]) != CY_RSLT_SUCCESS) return false;
    }
    return true;
}

static void consume_line(void)
{
    uint32_t boot_id, seq;
    rx_line[rx_length] = '\0';
    if (parse_ack(rx_line, &boot_id, &seq) && awaiting_ack && boot_id == local_boot_id && seq == pending_seq) {
        awaiting_ack = false;
        if (active_queue != NULL) event_queue_pop(active_queue);
    }
    rx_length = 0u;
}

cy_rslt_t uart_link_init(uint32_t boot_id)
{
    local_boot_id = boot_id;
    cy_rslt_t result = cyhal_uart_init(&uart, CYBSP_UART_TX, CYBSP_UART_RX, NC, NC, NULL, NULL);
    if (result != CY_RSLT_SUCCESS) return result;
    const cyhal_uart_cfg_t config = {
        .data_bits = 8u, .stop_bits = 1u, .parity = CYHAL_UART_PARITY_NONE,
        .rx_buffer = NULL, .rx_buffer_size = 0u
    };
    result = cyhal_uart_configure(&uart, &config, NULL);
    if (result == CY_RSLT_SUCCESS) result = cyhal_uart_set_baud(&uart, UART_BAUD, NULL);
    initialized = result == CY_RSLT_SUCCESS;
    link_error = !initialized;
    return result;
}

void uart_link_poll(uint32_t now_ms)
{
    (void)now_ms;
    if (!initialized) return;
    uint8_t byte;
    while (cyhal_uart_readable(&uart) > 0u) {
        if (cyhal_uart_getc(&uart, &byte, 1u) != CY_RSLT_SUCCESS) break;
        if (byte == '\n') {
            if (discard_line) {
                discard_line = false;
                rx_length = 0u;
                continue;
            }
            if (rx_length > 0u && rx_line[rx_length - 1u] == '\r') --rx_length;
            consume_line();
        } else if ((byte >= 0x20u && byte <= 0x7eu) || byte == '\r') {
            if (rx_length + 1u < sizeof(rx_line)) rx_line[rx_length++] = (char)byte;
            else {
                rx_length = 0u;
                discard_line = true;
            }
        } else {
            rx_length = 0u;
            discard_line = true;
        }
    }
}

bool uart_link_has_pending_ack(void)
{
    return awaiting_ack;
}

bool uart_link_has_error(void)
{
    return link_error;
}

void uart_link_dispatch(uint32_t now_ms, event_queue_t *queue)
{
    if (!initialized || queue == NULL) return;
    active_queue = queue;
    uart_link_poll(now_ms);

    if (awaiting_ack) {
        if ((uint32_t)(now_ms - sent_at_ms) < UART_ACK_TIMEOUT_MS) return;
        if (retries >= UART_MAX_RETRIES) {
            awaiting_ack = false; /* Drop the failed item and keep polling sensors. */
            event_queue_pop(queue);
            return;
        }
        ++retries;
        if (!send_action(queue->items[queue->head].action, pending_seq)) link_error = true;
        sent_at_ms = now_ms;
        return;
    }

    queued_event_t event;
    if (!event_queue_peek(queue, &event)) return;
    pending_seq = next_seq++;
    awaiting_ack = true;
    retries = 0u;
    sent_at_ms = now_ms;
    if (!send_action(event.action, pending_seq)) link_error = true;
}
