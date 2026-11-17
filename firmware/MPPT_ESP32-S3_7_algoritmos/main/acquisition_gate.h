#pragma once

#include "acquisition_worker.h"

typedef enum {
    ACQUISITION_STALE, ACQUISITION_ERROR, ACQUISITION_LATE, ACQUISITION_READY
} acquisition_gate_t;

// deadline_us=0 desactiva solo el limite temporal (preparacion/PWM detenido).
acquisition_gate_t acquisition_gate(const acquisition_result_t *result, uint32_t generation,
                                    uint64_t id, uint64_t deadline_us);
