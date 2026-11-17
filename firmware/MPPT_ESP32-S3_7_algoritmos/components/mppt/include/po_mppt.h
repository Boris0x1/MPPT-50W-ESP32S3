#ifndef PO_MPPT_H
#define PO_MPPT_H
#include "mppt.h"
#ifdef __cplusplus
extern "C" {
#endif
// Canonical entry name from the seven-algorithm MATLAB delivery.
typedef mppt_state_t po_mppt_t;
void po_mppt_init(po_mppt_t *s);
void po_mppt_reset(po_mppt_t *s);
double po_mppt_update(po_mppt_t *s,double Vpv_V,double Ipv_A);
#ifdef __cplusplus
}
#endif
#endif
