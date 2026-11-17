#include "adaptive_meng_without_probing_mppt.h"
// Adaptation: cfg(10)=0 disables ONLY periodic scouts. Global audits/recovery retained.
// The three original generated kernels remain unchanged.
/*
 * Academic License - for use in teaching, academic research, and meeting
 * course requirements at degree granting institutions only.  Not for
 * government, commercial, or other organizational use.
 * File: mppt_combinacion_mejorada_50w.c
 *
 * MATLAB Coder version            : 24.2
 * C/C++ source code generated on  : 13-Nov-2026 13:38:48
 */

/* Include Files */
#include "additional_mppt.h"
#include "mod.h"
#include "mppt_reference_types.h"
#include "mppt_unificado_50w.h"
#include "rt_nonfinite.h"
#include "rt_nonfinite.h"
#include <math.h>
#include <string.h>

/* Function Declarations */
static double fastRule(mppt_referenceStackData *SD, double V, double b_I,
                       double oldV, double oldP, double priorDP,
                       boolean_T *paused, double *resume, boolean_T newLocal,
                       const double cfg[15], double *h);

/* Function Definitions */
/*
 * V/I normalization, measured normalized slope and a bounded trust factor.
 *  All decisions use current/past V and I plus fixed nameplate constants.
 *
 * Arguments    : mppt_referenceStackData *SD
 *                double V
 *                double b_I
 *                double oldV
 *                double oldP
 *                double priorDP
 *                boolean_T *paused
 *                double *resume
 *                boolean_T newLocal
 *                const double cfg[15]
 *                double *h
 * Return Type  : double
 */
static double fastRule(mppt_referenceStackData *SD, double V, double b_I,
                       double oldV, double oldP, double priorDP,
                       boolean_T *paused, double *resume, boolean_T newLocal,
                       const double cfg[15], double *h)
{
  double P;
  double d;
  double direction;
  double dp;
  double dv;
  double relativeSlope;
  if ((!SD->pd->b_trust_not_empty) || newLocal) {
    SD->pd->b_trust = 1.0;
    SD->pd->b_trust_not_empty = true;
    SD->pd->b_lastDirection = -1.0;
  }
  P = V * b_I;
  dp = P - oldP;
  dv = V - oldV;
  if (newLocal) {
    direction = -1.0;
    relativeSlope = 1.0;
    *paused = false;
    *resume = -1.0;
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
        d = b_I;
      } else {
        d = 0.05;
      }
      dv = fabs(dv) / d;
      if ((dv >= 1.5) || rtIsNaN(dv)) {
        relativeSlope = 1.5;
      } else {
        relativeSlope = dv;
      }
    } else {
      direction = *resume;
      relativeSlope = 0.025;
    }
    if (direction == 0.0) {
      direction = *resume;
    }
    dv = 0.12 * P;
    if ((dv <= 0.5) || rtIsNaN(dv)) {
      dv = 0.5;
    }
    if ((fabs(dp) > dv) && (!(SD->pd->b_trust >= 0.5))) {
      SD->pd->b_trust = 0.5;
    }
    if (direction != SD->pd->b_lastDirection) {
      dv = 0.55 * SD->pd->b_trust;
      if ((dv <= 0.03) || rtIsNaN(dv)) {
        SD->pd->b_trust = 0.03;
      } else {
        SD->pd->b_trust = dv;
      }
    } else {
      dv = 1.12 * SD->pd->b_trust;
      if ((dv >= 1.0) || rtIsNaN(dv)) {
        SD->pd->b_trust = 1.0;
      } else {
        SD->pd->b_trust = dv;
      }
    }
  }
  if (b_I >= 0.001) {
    d = b_I;
  } else {
    d = 0.001;
  }
  dv = V / d / cfg[14];
  if ((dv <= 0.25) || rtIsNaN(dv)) {
    dv = 0.25;
  }
  if (dv >= 4.0) {
    dv = 4.0;
  }
  if (relativeSlope <= 0.015) {
    d = 0.015;
  } else {
    d = relativeSlope;
  }
  dv = cfg[2] * sqrt(dv) * d * SD->pd->b_trust;
  if ((dv <= 0.00015) || rtIsNaN(dv)) {
    dv = 0.00015;
  }
  if (dv >= 0.04) {
    *h = 0.04;
  } else {
    *h = dv;
  }
  if (cfg[12] <= 0.0) {
    *h = 0.004;
  }
  if ((!newLocal) && (cfg[11] > 0.0) && (relativeSlope < 0.5)) {
    if (*paused) {
      *paused = false;
    } else if (dp * priorDP <= 0.0) {
      *resume = direction;
      direction = 0.0;
      *paused = true;
    }
  } else {
    *paused = false;
  }
  if (direction != 0.0) {
    *resume = direction;
    SD->pd->b_lastDirection = direction;
  }
  return direction;
}

