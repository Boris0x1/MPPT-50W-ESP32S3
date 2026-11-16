/*
 * Academic License - for use in teaching, academic research, and meeting
 * course requirements at degree granting institutions only.  Not for
 * government, commercial, or other organizational use.
 * File: mppt_combinacion_mejorada_50w.h
 *
 * MATLAB Coder version            : 24.2
 * C/C++ source code generated on  : 13-Nov-2026 13:38:48
 */

#ifndef MPPT_COMBINACION_MEJORADA_50W_H
#define MPPT_COMBINACION_MEJORADA_50W_H

/* Include Files */
#include "mppt_reference_types.h"
#include "rtwtypes.h"
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Function Declarations */
void b_fastRule_init(mppt_referenceStackData *SD);

void c_mppt_combinacion_mejorada_50w(mppt_referenceStackData *SD);

double mppt_combinacion_mejorada_50w(mppt_referenceStackData *SD, double V,
                                     double b_I, double t, double *step,
                                     double *holdFlag, double diag[6]);

#ifdef __cplusplus
}
#endif

#endif
/*
 * File trailer for mppt_combinacion_mejorada_50w.h
 *
 * [EOF]
 */
