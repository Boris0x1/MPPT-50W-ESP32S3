#include "pwm_mapping.h"
#include "model_output.h"
#include "mppt_parameters.h"
#include <math.h>
int pwm_period_ticks(uint32_t resolution,uint32_t requested,uint32_t *ticks){
    if(!ticks||!resolution||!requested)return -1;
    double n=floor((double)resolution/requested+.5);
    if(n<2||n>65535)return -1;
    *ticks=(uint32_t)n;return 0;
}
int pwm_compare_ticks(double d,uint32_t period,pwm_policy_t policy,uint32_t *compare){
    if(!compare||period<2||period>65535||!isfinite(d)||d<0||d>.70)return -1;
    double request;
    if(policy==PWM_MODEL_10_BIT)request=model_duty_count(d)/1024.0;
    else if(policy==PWM_NATIVE_TICKS)request=fmin(MPPT_DUTY_MAX,d);
    else return -1;
    uint32_t n=(uint32_t)floor(request*period+.5);
    /* Effective physical duty never exceeds the original upper limit. */
    uint32_t maximum=(uint32_t)floor(MPPT_DUTY_MAX*period);
    if(n>maximum)n=maximum;
    *compare=n;return 0;
}
