#ifndef UART_LINK_H
#define UART_LINK_H

#include "event_queue.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { UART_LINK_OK, UART_LINK_ERROR } uart_link_result_t;

cy_rslt_t uart_link_init(uint32_t boot_id);
void uart_link_poll(uint32_t now_ms);
void uart_link_dispatch(uint32_t now_ms, event_queue_t *queue);
bool uart_link_has_pending_ack(void);
bool uart_link_has_error(void);

#endif
