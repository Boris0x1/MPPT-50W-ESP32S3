#ifndef ADAPTIVE_MENG_MPPT_H
#define ADAPTIVE_MENG_MPPT_H
#include "mppt.h"
#ifdef __cplusplus
extern "C" {
#endif
// Canonical entry name from the seven-algorithm MATLAB delivery.
typedef mppt_state_t adaptive_meng_mppt_t;
void adaptive_meng_mppt_init(adaptive_meng_mppt_t *s);
void adaptive_meng_mppt_reset(adaptive_meng_mppt_t *s);
double adaptive_meng_mppt_update(adaptive_meng_mppt_t *s,double Vpv_V,double Ipv_A);
#ifdef __cplusplus
}
#endif
#endif
