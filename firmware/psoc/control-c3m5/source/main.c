#include "cyhal.h"
#include "cybsp.h"
#include "event_fusion.h"
#include "event_queue.h"
#include "i2c_controller.h"
#include "sensor_registry.h"
#include "uart_link.h"

#define POLL_INTERVAL_MS 50u
#define LED_EVENT_MS 120u

typedef enum { HUB_INIT, HUB_RUNNING, HUB_SENSOR_ERROR, HUB_UART_ERROR } hub_state_t;

static sensor_record_t sensors[2];
static event_queue_t events;
static hub_state_t state = HUB_INIT;
static uint32_t led_event_until;
static uint32_t poll_due;
static bool i2c_ready;
static cyhal_lptimer_t system_timer;
static uint32_t system_timer_hz;

static uint32_t hub_time_ms(void)
{
    static uint32_t previous_ticks;
    static uint64_t elapsed_ticks;
    const uint32_t ticks = cyhal_lptimer_read(&system_timer);
    elapsed_ticks += (uint32_t)(ticks - previous_ticks);
    previous_ticks = ticks;
    return (uint32_t)((elapsed_ticks * 1000u) / system_timer_hz);
}

static bool make_boot_id(uint32_t *boot_id)
{
    cyhal_trng_t trng;
    if (boot_id == NULL || cyhal_trng_init(&trng) != CY_RSLT_SUCCESS) return false;
    uint32_t value = cyhal_trng_generate(&trng);
    cyhal_trng_free(&trng);
    *boot_id = value == 0u ? 1u : value;
    return true;
}

static void led_set(uint32_t port, uint32_t pin, bool on)
{
    Cy_GPIO_Write(port, pin, on ? CYBSP_LED_STATE_ON : CYBSP_LED_STATE_OFF);
}

static void update_leds(uint32_t now_ms)
{
    static uint32_t last_toggle;
    static bool heartbeat;
    if ((uint32_t)(now_ms - last_toggle) >= 500u) {
        last_toggle = now_ms;
        heartbeat = !heartbeat;
    }
    led_set(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN, state == HUB_RUNNING || state == HUB_SENSOR_ERROR ? heartbeat : false);
    led_set(CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_PIN, (int32_t)(led_event_until - now_ms) > 0);
}

int main(void)
{
    cy_rslt_t result = cybsp_init();
    if (result != CY_RSLT_SUCCESS) CY_ASSERT(0);
    result = cyhal_lptimer_init(&system_timer);
    if (result != CY_RSLT_SUCCESS) CY_ASSERT(0);
    cyhal_lptimer_info_t timer_info;
    cyhal_lptimer_get_info(&system_timer, &timer_info);
    system_timer_hz = timer_info.frequency_hz;
    if (system_timer_hz == 0u) CY_ASSERT(0);
    __enable_irq();

    event_queue_init(&events);
    sensor_registry_init(sensors);
    const cy_rslt_t i2c_result = i2c_controller_init();
    i2c_ready = i2c_result == CY_RSLT_SUCCESS;
    uint32_t boot_id;
    const bool boot_id_ready = make_boot_id(&boot_id);
    const cy_rslt_t uart_result = boot_id_ready ? uart_link_init(boot_id) : 0u;
    if (!boot_id_ready || uart_result != CY_RSLT_SUCCESS) state = HUB_UART_ERROR;
    else if (!i2c_ready) state = HUB_SENSOR_ERROR;
    else state = HUB_RUNNING;

    for (;;) {
        const uint32_t now_ms = hub_time_ms();
        if (i2c_ready && (uint32_t)(now_ms - poll_due) < 0x80000000u) {
            poll_due = now_ms + POLL_INTERVAL_MS;
            bool any_ok = false;
            for (uint8_t i = 0u; i < 2u; ++i) {
                sensor_event_t event;
                const i2c_poll_result_t poll = i2c_controller_poll(&sensors[i], &event);
                if (poll == I2C_POLL_OK) {
                    any_ok = true;
                    if (sensor_registry_accept(&sensors[i], &event, now_ms)) {
                        if (event_fusion_handle(&event, now_ms, &events)) led_event_until = now_ms + LED_EVENT_MS;
                    }
                } else {
                    if (poll == I2C_POLL_BAD_FRAME) {
                        sensors[i].available = true;
                        sensors[i].last_receive_ms = now_ms;
                    } else {
                        sensors[i].available = false;
                    }
                }
            }
            if (!uart_link_has_pending_ack() && state != HUB_UART_ERROR) {
                state = any_ok ? HUB_RUNNING : HUB_SENSOR_ERROR;
            }
        }

        uart_link_dispatch(now_ms, &events);
        if (uart_link_has_error()) state = HUB_UART_ERROR;
        update_leds(now_ms);
        cyhal_system_delay_ms(1u);
    }
}
