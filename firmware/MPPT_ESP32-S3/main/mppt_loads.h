#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    bool ready, active;
    uint8_t ohms, gpio;
    uint32_t revision;
    uint64_t switched_before_us, switched_after_us;
} mppt_loads_state_t;

// 10 ohm activo LOW; 20/30 activos HIGH. GPIO exclusivos de este modulo.
esp_err_t mppt_loads_init(void);
esp_err_t mppt_loads_select(int ohms, mppt_loads_state_t *out);
void mppt_loads_get_state(mppt_loads_state_t *out);
// Confirma latch y habilitacion GPIO; no mide resistencia ni conduccion del MOSFET.
