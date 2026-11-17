#include "mppt_control.h"

#include <math.h>
#include <string.h>
#include "mppt_config.h"
#include "pwm_mapping.h"

bool mppt_mode_valid(int mode) { return mode == MODE_STOP || mppt_mode_fast(mode); }
bool mppt_mode_fast(int mode) { return mode >= MODE_PO && mode <= MODE_LAST; }
uint32_t mppt_mode_period_us(int mode) {
    // STOP mantiene una consulta lenta del sensor, sin ejecutar MPPT ni PWM.
    return mppt_mode_fast(mode) ? FAST_PERIOD_US : STOP_ACQUISITION_PERIOD_MS * 1000U;
}

void mppt_control_reset(mppt_control_t *control, int mode)
{
    memset(control, 0, sizeof(*control));
    control->mode = mppt_mode_valid(mode) ? mode : MODE_STOP;
    model_output_init(&control->stage);
    if (mppt_mode_fast(control->mode))
        mppt_init(&control->imported, (mppt_kind_t)(control->mode - MODE_PO));
}

bool mppt_control_step(mppt_control_t *control, double vin, double iin)
{
    if (!isfinite(vin) || !isfinite(iin) || !isfinite(vin * iin) || vin <= 0 || iin < 0)
        return false;
    if (control->mode == MODE_STOP) return true;
    if (mppt_mode_fast(control->mode)) {
        control->base_duty = mppt_update(&control->imported, vin, iin);
        if (control->imported.output.status == MPPT_BAD_TIME) return false;
        if (!control->primed) control->stage_duty = control->base_duty;
        // Un rechazo de V/I congela memorias y etapa comun; el reloj avanza.
        if (control->imported.output.status == MPPT_OK)
            control->stage_duty = model_output_update(&control->stage, vin, control->base_duty);
        // MCPWM nativo: no pasar por la cuantizacion previa de 10 bits.
        if (pwm_compare_ticks(control->stage_duty, PWM_PERIOD_TICKS, PWM_NATIVE_TICKS,
                              &control->pwm_count) != 0) return false;
    } else return false;
    control->primed = true;
    return isfinite(control->stage_duty) && control->pwm_count <= PWM_MAX_COMPARE_TICKS;
}
