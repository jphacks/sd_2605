#include "radar_ai.h"
#include "radar_events.h"
#include "i2c_target.h"
#include "mtb_hal.h"
#include <inttypes.h>
#include <stdio.h>
static radar_events_t events;
static volatile uint32_t uptime_ms;
static void tick(void) { ++uptime_ms; }
static bool entropy(void *unused, uint32_t *value)
{
    (void)unused;
    mtb_hal_trng_t trng;
    if (Cy_Crypto_Core_Enable(CRYPTO) != CY_CRYPTO_SUCCESS) return false;
    if (mtb_hal_trng_setup(&trng, NULL) != CY_RSLT_SUCCESS) return false;
    return mtb_hal_trng_get_uint32(&trng, value) == CY_RSLT_SUCCESS;
}
cy_rslt_t radar_ai_events_init(void)
{
    uint32_t boot;
    if (!radar_boot_id(entropy, NULL, &boot)) {
        printf("[BOOT] TRNG failed; event target not started\r\n");
        return CY_RSLT_TYPE_ERROR;
    }
    /* Bare-metal official application does not use SysTick/RTOS. */
    Cy_SysTick_Init(CY_SYSTICK_CLOCK_SOURCE_CLK_CPU, SystemCoreClock / 1000u);
    Cy_SysTick_SetCallback(0u, tick);
    radar_events_init(&events, boot);
    printf("[BOOT] boot_id=%" PRIu32 "\r\n", boot);
    return i2c_target_init(&events.last);
}
void radar_ai_result(const char *label, float score)
{
    if (!radar_events_observe(&events, label, score, uptime_ms)) return;
    i2c_target_publish(&events.last);
    printf("[RADAR] %s confidence=%u\r\n",
           events.last.code == 1 ? "PUSH" : "CIRCLE", events.last.confidence);
    printf("[I2C] event seq=%" PRIu32 " kind=%u code=%u\r\n",
           events.last.seq, events.last.kind, events.last.code);
}
