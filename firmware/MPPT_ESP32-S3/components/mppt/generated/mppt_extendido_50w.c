/*
 * Academic License - for use in teaching, academic research, and meeting
 * course requirements at degree granting institutions only.  Not for
 * government, commercial, or other organizational use.
 * File: mppt_extendido_50w.c
 *
 * MATLAB Coder version            : 24.2
 * C/C++ source code generated on  : 13-Nov-2026 13:38:48
 */

/* Include Files */
#include "mppt_extendido_50w.h"
#include "mppt_reference_types.h"
#include "rt_nonfinite.h"

/* Function Definitions */
/*
 * Arguments    : mppt_referenceStackData *SD
 * Return Type  : void
 */
void fastLocal_init(mppt_referenceStackData *SD)
{
  SD->pd->b_d_not_empty = false;
  SD->pd->previousDP = 0.0;
  SD->pd->paused = false;
  SD->pd->resume = -1.0;
  SD->pd->b_started = false;
}

/*
 * Arguments    : mppt_referenceStackData *SD
 * Return Type  : void
 */
void fastRule_init(mppt_referenceStackData *SD)
{
  SD->pd->trust_not_empty = false;
}

/*
 * File trailer for mppt_extendido_50w.c
 *
 * [EOF]
 */
