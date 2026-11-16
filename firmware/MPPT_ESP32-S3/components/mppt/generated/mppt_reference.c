/*
 * Academic License - for use in teaching, academic research, and meeting
 * course requirements at degree granting institutions only.  Not for
 * government, commercial, or other organizational use.
 * File: mppt_reference.c
 *
 * MATLAB Coder version            : 24.2
 * C/C++ source code generated on  : 13-Nov-2026 13:38:48
 */

/* Include Files */
#include "mppt_reference.h"
#include "mppt_combinacion_mejorada_50w.h"
#include "mppt_reference_types.h"
#include "rt_nonfinite.h"
#include "rt_nonfinite.h"
#include <math.h>

/* Function Definitions */
/*
 * Dispatch only: exact archived functions and simulation configurations.
 *
 * Arguments    : mppt_referenceStackData *SD
 *                double V
 *                double b_I
 *                double t
 *                int kind
 *                double *D
 *                double *step
 *                double *holdFlag
 *                double diag[6]
 * Return Type  : void
 */
void mppt_reference(mppt_referenceStackData *SD, double V, double b_I, double t,
                    int kind, double *D, double *step, double *holdFlag,
                    double diag[6])
{
  static const signed char b_diag[6] = {4, 0, 0, 0, 0, 0};
  double P_tmp;
  double d;
  double direction;
  double dp;
  double dv;
  double old;
  int b_direction;
  boolean_T newLocal;
  if (kind == 0) {
    /*  MPPT at 2 ms. V and I are the only measurements. t is a local clock. */
    /*  Diagnostic phases: 0 wait, 1 scan, 2 recover, 3 hold, 4 refine. */
    /*  cfg(9)=0 is reserved for an ablation without global search. */
    /*  cfg(10) is the scout interval in seconds. Zero disables only scouts. */
    /*  diag(5) gives the exploration type: 1 full, 2 short scout, 3 deep scout.
     */
    /*  cfg(11): 0 PO, 1 INC, 2 Meng literal, 3 original scout, 4 integrated. */
    /*  cfg(12): two-step gate; cfg(13): adaptive V/I step. */
    /*  cfg(14): integration step cap (literal Meng always uses 0.05). */
    /*  mode 5: pilot with a 0.00035 floor; mode 6 retains the paper floor
     * 0.002. */
    /*  Both are normalized by nameplate resistance cfg(15); pauses last one
     * tick. */
    /*  Direct transcription of Algorithm 2 (p.5332), with section 3.3 step. */
    /*  Startup seed and singularity guard are implementation conventions. */
    if (!SD->pd->d_not_empty) {
      SD->pd->d = 0.4;
      SD->pd->d_not_empty = true;
      SD->pd->v1 = V;
      SD->pd->p1 = V * b_I;
    }
    dv = V * b_I;
    dp = dv - SD->pd->p1;
    old = SD->pd->d;
    b_direction = 0;
    if ((t >= 0.02) && ((!rtIsInf(dv)) && (!rtIsNaN(dv))) && (V >= 0.1) &&
        (b_I >= 0.001)) {
      if (!SD->pd->started) {
        b_direction = -1;
        SD->pd->started = true;
      } else if (dp != 0.0) {
        if ((dp > 0.0) == (V - SD->pd->v1 > 0.0)) {
          b_direction = -1;
        } else {
          b_direction = 1;
        }
      }
      d = SD->pd->d + (double)b_direction * 0.004;
      if ((d <= 0.0) || rtIsNaN(d)) {
        d = 0.0;
      }
      if (d >= 0.69921875) {
        SD->pd->d = 0.69921875;
      } else {
        SD->pd->d = d;
      }
    }
    SD->pd->p1 = dv;
    SD->pd->v1 = V;
    *D = SD->pd->d;
    *step = SD->pd->d - old;
    if (SD->pd->started && (b_direction == 0)) {
      newLocal = true;
    } else {
      newLocal = false;
    }
    *holdFlag = newLocal;
    for (b_direction = 0; b_direction < 6; b_direction++) {
      diag[b_direction] = b_diag[b_direction];
    }
  } else if (kind == 1) {
    /*  MPPT at 2 ms. V and I are the only measurements. t is a local clock. */
    /*  Diagnostic phases: 0 wait, 1 scan, 2 recover, 3 hold, 4 refine. */
    /*  cfg(9)=0 is reserved for an ablation without global search. */
    /*  cfg(10) is the scout interval in seconds. Zero disables only scouts. */
    /*  diag(5) gives the exploration type: 1 full, 2 short scout, 3 deep scout.
     */
    /*  cfg(11): 0 PO, 1 INC, 2 Meng literal, 3 original scout, 4 integrated. */
    /*  cfg(12): two-step gate; cfg(13): adaptive V/I step. */
    /*  cfg(14): integration step cap (literal Meng always uses 0.05). */
    /*  mode 5: pilot with a 0.00035 floor; mode 6 retains the paper floor
     * 0.002. */
    /*  Both are normalized by nameplate resistance cfg(15); pauses last one
     * tick. */
    /*  Own extension of the two-increment/V-I rule; not the Meng replica. */
    if (!SD->pd->b_d_not_empty) {
      SD->pd->b_d = 0.4;
      SD->pd->b_d_not_empty = true;
      SD->pd->previousV = V;
      SD->pd->previousP = V * b_I;
    }
    old = SD->pd->b_d;
    P_tmp = V * b_I;
    direction = 0.0;
    if ((t >= 0.02) && ((!rtIsInf(P_tmp)) && (!rtIsNaN(P_tmp))) && (V >= 0.1) &&
        (b_I >= 0.001)) {
      newLocal = !SD->pd->b_started;
      /*  V/I normalization, measured normalized slope and a bounded trust
       * factor. */
      /*  All decisions use current/past V and I plus fixed nameplate constants.
       */
      if ((!SD->pd->trust_not_empty) || newLocal) {
        SD->pd->trust = 1.0;
        SD->pd->trust_not_empty = true;
        SD->pd->lastDirection = -1.0;
      }
      dp = P_tmp - SD->pd->previousP;
      dv = V - SD->pd->previousV;
      if (newLocal) {
        direction = -1.0;
        dv = 1.0;
        SD->pd->paused = false;
        SD->pd->resume = -1.0;
      } else {
        if (fabs(dv) > 0.0025) {
          dv = dp / dv;
          if (rtIsNaN(dv)) {
            direction = rtNaN;
          } else if (dv < 0.0) {
            direction = 1.0;
          } else {
            direction = -(double)(dv > 0.0);
          }
          if (b_I >= 0.05) {
            SD->pd->previousV = b_I;
          } else {
            SD->pd->previousV = 0.05;
          }
          d = fabs(dv) / SD->pd->previousV;
          if ((d >= 1.5) || rtIsNaN(d)) {
            dv = 1.5;
          } else {
            dv = d;
          }
        } else {
          direction = SD->pd->resume;
          dv = 0.025;
        }
        if (direction == 0.0) {
          direction = SD->pd->resume;
        }
        d = 0.12 * P_tmp;
        if (d <= 0.5) {
          d = 0.5;
        }
        if ((fabs(dp) > d) && (!(SD->pd->trust >= 0.5))) {
          SD->pd->trust = 0.5;
        }
        if (direction != SD->pd->lastDirection) {
          d = 0.55 * SD->pd->trust;
          if ((d <= 0.03) || rtIsNaN(d)) {
            SD->pd->trust = 0.03;
          } else {
            SD->pd->trust = d;
          }
        } else {
          d = 1.12 * SD->pd->trust;
          if ((d >= 1.0) || rtIsNaN(d)) {
            SD->pd->trust = 1.0;
          } else {
            SD->pd->trust = d;
          }
        }
      }
      d = V / b_I / 6.6605839416058386;
      if ((d <= 0.25) || rtIsNaN(d)) {
        d = 0.25;
      }
      if (d >= 4.0) {
        d = 4.0;
      }
      if (dv <= 0.015) {
        SD->pd->previousV = 0.015;
      } else {
        SD->pd->previousV = dv;
      }
      d = 0.032 * sqrt(d) * SD->pd->previousV * SD->pd->trust;
      if ((d <= 0.00015) || rtIsNaN(d)) {
        d = 0.00015;
      }
      if ((!newLocal) && (dv < 0.5)) {
        if (SD->pd->paused) {
          SD->pd->paused = false;
        } else if (dp * SD->pd->previousDP <= 0.0) {
          SD->pd->resume = direction;
          direction = 0.0;
          SD->pd->paused = true;
        }
      } else {
        SD->pd->paused = false;
      }
      if (direction != 0.0) {
        SD->pd->resume = direction;
        SD->pd->lastDirection = direction;
      }
      SD->pd->b_started = true;
      if (d >= 0.04) {
        d = 0.04;
      }
      d = SD->pd->b_d + direction * d;
      if ((d <= 0.0) || rtIsNaN(d)) {
        d = 0.0;
      }
      if (d >= 0.69921875) {
        SD->pd->b_d = 0.69921875;
      } else {
        SD->pd->b_d = d;
      }
    }
    SD->pd->previousDP = P_tmp - SD->pd->previousP;
    SD->pd->previousP = P_tmp;
    SD->pd->previousV = V;
    *D = SD->pd->b_d;
    *step = SD->pd->b_d - old;
    if (SD->pd->b_started && (direction == 0.0)) {
      newLocal = true;
    } else {
      newLocal = false;
    }
    *holdFlag = newLocal;
    for (b_direction = 0; b_direction < 6; b_direction++) {
      diag[b_direction] = b_diag[b_direction];
    }
  } else {
    *D = mppt_combinacion_mejorada_50w(SD, V, b_I, t, step, holdFlag, diag);
  }
}

/*
 * File trailer for mppt_reference.c
 *
 * [EOF]
 */
