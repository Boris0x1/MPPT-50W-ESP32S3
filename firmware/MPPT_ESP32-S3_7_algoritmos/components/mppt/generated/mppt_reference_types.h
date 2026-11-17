/*
 * Academic License - for use in teaching, academic research, and meeting
 * course requirements at degree granting institutions only.  Not for
 * government, commercial, or other organizational use.
 * File: mppt_reference_types.h
 *
 * MATLAB Coder version            : 24.2
 * C/C++ source code generated on  : 13-Nov-2026 13:38:48
 */

#ifndef MPPT_REFERENCE_TYPES_H
#define MPPT_REFERENCE_TYPES_H

/* Include Files */
#include "rtwtypes.h"

/* Type Definitions */
#ifndef c_typedef_mppt_referencePersist
#define c_typedef_mppt_referencePersist
typedef struct {
  double d;
  boolean_T d_not_empty;
  double v1;
  double p1;
  boolean_T started;
  double b_d;
  boolean_T b_d_not_empty;
  double previousV;
  double previousP;
  double previousDP;
  boolean_T paused;
  double resume;
  boolean_T b_started;
  double trust;
  boolean_T trust_not_empty;
  double lastDirection;
  double priorLocalDP;
  boolean_T pausedLast;
  double resumeDirection;
  double failedScoutCount;
  double phase;
  boolean_T phase_not_empty;
  double entered;
  double c_d;
  double vRef;
  double beforeEventV;
  boolean_T pendingAudit;
  double lastAudit;
  double lastScout;
  double kind;
  boolean_T shortNext;
  boolean_T lowerVoltageNext;
  double b_previousV;
  double b_previousP;
  double vFilt;
  double pFast;
  double iFast;
  double pSlow;
  double iSlow;
  double eventCount;
  boolean_T localStarted;
  double ringV[32];
  double ringP[32];
  double ringD[32];
  double ringNext;
  double ringCount;
  double incumbentV;
  double incumbentP;
  double incumbentD;
  double firstEdge;
  double secondEdge;
  double leg;
  double edgeCount;
  double bestScore;
  double bestV;
  double bestD;
  double measuredD;
  double targetD;
  boolean_T testPeak;
  boolean_T approach;
  double confirmCount;
  double confirmPower;
  double b_trust;
  boolean_T b_trust_not_empty;
  double b_lastDirection;
} mppt_referencePersistentData;
#endif /* c_typedef_mppt_referencePersist */

#ifndef typedef_mppt_referenceStackData
#define typedef_mppt_referenceStackData
typedef struct {
  mppt_referencePersistentData *pd;
} mppt_referenceStackData;
#endif /* typedef_mppt_referenceStackData */

#endif
/*
 * File trailer for mppt_reference_types.h
 *
 * [EOF]
 */
