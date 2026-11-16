/*
 * Academic License - for use in teaching, academic research, and meeting
 * course requirements at degree granting institutions only.  Not for
 * government, commercial, or other organizational use.
 * File: mppt_unificado_50w.h
 *
 * MATLAB Coder version            : 24.2
 * C/C++ source code generated on  : 13-Nov-2026 13:38:48
 */

#ifndef MPPT_UNIFICADO_50W_H
#define MPPT_UNIFICADO_50W_H

/* Include Files */
#include "mppt_reference_types.h"
#include "rtwtypes.h"
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Function Declarations */
void localArticle_init(mppt_referenceStackData *SD);

boolean_T quietWindow(const double v[32], const double p[32],
                      const double d[32], double next, double *V, double *P,
                      double *D);

#ifdef __cplusplus
}
#endif

#endif
/*
 * File trailer for mppt_unificado_50w.h
 *
 * [EOF]
 */
