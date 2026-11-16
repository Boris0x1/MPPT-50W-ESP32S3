#ifndef COMBINACION_FINAL_H
#define COMBINACION_FINAL_H
#include "mppt.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mppt_state_t combinacion_final_t;
void combinacion_final_init(combinacion_final_t *s);
void combinacion_final_reset(combinacion_final_t *s);
double combinacion_final_update(combinacion_final_t *s, double Vpv_V, double Ipv_A);
#ifdef __cplusplus
}
#endif
#endif
