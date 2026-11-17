#ifndef MENG_VI_MPPT_H
#define MENG_VI_MPPT_H
#include "mppt.h"
#ifdef __cplusplus
extern "C" {
#endif
// Canonical entry name from the seven-algorithm MATLAB delivery.
typedef mppt_state_t meng_vi_mppt_t;
void meng_vi_mppt_init(meng_vi_mppt_t *s);
void meng_vi_mppt_reset(meng_vi_mppt_t *s);
double meng_vi_mppt_update(meng_vi_mppt_t *s,double Vpv_V,double Ipv_A);
#ifdef __cplusplus
}
#endif
#endif
