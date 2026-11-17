#ifndef INC_MPPT_H
#define INC_MPPT_H
#include "mppt.h"
#ifdef __cplusplus
extern "C" {
#endif
// Canonical entry name from the seven-algorithm MATLAB delivery.
typedef mppt_state_t inc_mppt_t;
void inc_mppt_init(inc_mppt_t *s);
void inc_mppt_reset(inc_mppt_t *s);
double inc_mppt_update(inc_mppt_t *s,double Vpv_V,double Ipv_A);
#ifdef __cplusplus
}
#endif
#endif
