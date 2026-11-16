#ifndef MPPT_PARAMETERS_H
#define MPPT_PARAMETERS_H
/* Model values. Generated kernel embeds these same frozen constants.
 * Changing macros alone DOES NOT retune the generated kernel. Regenerate and
 * repeat equivalence tests for any intentional new configuration. */
#define MPPT_PERIOD_US 2000ULL
#define MPPT_INITIAL_DUTY 0.4
#define MPPT_DUTY_MAX (716.0/1024.0)
#define MPPT_DUTY_MIN 0.0
#define MPPT_START_S 0.020
#define MPPT_PO_STEP 0.004
#define MPPT_MENG_KAPPA 0.032
#define MPPT_COMB_KAPPA 0.024
#define MPPT_VM_V 18.25
#define MPPT_IM_A 2.74
#define MPPT_MODEL_PWM_HZ 75000U
#define MPPT_MODEL_BITS 10U
#endif
