#ifndef ADDITIONAL_MPPT_H
#define ADDITIONAL_MPPT_H
#include <stdbool.h>
#include "mppt_reference_types.h"
// The four additional entries use the same resettable persistent-data layout.
double local_article_reference(mppt_referenceStackData *,double V,double I,double t,int literal,
                             double *step,double *hold,double diag[6]);
double po_with_probing_reference(mppt_referenceStackData *,double V,double I,double t,
                       double *step,double *hold,double diag[6]);
double adaptive_meng_without_probing_reference(mppt_referenceStackData *,double V,double I,double t,
                                double *step,double *hold,double diag[6]);
#endif
