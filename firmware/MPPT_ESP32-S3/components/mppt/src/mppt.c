#include "mppt.h"
#include "mppt_parameters.h"
#include "mppt_reference.h"
#include "mppt_reference_initialize.h"
#include <math.h>
#include <string.h>
#include <limits.h>
_Static_assert(sizeof(double)==8 && sizeof(int)==4 && CHAR_BIT==8,
               "Generated kernel requires 64-bit double, 32-bit int, 8-bit byte");
void mppt_init(mppt_state_t *s, mppt_kind_t kind) {
    memset(s,0,sizeof(*s));
    s->kind=kind;
    mppt_referenceStackData sd={&s->memory};
    mppt_reference_initialize(&sd);
    s->output.duty=MPPT_INITIAL_DUTY;
}
void mppt_reset(mppt_state_t *s) { mppt_kind_t k=s->kind; mppt_init(s,k); }
double mppt_update_at(mppt_state_t *s,double V,double I,double t) {
    if (!isfinite(t)||t<0||(s->has_time && t<=s->last_time) ||
        s->kind<MPPT_PO||s->kind>MPPT_COMBINACION_FINAL) {
        s->output.status=MPPT_BAD_TIME; s->output.step=0; return s->output.duty;
    }
    s->last_time=t; s->has_time=true;
    if (!isfinite(V)||!isfinite(I)||!isfinite(V*I)||V<.1||I<.001) {
        s->output.status=MPPT_INVALID_SAMPLE; s->output.step=0; return s->output.duty;
    }
    mppt_referenceStackData sd={&s->memory};
    mppt_reference(&sd,V,I,t,(int)s->kind,&s->output.duty,&s->output.step,
                   &s->output.hold,s->output.diag);
    s->output.internal_phase=(s->kind==MPPT_COMBINACION_FINAL)?(int)s->memory.phase:4;
    s->output.status=MPPT_OK;
    return s->output.duty;
}
double mppt_update(mppt_state_t *s,double V,double I) {
    double t=(double)s->tick*0.002; s->tick++;
    return mppt_update_at(s,V,I,t);
}
