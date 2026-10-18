#include "radar_events.h"
#include "i2c_target.h"
#include "cy_scb_i2c.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void (*callback)(uint32_t);
static uint8_t *tx;
static uint32_t tx_size;
static unsigned locked;
const cy_stc_scb_i2c_config_t CYBSP_I2C_CAM_CONTROLLER_config = {0};
uint32_t Cy_SysLib_EnterCriticalSection(void) { assert(!locked); locked = 1; return 0; }
void Cy_SysLib_ExitCriticalSection(uint32_t key) { (void)key; assert(locked); locked = 0; }
cy_en_sysint_status_t Cy_SysInt_Init(const cy_stc_sysint_t *c, void (*h)(void))
{ assert(c->intrSrc == 5 && c->intrPriority == 3 && h); return CY_SYSINT_SUCCESS; }
void NVIC_ClearPendingIRQ(IRQn_Type irq) { assert(irq == 5); }
void NVIC_EnableIRQ(IRQn_Type irq) { assert(irq == 5); }
cy_en_scb_i2c_status_t Cy_SCB_I2C_Init(void *hw, const cy_stc_scb_i2c_config_t *c, cy_stc_scb_i2c_context_t *ctx)
{
    assert(hw == CYBSP_I2C_CAM_CONTROLLER_HW && ctx);
    assert(c->i2cMode == CY_SCB_I2C_SLAVE && c->slaveAddress == 0x30);
    assert(c->slaveAddressMask == 0xFE && !c->ackGeneralAddr && !c->acceptAddrInFifo);
    assert(c->useTxFifo && c->useRxFifo); return CY_SCB_I2C_SUCCESS;
}
void Cy_SCB_I2C_SlaveConfigReadBuf(void *hw, uint8_t *buf, uint32_t n, cy_stc_scb_i2c_context_t *ctx)
{ (void)hw; (void)ctx; tx = buf; tx_size = n; }
void Cy_SCB_I2C_SlaveConfigWriteBuf(void *hw, uint8_t *buf, uint32_t n, cy_stc_scb_i2c_context_t *ctx)
{ (void)hw; (void)ctx; assert(buf && n == 1); }
void Cy_SCB_I2C_Interrupt(void *hw, cy_stc_scb_i2c_context_t *ctx) { (void)hw; (void)ctx; }
void Cy_SCB_I2C_RegisterEventCallback(void *hw, void (*cb)(uint32_t), cy_stc_scb_i2c_context_t *ctx)
{ (void)hw; (void)ctx; callback = cb; }
void Cy_SCB_I2C_Enable(void *hw) { assert(hw == CYBSP_I2C_CAM_CONTROLLER_HW); }
static bool entropy(void *ctx, uint32_t *out) { *out = ++*(uint32_t *)ctx; return true; }
static bool failed_entropy(void *ctx, uint32_t *out) { (void)ctx; (void)out; return false; }
int main(void)
{
    assert(protocol_crc8((const uint8_t *)"123456789", 9) == 0xF4);
    assert(protocol_crc8(NULL, 0) == 0);
    radar_event_t e = {.boot_id=0x12345678, .seq=0x90ABCDEF,
        .uptime_ms=0x10203040, .kind=1, .code=2, .confidence=92, .valid=true};
    uint8_t bytes[24]; protocol_serialize(&e, bytes);
    const uint8_t expected[23] = {0xA5,1,1,1,2,92,1,0,0x78,0x56,0x34,0x12,
        0xEF,0xCD,0xAB,0x90,0x40,0x30,0x20,0x10,0,0,0};
    assert(memcmp(bytes, expected, 23) == 0);
    assert(bytes[23] == protocol_crc8(expected, 23));
    uint32_t seed=99, a, b;
    assert(radar_boot_id(entropy, &seed, &a) && radar_boot_id(entropy, &seed, &b));
    assert(a != b);
    assert(!radar_boot_id(failed_entropy, NULL, &a));
    assert(!radar_boot_id(NULL, NULL, &a));
    radar_events_t s; radar_events_init(&s, b);
    assert(s.last.seq == 0 && !s.last.valid && s.last.kind == 0);
    assert(i2c_target_init(&s.last) == 0);
    callback(1); assert(tx_size == 24 && tx[3] == 0 && tx[6] == 0);
    assert(!radar_events_observe(&s, "Push", NAN, 0));
    assert(!radar_events_observe(&s, "Push", 1.1f, 0));
    assert(!radar_events_observe(&s, "Push", 0.79f, 0));
    assert(!radar_events_observe(&s, "Push", 0.92f, 10));
    assert(radar_events_observe(&s, "Push", 0.92f, 20));
    assert(s.last.seq == 1 && s.last.code == 1 && s.last.confidence == 92);
    for (unsigned i=0; i<100; ++i) assert(!radar_events_observe(&s, "Push", 0.99f, 2000+i));
    assert(s.last.seq == 1);
    i2c_target_publish(&s.last); callback(1); memcpy(bytes, tx, 24);
    radar_events_observe(&s, "unlabelled", 0.9f, 21);
    assert(!radar_events_observe(&s, "Circle", 0.9f, 22));
    assert(!radar_events_observe(&s, "Circle", 0.9f, 23));
    assert(radar_events_observe(&s, "Circle", 0.9f, 1020));
    assert(s.last.seq == 2 && s.last.code == 2 && s.last.kind == 1);
    i2c_target_publish(&s.last);
    /* Publish during an ongoing read: old transfer remains byte-for-byte stable. */
    assert(memcmp(bytes, tx, 24) == 0);
    callback(1); assert(tx[12] == 2 && tx[4] == 2);
    for (unsigned i=0; i<10000; ++i) {
        callback(1); assert(tx_size == 24 && tx[12] == 2);
        assert(tx[23] == protocol_crc8(tx, 23));
    }
    callback(2);
    /* Unsigned millis and sequence rollover. */
    s.armed = true; s.last_ms = UINT32_MAX-500u; s.last.seq = UINT32_MAX;
    s.candidate=1; s.streak=1;
    assert(radar_events_observe(&s, "Push", 0.8f, 600));
    assert(s.last.seq == 0 && s.last.valid);
    radar_event_t retained=s.last;
    assert(!radar_events_observe(&s, "unknown", 0.99f, 700));
    assert(memcmp(&retained, &s.last, sizeof(retained)) == 0);
    puts("PASS: CRC, wire format, boot entropy contract, gesture policy, seq, rollover, 24-byte read callback, transfer isolation, 10000 repeated reads");
}
