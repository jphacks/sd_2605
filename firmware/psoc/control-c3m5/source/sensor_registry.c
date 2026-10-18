#include "sensor_registry.h"

void sensor_registry_init(sensor_record_t records[2])
{
    records[0] = (sensor_record_t){ .address = 0x30u, .source = SOURCE_RADAR };
    records[1] = (sensor_record_t){ .address = 0x31u, .source = SOURCE_VISION };
}

bool sensor_registry_accept(sensor_record_t *record, const sensor_event_t *event, uint32_t receive_ms)
{
    if (record == NULL || event == NULL) return false;
    record->available = true;
    record->last_receive_ms = receive_ms;
    record->last_kind = event->kind;
    record->last_code = event->code;
    if (record->has_identity && record->last_boot_id == event->boot_id && record->last_seq == event->seq) return false;
    record->last_boot_id = event->boot_id;
    record->last_seq = event->seq;
    record->has_identity = true;
    return true;
}
