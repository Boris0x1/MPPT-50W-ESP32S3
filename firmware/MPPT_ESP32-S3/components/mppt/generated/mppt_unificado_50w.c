/*
 * Academic License - for use in teaching, academic research, and meeting
 * course requirements at degree granting institutions only.  Not for
 * government, commercial, or other organizational use.
 * File: mppt_unificado_50w.c
 *
 * MATLAB Coder version            : 24.2
 * C/C++ source code generated on  : 13-Nov-2026 13:38:48
 */

/* Include Files */
#include "mppt_unificado_50w.h"
#include "minOrMax.h"
#include "mod.h"
#include "mppt_reference_types.h"
#include "rt_nonfinite.h"
#include "rt_nonfinite.h"
#include <math.h>

/* Function Definitions */
/*
 * Arguments    : mppt_referenceStackData *SD
 * Return Type  : void
 */
void localArticle_init(mppt_referenceStackData *SD)
{
  SD->pd->d_not_empty = false;
  SD->pd->started = false;
}

/*
 * Arguments    : const double v[32]
 *                const double p[32]
 *                const double d[32]
 *                double next
 *                double *V
 *                double *P
 *                double *D
 * Return Type  : boolean_T
 */
boolean_T quietWindow(const double v[32], const double p[32],
                      const double d[32], double next, double *V, double *P,
                      double *D)
{
  double earlier;
  double later;
  double sumD;
  double sumOldP;
  double sumOldV;
  double sumP;
  double sumV;
  int k;
  boolean_T quiet;
  sumOldV = 0.0;
  sumOldP = 0.0;
  sumV = 0.0;
  sumP = 0.0;
  sumD = 0.0;
  for (k = 0; k < 16; k++) {
    earlier = b_mod((next - 1.0) + (double)k) + 1.0;
    later = b_mod((next + 15.0) + (double)k) + 1.0;
    sumOldV += v[(int)earlier - 1];
    sumOldP += p[(int)earlier - 1];
    sumV += v[(int)later - 1];
    sumP += p[(int)later - 1];
    sumD += d[(int)later - 1];
  }
  *V = sumV / 16.0;
  *P = sumP / 16.0;
  *D = sumD / 16.0;
  sumV = 0.011 * *P;
  if ((sumV <= 0.09) || rtIsNaN(sumV)) {
    sumV = 0.09;
  }
  if (fabs(*P - sumOldP / 16.0) <= sumV) {
    sumV = 0.011 * *V;
    if ((sumV <= 0.07) || rtIsNaN(sumV)) {
      sumV = 0.07;
    }
    if ((fabs(*V - sumOldV / 16.0) <= sumV) &&
        (maximum(d) - minimum(d) <= 0.016) &&
        (maximum(v) - minimum(v) <= 0.8)) {
      quiet = true;
    } else {
      quiet = false;
    }
  } else {
    quiet = false;
  }
  return quiet;
}

/*
 * File trailer for mppt_unificado_50w.c
 *
 * [EOF]
 */
