#include "direction_detector.h"
#include "protocol_v1.h"
#include "vision_events.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static person_box_t box_at(float cx)
{
    person_box_t b = { cx - 5.0f, 10.0f, cx + 5.0f, 50.0f, 0.9f };
    return b;
}
static direction_config_t config(void)
{
    direction_config_t c = { 100u, 100u, 4u, 2u, 3u, 0.02f, 0.22f, 0.40f, 0.10f, false };
    return c;
}
static direction_t run_track(const float *xs, size_t n)
{
    direction_tracker_t t; direction_config_t c = config();
    direction_tracker_init(&t, &c);
    direction_t got = DIRECTION_NONE;
    for (size_t i = 0; i < n; ++i) {
        person_box_t b = box_at(xs[i]);
        direction_t d = direction_tracker_update(&t, &b, 1u);
        if (d != DIRECTION_NONE) got = d;
    }
    for (unsigned i = 0; i < 3; ++i) {
        direction_t d = direction_tracker_update(&t, NULL, 0u);
        if (d != DIRECTION_NONE) got = d;
    }
    return got;
}
static uint32_t lock_noop(void *ctx) { (void)ctx; return 0; }
static void unlock_noop(void *ctx, uint32_t key) { (void)ctx; (void)key; }

int main(void)
{
    const uint8_t vector[] = "123456789";
    assert(protocol_v1_crc8(vector, 9u) == 0xF4u);

    vision_event_t e = { VISION_KIND_PASSAGE, VISION_DIR_LEFT_TO_RIGHT, 88u,
                         1u, 0x12345678u, 0x10203040u, 0xA1B2C3D4u };
    uint8_t packet[VISION_PROTOCOL_SIZE];
    protocol_v1_encode(&e, packet);
    assert(packet[0] == 0xA5u && packet[1] == 1u && packet[2] == 2u);
    assert(packet[3] == VISION_KIND_PASSAGE && packet[4] == VISION_DIR_LEFT_TO_RIGHT);
    assert(packet[8] == 0x78u && packet[11] == 0x12u);
    assert(packet[12] == 0x40u && packet[15] == 0x10u);
    assert(packet[16] == 0xD4u && packet[19] == 0xA1u);
    assert(packet[23] == protocol_v1_crc8(packet, 23u));

    const float still[] = { 50, 50, 50, 50, 50 };
    assert(run_track(still, 5) == DIRECTION_NONE); /* presence alone is not a passage */
    const float ltr[] = { 5, 25, 45, 65, 85, 95 };
    const float rtl[] = { 95, 75, 55, 35, 15, 5 };
    const float turn[] = { 5, 30, 55, 40, 25 };
    assert(run_track(ltr, 6) == DIRECTION_LEFT_TO_RIGHT);
    assert(run_track(rtl, 6) == DIRECTION_RIGHT_TO_LEFT);
    assert(run_track(turn, 5) == DIRECTION_UNKNOWN);
    direction_tracker_t mirrored; direction_config_t mc = config(); mc.mirrored_input = true;
    direction_tracker_init(&mirrored, &mc);
    direction_t mirrored_result = DIRECTION_NONE;
    for (size_t i = 0; i < 6; ++i) {
        person_box_t mb = box_at(rtl[i]);
        direction_t md = direction_tracker_update(&mirrored, &mb, 1u);
        if (md != DIRECTION_NONE) mirrored_result = md;
    }
    for (unsigned i = 0; i < 3; ++i) {
        direction_t md = direction_tracker_update(&mirrored, NULL, 0u);
        if (md != DIRECTION_NONE) mirrored_result = md;
    }
    assert(mirrored_result == DIRECTION_LEFT_TO_RIGHT);

    direction_tracker_t t; direction_config_t c = config(); direction_tracker_init(&t, &c);
    person_box_t b = box_at(5); direction_tracker_update(&t, &b, 1u);
    b = box_at(30); direction_tracker_update(&t, &b, 1u);
    b = box_at(55); direction_tracker_update(&t, &b, 1u);
    person_box_t pair[] = { box_at(60), box_at(65) };
    direction_tracker_update(&t, pair, 2u);
    direction_tracker_update(&t, pair, 2u);
    assert(direction_tracker_update(&t, pair, 2u) == DIRECTION_UNKNOWN);

    protocol_snapshot_t snapshot;
    protocol_snapshot_init(&snapshot, lock_noop, unlock_noop, NULL, 0xAABBCCDDu);
    uint8_t readback[VISION_PROTOCOL_SIZE]; protocol_snapshot_read(&snapshot, readback);
    assert(readback[0] == 0xA5 && readback[3] == VISION_KIND_NONE && readback[6] == 0);
    protocol_snapshot_publish(&snapshot, &e); protocol_snapshot_read(&snapshot, readback);
    assert(memcmp(packet, readback, sizeof(packet)) == 0);
    vision_events_t events;
    vision_events_init(&events, &snapshot, 0xAABBCCDDu, lock_noop, unlock_noop, NULL);
    vision_events_publish_passage(&events, DIRECTION_RIGHT_TO_LEFT, 77u, 1234u);
    protocol_snapshot_read(&snapshot, readback);
    assert(readback[3] == VISION_KIND_PASSAGE && readback[4] == VISION_DIR_RIGHT_TO_LEFT);
    assert(readback[5] == 77u && readback[6] == 1u && readback[8] == 0xDDu);
    assert(readback[12] == 1u && readback[13] == 0u && readback[23] == protocol_v1_crc8(readback, 23u));
    uint8_t repeated[VISION_PROTOCOL_SIZE]; protocol_snapshot_read(&snapshot, repeated);
    assert(memcmp(readback, repeated, sizeof(readback)) == 0);
    vision_events_publish_idle(&events, 2345u); protocol_snapshot_read(&snapshot, readback);
    assert(readback[3] == VISION_KIND_NONE && readback[6] == 0u && readback[12] == 1u);
    puts("edge-e84 portable logic tests passed");
    return 0;
}
