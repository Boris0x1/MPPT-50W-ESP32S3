#pragma once

#include "mppt.h"
#include "model_output.h"

// Numeros del protocolo serie; no son los indices internos de la biblioteca.
enum {
    MODE_STOP = 0,
    // ID 1 retirado (P&O de 40 ms); no reutilizar en registros/protocolo.
    MODE_PO = 2,
    MODE_ADAPTIVE_MENG = 3,
    MODE_ADAPTIVE_MENG_WITH_PROBING = 4,
    MODE_INC = 5,
    MODE_MENG_VI = 6,
    MODE_PO_WITH_PROBING = 7,
    MODE_ADAPTIVE_MENG_WITHOUT_PERIODIC_PROBING = 8,
    MODE_LAST = MODE_ADAPTIVE_MENG_WITHOUT_PERIODIC_PROBING,
};

typedef struct {
    mppt_state_t imported;
    model_output_t stage;
    int mode;
    bool primed;
    double base_duty, stage_duty;
    uint32_t pwm_count;
} mppt_control_t;

bool mppt_mode_valid(int mode);
bool mppt_mode_fast(int mode);
uint32_t mppt_mode_period_us(int mode);
void mppt_control_reset(mppt_control_t *control, int mode);
// Unica instancia de control, invocada por una sola tarea.
bool mppt_control_step(mppt_control_t *control, double vin, double iin);
