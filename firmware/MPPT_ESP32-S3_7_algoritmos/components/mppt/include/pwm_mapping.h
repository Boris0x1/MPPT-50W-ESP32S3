#ifndef PWM_MAPPING_H
#define PWM_MAPPING_H
#include <stdint.h>
/* Only output quantization changes. MPPT double state remains untouched. */
typedef enum { PWM_MODEL_10_BIT=0, PWM_NATIVE_TICKS=1 } pwm_policy_t;
int pwm_period_ticks(uint32_t resolution_hz,uint32_t requested_hz,uint32_t *ticks);
int pwm_compare_ticks(double Dstage,uint32_t period,pwm_policy_t policy,uint32_t *compare);
#endif