/*
 * Arguments    : mppt_referenceStackData *SD
 * Return Type  : void
 */
double adaptive_meng_without_probing_reference(mppt_referenceStackData *SD, double V,
                                     double b_I, double t, double *step,
                                     double *holdFlag, double diag[6])
{
  static const double dv[15] = {0.0, 0.0, 0.024, 0.0,  0.4,
                                0.0, 0.7, 0.02,  10.0, 0.0,
                                8.0, 1.0, 1.0,   0.04, 6.6605839416058386};
  double D;
  double P;
  double localStep;
  double oldD;
  double oldVFilt;
  int trigger;
  boolean_T clampedReady;
  boolean_T confirmed;
  boolean_T eligibleRegion;
  boolean_T fullDue;
  boolean_T guard1;
  boolean_T guard2;
  boolean_T guard3;
  boolean_T guard4;
  boolean_T lowRegion;
  /*  cfg(11): 0 PO, 1 INC, 2 Meng literal, 3 original scout, 4 integrated. */
  /*  cfg(12): two-step gate; cfg(13): adaptive V/I step. */
  /*  cfg(14): integration step cap (literal Meng always uses 0.05). */
  /*  mode 5: pilot with a 0.00035 floor; mode 6 retains the paper floor 0.002.
   */
  /*  Both are normalized by nameplate resistance cfg(15); pauses last one tick.
   */
  if (!SD->pd->phase_not_empty) {
    SD->pd->phase_not_empty = true;
    SD->pd->entered = t;
    SD->pd->c_d = 0.4;
    SD->pd->b_previousV = V;
    SD->pd->b_previousP = V * b_I;
    SD->pd->vFilt = V;
    SD->pd->pFast = SD->pd->b_previousP;
    SD->pd->iFast = b_I;
    SD->pd->pSlow = SD->pd->pFast;
    SD->pd->iSlow = b_I;
    SD->pd->incumbentD = 0.4;
    SD->pd->bestD = 0.4;
    SD->pd->measuredD = 0.4;
    SD->pd->targetD = 0.4;
  }
  P = V * b_I;
  D = SD->pd->c_d;
  *step = 0.0;
  *holdFlag = (SD->pd->phase == 3.0);
  if ((SD->pd->vRef > 0.0) && (SD->pd->vRef <= 8.5)) {
    lowRegion = true;
  } else {
    lowRegion = false;
  }
  if ((SD->pd->vRef > 0.0) &&
      ((SD->pd->vRef < 15.0) || (SD->pd->c_d >= 0.58))) {
    eligibleRegion = true;
  } else {
    eligibleRegion = false;
  }
  if (SD->pd->phase <= 4.0) {
    diag[0] = SD->pd->phase;
  } else {
    diag[0] = 4.0;
  }
  diag[1] = (SD->pd->phase == 1.0);
  diag[2] = SD->pd->vRef;
  diag[3] = 0.0;
  diag[4] = SD->pd->kind;
  diag[5] = eligibleRegion;
  if (SD->pd->phase == 5.0) {
    diag[0] = 2.0;
  }
  if ((!rtIsInf(P)) && (!rtIsNaN(P)) && (!(V < 0.1)) && (!(b_I < 0.001))) {
    oldD = SD->pd->c_d;
    oldVFilt = SD->pd->vFilt;
    SD->pd->vFilt += 0.3 * (V - SD->pd->vFilt);
    SD->pd->pFast += 0.2 * (P - SD->pd->pFast);
    SD->pd->iFast += 0.2 * (b_I - SD->pd->iFast);
    trigger = 0;
    if ((SD->pd->phase == 0.0) && (t >= 0.02)) {
      SD->pd->phase = 4.0;
      SD->pd->entered = t;
    }
    if (SD->pd->phase == 4.0) {
      /*  Local hill climb during acquisition and after confirmed disturbance.
       */
      /*  A 64 ms window screens drift before committing to a voltage reference.
       */
      SD->pd->ringV[(int)SD->pd->ringNext - 1] = V;
      SD->pd->ringP[(int)SD->pd->ringNext - 1] = P;
      SD->pd->ringD[(int)SD->pd->ringNext - 1] = SD->pd->c_d;
      SD->pd->ringNext = b_mod(SD->pd->ringNext) + 1.0;
      if ((SD->pd->ringCount + 1.0 >= 32.0) ||
          rtIsNaN(SD->pd->ringCount + 1.0)) {
        SD->pd->ringCount = 32.0;
      } else {
        SD->pd->ringCount++;
      }
      confirmed = !SD->pd->localStarted;
      if (confirmed) {
        SD->pd->localStarted = true;
        SD->pd->priorLocalDP = 0.0;
        SD->pd->pausedLast = false;
        SD->pd->resumeDirection = -1.0;
      }
      oldVFilt = fastRule(SD, V, b_I, SD->pd->b_previousV, SD->pd->b_previousP,
                          SD->pd->priorLocalDP, &SD->pd->pausedLast,
                          &SD->pd->resumeDirection, confirmed, dv, &localStep);
      SD->pd->priorLocalDP = P - SD->pd->b_previousP;
      D = SD->pd->c_d + oldVFilt * localStep;
      if ((D <= 0.0) || rtIsNaN(D)) {
        D = 0.0;
      }
      if (D >= 0.69921875) {
        SD->pd->c_d = 0.69921875;
      } else {
        SD->pd->c_d = D;
      }
      if ((SD->pd->ringCount == 32.0) && (t - SD->pd->entered >= 0.08)) {
        confirmed = quietWindow(SD->pd->ringV, SD->pd->ringP, SD->pd->ringD,
                                SD->pd->ringNext, &oldVFilt, &localStep,
                                &SD->pd->b_previousP);
        if (confirmed) {
          if (SD->pd->beforeEventV > 0.0) {
            D = 0.18 * SD->pd->beforeEventV;
            if (D <= 1.0) {
              D = 1.0;
            }
            if (fabs(oldVFilt - SD->pd->beforeEventV) > D) {
              SD->pd->pendingAudit = true;
            } else {
              SD->pd->pendingAudit = false;
            }
          } else {
            SD->pd->pendingAudit = false;
          }
          SD->pd->beforeEventV = 0.0;
          SD->pd->vRef = oldVFilt;
          SD->pd->c_d = SD->pd->b_previousP;
          SD->pd->pSlow = localStep;
          SD->pd->iSlow = localStep / oldVFilt;
          SD->pd->pFast = P;
          SD->pd->iFast = b_I;
          SD->pd->eventCount = 0.0;
          SD->pd->phase = 3.0;
          SD->pd->entered = t;
          SD->pd->kind = 0.0;
        }
      }
    } else {
      guard1 = false;
      guard2 = false;
      guard3 = false;
      if (SD->pd->phase == 3.0) {
        /*  Voltage servo and a fast/slow detector. Gradual drift updates its
         * anchors. */
        /*  A queued event is relevant only near the quiet point that armed it.
         */
        SD->pd->b_previousP = t - SD->pd->entered;
        SD->pd->pendingAudit =
            ((!(SD->pd->b_previousP > 0.25)) && SD->pd->pendingAudit);
        SD->pd->b_previousV = SD->pd->vFilt - SD->pd->vRef;
        D = 0.0025 * SD->pd->vRef;
        SD->pd->measuredD = fabs(SD->pd->b_previousV);
        if ((D <= 0.035) || rtIsNaN(D)) {
          D = 0.035;
        }
        if (SD->pd->measuredD > D) {
          D = 0.003 * SD->pd->b_previousV;
          if ((D <= -0.006) || rtIsNaN(D)) {
            D = -0.006;
          }
          if (D >= 0.006) {
            D = 0.006;
          }
          D += SD->pd->c_d;
          if ((D <= 0.0) || rtIsNaN(D)) {
            D = 0.0;
          }
          if (D >= 0.69921875) {
            SD->pd->c_d = 0.69921875;
          } else {
            SD->pd->c_d = D;
          }
        }
        D = 0.105 * SD->pd->pSlow;
        if ((D <= 0.5) || rtIsNaN(D)) {
          D = 0.5;
        }
        localStep = fabs(SD->pd->pFast - SD->pd->pSlow) / D;
        D = 0.13 * SD->pd->iSlow;
        if ((D <= 0.045) || rtIsNaN(D)) {
          D = 0.045;
        }
        D = fabs(SD->pd->iFast - SD->pd->iSlow) / D;
        if ((localStep <= D) || rtIsNaN(D)) {
          D = localStep;
        }
        localStep = SD->pd->measuredD / 0.8;
        if ((localStep >= D) || rtIsNaN(D)) {
          oldVFilt = localStep;
        } else {
          oldVFilt = D;
        }
        if ((oldVFilt >= 1.0) && (SD->pd->b_previousP >= 0.05)) {
          SD->pd->eventCount++;
        } else if (oldVFilt < 0.65) {
          SD->pd->eventCount = 0.0;
        } else if ((SD->pd->eventCount - 1.0 <= 0.0) ||
                   rtIsNaN(SD->pd->eventCount - 1.0)) {
          SD->pd->eventCount = 0.0;
        } else {
          SD->pd->eventCount--;
        }
        if (SD->pd->eventCount < 2.0) {
          SD->pd->pSlow += 0.035 * (P - SD->pd->pSlow);
          SD->pd->iSlow += 0.035 * (b_I - SD->pd->iSlow);
        }
        if (SD->pd->eventCount >= 4.0) {
          SD->pd->failedScoutCount = 0.0;
          SD->pd->beforeEventV = SD->pd->vRef;
          SD->pd->eventCount = 0.0;
          SD->pd->entered = t;
          SD->pd->ringCount = 0.0;
          SD->pd->ringNext = 1.0;
          SD->pd->localStarted = false;
          /*  Opposing V/I movement is consistent with restoring this reference.
           */
          /*  This test does not identify the physical cause of the disturbance.
           */
          if (SD->pd->measuredD > 0.65) {
            localStep = SD->pd->iFast - SD->pd->iSlow;
            if ((fabs(localStep) > 0.02) &&
                (SD->pd->b_previousV * localStep < 0.0)) {
              SD->pd->phase = 5.0;
            } else {
              SD->pd->phase = 4.0;
            }
          } else {
            SD->pd->phase = 4.0;
          }
        } else if ((SD->pd->b_previousP >= 0.05) && (oldVFilt < 0.65)) {
          D = 0.02 * SD->pd->vRef;
          if ((D <= 0.18) || rtIsNaN(D)) {
            D = 0.18;
          }
          confirmed = (SD->pd->measuredD < D);
          /*  The voltage target may be unreachable at the upper PWM limit. */
          /*  Stable measured P/I permit probing toward lower duty from that
           * limit. */
          if ((oldD >= 0.69721875) && (SD->pd->b_previousV > 0.0)) {
            D = 0.03 * SD->pd->iSlow;
            if ((D <= 0.015) || rtIsNaN(D)) {
              D = 0.015;
            }
            if (fabs(SD->pd->iFast - SD->pd->iSlow) < D) {
              clampedReady = true;
            } else {
              clampedReady = false;
            }
          } else {
            clampedReady = false;
          }
          oldVFilt = floor(SD->pd->failedScoutCount / 2.0);
          if (confirmed) {
            localStep = t - SD->pd->lastAudit;
            if ((localStep >= 6.0) ||
                (SD->pd->pendingAudit && (localStep >= 1.2))) {
              fullDue = true;
            } else {
              fullDue = false;
            }
          } else {
            fullDue = false;
          }
          if (false && eligibleRegion && (!SD->pd->pendingAudit)) {
            if ((oldVFilt + 1.0 >= 3.0) || rtIsNaN(oldVFilt + 1.0)) {
              SD->pd->b_previousV = 3.0;
            } else {
              SD->pd->b_previousV = oldVFilt + 1.0;
            }
            if ((t - SD->pd->lastScout >= SD->pd->b_previousV) &&
                (confirmed || clampedReady)) {
              D = 0.03 * SD->pd->pSlow;
              if ((D <= 0.12) || rtIsNaN(D)) {
                D = 0.12;
              }
              if ((fabs(SD->pd->pFast - SD->pd->pSlow) < D) &&
                  ((!lowRegion) || (oldD >= 0.06))) {
                confirmed = true;
              } else {
                confirmed = false;
              }
            } else {
              confirmed = false;
            }
          } else {
            confirmed = false;
          }
          if (fullDue || confirmed) {
            SD->pd->phase = 1.0;
            SD->pd->entered = t;
            SD->pd->lastScout = t;
            trigger = 1;
            SD->pd->pendingAudit = false;
            SD->pd->incumbentV = SD->pd->vRef;
            SD->pd->incumbentP = SD->pd->pSlow;
            SD->pd->incumbentD = oldD;
            SD->pd->leg = 1.0;
            SD->pd->edgeCount = 0.0;
            SD->pd->bestScore = 0.0;
            SD->pd->bestV = 0.0;
            SD->pd->bestD = SD->pd->c_d;
            if (fullDue) {
              SD->pd->kind = 1.0;
              SD->pd->lastAudit = t;
              if (SD->pd->c_d <= 0.69921875 - SD->pd->c_d) {
                SD->pd->firstEdge = 0.0;
                SD->pd->secondEdge = 0.69921875;
              } else {
                SD->pd->firstEdge = 0.69921875;
                SD->pd->secondEdge = 0.0;
              }
            } else if (lowRegion) {
              SD->pd->kind = 3.0;
              if (SD->pd->shortNext) {
                SD->pd->kind = 2.0;
              }
              SD->pd->shortNext = !SD->pd->shortNext;
              SD->pd->firstEdge = 0.0;
            } else {
              SD->pd->kind = 3.0;
              if ((SD->pd->lowerVoltageNext &&
                   (0.69921875 - SD->pd->c_d >= 0.06)) ||
                  (SD->pd->c_d < 0.06)) {
                SD->pd->firstEdge = 0.69921875;
              } else {
                SD->pd->firstEdge = 0.0;
              }
              SD->pd->lowerVoltageNext = !SD->pd->lowerVoltageNext;
            }
          }
        }
      } else if (SD->pd->phase == 5.0) {
        /*  Brief, bounded recovery around the last reference with voltage lead.
         */
        SD->pd->b_previousV = SD->pd->vFilt - SD->pd->vRef;
        localStep = SD->pd->b_previousV + 3.0 * (SD->pd->vFilt - oldVFilt);
        D = 0.0025 * SD->pd->vRef;
        if ((D <= 0.035) || rtIsNaN(D)) {
          D = 0.035;
        }
        if (fabs(localStep) > D) {
          D = 0.003 * localStep;
          if ((D <= -0.006) || rtIsNaN(D)) {
            D = -0.006;
          }
          if (D >= 0.006) {
            D = 0.006;
          }
          D += SD->pd->c_d;
          if ((D <= 0.0) || rtIsNaN(D)) {
            D = 0.0;
          }
          if (D >= 0.69921875) {
            SD->pd->c_d = 0.69921875;
          } else {
            SD->pd->c_d = D;
          }
        }
        localStep = t - SD->pd->entered;
        if (((localStep >= 0.018) && (fabs(SD->pd->b_previousV) < 0.3)) ||
            (localStep >= 0.082)) {
          SD->pd->phase = 4.0;
          SD->pd->entered = t;
        }
      } else if (SD->pd->phase == 1.0) {
        /*  A full audit covers both ends. A scout visits just one voltage side.
         */
        D = 0.12 * SD->pd->incumbentV;
        localStep = 0.065 * V;
        if ((P <= SD->pd->b_previousP) || rtIsNaN(SD->pd->b_previousP)) {
          SD->pd->b_previousP = P;
        }
        if (t - SD->pd->entered >= 0.01) {
          if ((D <= 0.45) || rtIsNaN(D)) {
            D = 0.45;
          }
          if (fabs(V - SD->pd->incumbentV) >= D) {
            if ((localStep <= 0.3) || rtIsNaN(localStep)) {
              localStep = 0.3;
            }
            if ((fabs(V - SD->pd->b_previousV) <= localStep) &&
                (SD->pd->b_previousP > SD->pd->bestScore)) {
              SD->pd->bestScore = SD->pd->b_previousP;
              SD->pd->bestV = 0.5 * (V + SD->pd->b_previousV);
              SD->pd->bestD = 0.5 * (SD->pd->c_d + SD->pd->measuredD);
            }
          }
        }
        oldVFilt = SD->pd->firstEdge;
        if (SD->pd->leg == 2.0) {
          oldVFilt = SD->pd->secondEdge;
        }
        D = oldVFilt - SD->pd->c_d;
        if ((D <= -0.03) || rtIsNaN(D)) {
          D = -0.03;
        }
        if (D >= 0.03) {
          D = 0.03;
        }
        SD->pd->c_d += D;
        if (fabs(SD->pd->c_d - oldVFilt) < 1.0E-9) {
          SD->pd->edgeCount++;
        } else {
          SD->pd->edgeCount = 0.0;
        }
        /*  A shallow scout samples beyond the local lobe, then returns if no */
        /*  advantage is visible. Its paired deep scout covers the rest of that
         * side. */
        localStep = V - SD->pd->incumbentV;
        if ((localStep >= 0.5) &&
            (SD->pd->b_previousP < 0.8 * SD->pd->incumbentP)) {
          confirmed = true;
        } else {
          confirmed = false;
        }
        D = 0.35 * SD->pd->incumbentV;
        guard4 = false;
        if (SD->pd->kind == 2.0) {
          if (confirmed) {
            guard4 = true;
          } else {
            if ((D <= 2.4) || rtIsNaN(D)) {
              D = 2.4;
            }
            if (localStep >= D) {
              guard4 = true;
            } else {
              confirmed = false;
            }
          }
        } else {
          confirmed = false;
        }
        if (guard4) {
          D = 0.012 * SD->pd->incumbentP;
          if ((D <= 0.08) || rtIsNaN(D)) {
            D = 0.08;
          }
          if (SD->pd->bestScore <= SD->pd->incumbentP + D) {
            confirmed = true;
          } else {
            confirmed = false;
          }
        }
        if ((SD->pd->edgeCount >= 7.0) || confirmed) {
          if ((SD->pd->kind == 1.0) && (SD->pd->leg == 1.0)) {
            SD->pd->leg = 2.0;
            SD->pd->edgeCount = 0.0;
          } else {
            SD->pd->phase = 2.0;
            SD->pd->entered = t;
            D = 0.012 * SD->pd->incumbentP;
            if ((D <= 0.08) || rtIsNaN(D)) {
              D = 0.08;
            }
            SD->pd->testPeak = (SD->pd->bestScore > SD->pd->incumbentP + D);
            if (SD->pd->testPeak) {
              SD->pd->vRef = SD->pd->bestV;
              SD->pd->targetD = SD->pd->bestD;
            } else {
              SD->pd->vRef = SD->pd->incumbentV;
              SD->pd->targetD = SD->pd->incumbentD;
            }
            SD->pd->approach = true;
            SD->pd->confirmCount = 0.0;
            SD->pd->confirmPower = 0.0;
          }
        }
      } else if (SD->pd->phase == 2.0) {
        /*  Revisit the measured candidate. A settled confirmation prevents */
        /*  accepting a transient as an improved equilibrium point. */
        if (SD->pd->approach) {
          oldVFilt = 0.03;
          /*  Only the already measured incumbent is revisited faster. */
          /*  cfg(1)=1 disables this own extension for ablation/regression. */
          if ((!SD->pd->testPeak) && (SD->pd->targetD > SD->pd->c_d)) {
            oldVFilt = 0.1;
          }
          D = SD->pd->targetD - SD->pd->c_d;
          if ((-oldVFilt >= D) || rtIsNaN(D)) {
            D = -oldVFilt;
          }
          if (oldVFilt <= D) {
            D = oldVFilt;
          }
          SD->pd->c_d += D;
          SD->pd->approach = (fabs(SD->pd->c_d - SD->pd->targetD) > 0.001);
        } else {
          SD->pd->b_previousV = SD->pd->vFilt - SD->pd->vRef;
          oldVFilt = SD->pd->vFilt - oldVFilt;
          localStep = SD->pd->b_previousV + 3.0 * oldVFilt;
          D = 0.0025 * SD->pd->vRef;
          if ((D <= 0.035) || rtIsNaN(D)) {
            D = 0.035;
          }
          if (fabs(localStep) > D) {
            D = 0.004 * localStep;
            if ((D <= -0.01) || rtIsNaN(D)) {
              D = -0.01;
            }
            if (D >= 0.01) {
              D = 0.01;
            }
            D += SD->pd->c_d;
            if ((D <= 0.0) || rtIsNaN(D)) {
              D = 0.0;
            }
            if (D >= 0.69921875) {
              SD->pd->c_d = 0.69921875;
            } else {
              SD->pd->c_d = D;
            }
          }
          D = 0.018 * SD->pd->vRef;
          if ((D <= 0.18) || rtIsNaN(D)) {
            D = 0.18;
          }
          if ((fabs(SD->pd->b_previousV) < D) && (fabs(oldVFilt) < 0.07)) {
            SD->pd->confirmCount++;
            SD->pd->confirmPower += P;
          } else {
            SD->pd->confirmCount = 0.0;
            SD->pd->confirmPower = 0.0;
          }
        }
        confirmed = (SD->pd->confirmCount >= 8.0);
        if (confirmed || (t - SD->pd->entered >= 0.26)) {
          if (SD->pd->kind >= 2.0) {
            guard4 = false;
            if (SD->pd->testPeak && confirmed) {
              D = 0.012 * SD->pd->incumbentP;
              if ((SD->pd->confirmCount <= 1.0) ||
                  rtIsNaN(SD->pd->confirmCount)) {
                SD->pd->b_previousV = 1.0;
              } else {
                SD->pd->b_previousV = SD->pd->confirmCount;
              }
              if ((D <= 0.08) || rtIsNaN(D)) {
                D = 0.08;
              }
              if (SD->pd->confirmPower / SD->pd->b_previousV >
                  SD->pd->incumbentP + D) {
                SD->pd->failedScoutCount = 0.0;
              } else {
                guard4 = true;
              }
            } else {
              guard4 = true;
            }
            if (guard4 && ((!SD->pd->testPeak) && confirmed)) {
              if ((SD->pd->failedScoutCount + 1.0 >= 6.0) ||
                  rtIsNaN(SD->pd->failedScoutCount + 1.0)) {
                SD->pd->failedScoutCount = 6.0;
              } else {
                SD->pd->failedScoutCount++;
              }
            }
          }
          if (SD->pd->testPeak) {
            if (!confirmed) {
              guard2 = true;
            } else {
              D = 0.012 * SD->pd->incumbentP;
              if ((SD->pd->confirmCount <= 1.0) ||
                  rtIsNaN(SD->pd->confirmCount)) {
                SD->pd->b_previousV = 1.0;
              } else {
                SD->pd->b_previousV = SD->pd->confirmCount;
              }
              if ((D <= 0.08) || rtIsNaN(D)) {
                D = 0.08;
              }
              if (SD->pd->confirmPower / SD->pd->b_previousV <=
                  SD->pd->incumbentP + D) {
                guard2 = true;
              } else {
                guard3 = true;
              }
            }
          } else {
            guard3 = true;
          }
        }
      }
      if (guard3) {
        if (confirmed && (!SD->pd->testPeak)) {
          D = 0.012 * SD->pd->incumbentP;
          localStep = SD->pd->confirmPower / SD->pd->confirmCount;
          if ((D <= 0.08) || rtIsNaN(D)) {
            D = 0.08;
          }
          if (fabs(localStep - SD->pd->incumbentP) <= D) {
            SD->pd->phase = 3.0;
            SD->pd->entered = t;
            SD->pd->kind = 0.0;
            SD->pd->pSlow = localStep;
            SD->pd->iSlow = b_I;
            SD->pd->pFast = P;
            SD->pd->iFast = b_I;
            SD->pd->eventCount = 0.0;
          } else {
            guard1 = true;
          }
        } else {
          guard1 = true;
        }
      }
      if (guard2) {
        SD->pd->vRef = SD->pd->incumbentV;
        SD->pd->targetD = SD->pd->incumbentD;
        SD->pd->testPeak = false;
        SD->pd->approach = true;
        SD->pd->entered = t;
        SD->pd->confirmCount = 0.0;
        SD->pd->confirmPower = 0.0;
      }
      if (guard1) {
        SD->pd->phase = 4.0;
        SD->pd->entered = t;
        SD->pd->ringCount = 0.0;
        SD->pd->ringNext = 1.0;
        SD->pd->localStarted = false;
        SD->pd->beforeEventV = 0.0;
      }
    }
    SD->pd->measuredD = oldD;
    SD->pd->b_previousV = V;
    SD->pd->b_previousP = P;
    if ((SD->pd->c_d <= 0.0) || rtIsNaN(SD->pd->c_d)) {
      D = 0.0;
    } else {
      D = SD->pd->c_d;
    }
    if (D >= 0.69921875) {
      D = 0.69921875;
    }
    SD->pd->c_d = D;
    *step = D - oldD;
    *holdFlag = (SD->pd->phase == 3.0);
    localStep = SD->pd->phase;
    if (SD->pd->phase == 5.0) {
      localStep = 2.0;
    }
    diag[0] = localStep;
    diag[1] = (SD->pd->phase == 1.0);
    diag[2] = SD->pd->vRef;
    diag[3] = trigger;
    diag[4] = SD->pd->kind;
    diag[5] = eligibleRegion;
  }
  return D;
}

/*
 * File trailer for mppt_combinacion_mejorada_50w.c
 *
 * [EOF]
 */

void adaptive_meng_without_probing_mppt_init(adaptive_meng_without_probing_mppt_t *s) { mppt_init(s,MPPT_ADAPTIVE_MENG_WITHOUT_PERIODIC_PROBING); }
void adaptive_meng_without_probing_mppt_reset(adaptive_meng_without_probing_mppt_t *s) { mppt_reset(s); }
double adaptive_meng_without_probing_mppt_update(adaptive_meng_without_probing_mppt_t *s,double V,double I) { return mppt_update(s,V,I); }
