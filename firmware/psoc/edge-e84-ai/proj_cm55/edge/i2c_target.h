#pragma once
#include "protocol_v1.h"
#include "cybsp.h"
cy_rslt_t i2c_target_init(const radar_event_t *initial);
/* Single CM55 foreground writer. Never call from DMA/another core. */
void i2c_target_publish(const radar_event_t *event);
