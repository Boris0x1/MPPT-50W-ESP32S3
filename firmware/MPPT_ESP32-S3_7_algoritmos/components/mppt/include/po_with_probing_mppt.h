#ifndef PO_WITH_PROBING_MPPT_H
#define PO_WITH_PROBING_MPPT_H
#include "mppt.h"
#ifdef __cplusplus
extern "C" {
#endif
// Canonical entry name from the seven-algorithm MATLAB delivery.
typedef mppt_state_t po_with_probing_mppt_t;
void po_with_probing_mppt_init(po_with_probing_mppt_t *s);
void po_with_probing_mppt_reset(po_with_probing_mppt_t *s);
double po_with_probing_mppt_update(po_with_probing_mppt_t *s,double Vpv_V,double Ipv_A);
#ifdef __cplusplus
}
#endif
#endif
