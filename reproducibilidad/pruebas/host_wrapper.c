#include "mppt.h"
#include "mppt_control.h"
#include "mppt_config.h"
#include "po_mppt.h"
#include "inc_mppt.h"
#include "meng_vi_mppt.h"
#include "adaptive_meng_mppt.h"
#include "po_with_probing_mppt.h"
#include "adaptive_meng_with_probing_mppt.h"
#include "adaptive_meng_without_probing_mppt.h"
#include <math.h>
#include <string.h>
/* Independent states: allow a test of interleaved controller instances. */
static mppt_state_t states[7];
void review_reset(int kind) { mppt_init(&states[kind], (mppt_kind_t)kind); }
void review_step(int kind,double v,double i,double t,double out[9]) {
    mppt_state_t *s=&states[kind];
    mppt_update_at(s,v,i,t);
    out[0]=s->output.duty; out[1]=s->output.step;out[2]=s->output.hold;
    memcpy(out+3,s->output.diag,6*sizeof(double));
}
int review_status(int kind) { return states[kind].output.status; }
int review_internal_phase(int kind) { return states[kind].output.internal_phase; }
static mppt_control_t controls[7];
void review_control_reset(int kind) { mppt_control_reset(&controls[kind],kind+2); }
int review_control_step(int kind,double v,double i,double out[5]) {
    mppt_control_t *s=&controls[kind];
    int ok=mppt_control_step(s,v,i);
    out[0]=s->base_duty;out[1]=s->stage_duty;out[2]=s->pwm_count;
    out[3]=PWM_PERIOD_TICKS;out[4]=PWM_MAX_COMPARE_TICKS;
    return ok;
}
int review_mode_period(int mode) { return mppt_mode_period_us(mode); }
static mppt_state_t public_states[7];
typedef void (*init_fn)(mppt_state_t *);
typedef double (*update_fn)(mppt_state_t *,double,double);
static init_fn initializers[7]={po_mppt_init,adaptive_meng_mppt_init,
    adaptive_meng_with_probing_mppt_init,inc_mppt_init,meng_vi_mppt_init,
    po_with_probing_mppt_init,adaptive_meng_without_probing_mppt_init};
static init_fn resets[7]={po_mppt_reset,adaptive_meng_mppt_reset,
    adaptive_meng_with_probing_mppt_reset,inc_mppt_reset,meng_vi_mppt_reset,
    po_with_probing_mppt_reset,adaptive_meng_without_probing_mppt_reset};
static update_fn updates[7]={po_mppt_update,adaptive_meng_mppt_update,
    adaptive_meng_with_probing_mppt_update,inc_mppt_update,meng_vi_mppt_update,
    po_with_probing_mppt_update,adaptive_meng_without_probing_mppt_update};
void review_public_init(int kind) { initializers[kind](&public_states[kind]); }
void review_public_reset(int kind) { resets[kind](&public_states[kind]); }
void review_public_update(int kind,double v,double i,double out[9]) {
    mppt_state_t *s=&public_states[kind];
    out[0]=updates[kind](s,v,i);out[1]=s->output.step;out[2]=s->output.hold;
    memcpy(out+3,s->output.diag,6*sizeof(double));
}
