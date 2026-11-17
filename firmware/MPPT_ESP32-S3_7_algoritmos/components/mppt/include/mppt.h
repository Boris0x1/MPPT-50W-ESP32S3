#ifndef MPPT_H
#define MPPT_H
#include <stdint.h>
#include <stdbool.h>
#include "mppt_reference_types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Measurements: volts/amperes. No environmental or output-side measurements. */
typedef enum { MPPT_PO=0, MPPT_ADAPTIVE_MENG=1, MPPT_ADAPTIVE_MENG_WITH_PROBING=2,
    MPPT_INC=3, MPPT_MENG_VI=4, MPPT_PO_WITH_PROBING=5, MPPT_ADAPTIVE_MENG_WITHOUT_PERIODIC_PROBING=6 } mppt_kind_t;
// Unguarded dispatch for reference replay; physical integration uses mppt_update_at.
void mppt_dispatch(mppt_referenceStackData *,double V,double I,double t,int kind,
                   double *D,double *step,double *hold,double diag[6]);
typedef enum { MPPT_OK=0, MPPT_INVALID_SAMPLE=1, MPPT_BAD_TIME=2 } mppt_status_t;
typedef struct {
    double duty;       /* normalized request before hardware tick mapping */
    double step;       /* change of request during this call */
    double hold;       /* original MATLAB holdFlag */
    double diag[6];    /* reported phase, scanning, Vref, trigger, scan type, eligible */
    int internal_phase;/* 5=bounded recovery; original diagnostic reports it as 2 */
    mppt_status_t status;
} mppt_output_t;
typedef struct {
    mppt_referencePersistentData memory; /* all original persistent variables */
    mppt_kind_t kind;
    uint64_t tick;
    double last_time;
    bool has_time;
    mppt_output_t output;
} mppt_state_t;
/* Call init/reset in a single setup task, not concurrently or in an ISR. */
void mppt_init(mppt_state_t *s, mppt_kind_t kind);
void mppt_reset(mppt_state_t *s);
/* One scheduled call per 2 ms. First call is at local time zero. Invalid
 * measurements hold output and freeze algorithm memory; tick still advances.
 * This boundary guard deliberately avoids the NaN-poisoned histories possible
 * in the original local MATLAB functions. Hardware faults must disarm output. */
double mppt_update(mppt_state_t *s, double Vpv, double Ipv);
/* Explicit time, seconds since reset, for deadline-aware integration/replay.
 * Never use absolute microseconds or change time origin without reset. */
double mppt_update_at(mppt_state_t *s, double Vpv, double Ipv, double time_s);
#ifdef __cplusplus
}
#endif
#endif
