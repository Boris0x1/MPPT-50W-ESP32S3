/*
 * Academic License - for use in teaching, academic research, and meeting
 * course requirements at degree granting institutions only.  Not for
 * government, commercial, or other organizational use.
 * File: mppt_reference_initialize.c
 *
 * MATLAB Coder version            : 24.2
 * C/C++ source code generated on  : 13-Nov-2026 13:38:48
 */

/* Include Files */
#include "mppt_reference_initialize.h"
#include "mppt_combinacion_mejorada_50w.h"
#include "mppt_extendido_50w.h"
#include "mppt_reference_types.h"
#include "mppt_unificado_50w.h"
#include "rt_nonfinite.h"

/* Function Definitions */
/*
 * Arguments    : mppt_referenceStackData *SD
 * Return Type  : void
 */
void mppt_reference_initialize(mppt_referenceStackData *SD)
{
  rt_InitInfAndNaN();
  localArticle_init(SD);
  fastLocal_init(SD);
  fastRule_init(SD);
  c_mppt_combinacion_mejorada_50w(SD);
  b_fastRule_init(SD);
}

/*
 * File trailer for mppt_reference_initialize.c
 *
 * [EOF]
 */
