#ifndef MENG_VI_MAS_H
#define MENG_VI_MAS_H
#include "mppt.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mppt_state_t meng_vi_mas_t;
void meng_vi_mas_init(meng_vi_mas_t *s);
void meng_vi_mas_reset(meng_vi_mas_t *s);
double meng_vi_mas_update(meng_vi_mas_t *s, double Vpv_V, double Ipv_A);
#ifdef __cplusplus
}
#endif
#endif
