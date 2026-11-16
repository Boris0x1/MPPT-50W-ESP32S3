/*
 * Academic License - for use in teaching, academic research, and meeting
 * course requirements at degree granting institutions only.  Not for
 * government, commercial, or other organizational use.
 * File: mppt_reference.h
 *
 * MATLAB Coder version            : 24.2
 * C/C++ source code generated on  : 13-Nov-2026 13:38:48
 */

#ifndef MPPT_REFERENCE_H
#define MPPT_REFERENCE_H

/* Include Files */
#include "mppt_reference_types.h"
#include "rtwtypes.h"
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Function Declarations */
extern void mppt_reference(mppt_referenceStackData *SD, double V, double b_I,
                           double t, int kind, double *D, double *step,
                           double *holdFlag, double diag[6]);

#ifdef __cplusplus
}
#endif

#endif
/*
 * File trailer for mppt_reference.h
 *
 * [EOF]
 */
