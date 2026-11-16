#ifndef PO_AJUSTADO_H
#define PO_AJUSTADO_H
#include "mppt.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef mppt_state_t po_ajustado_t;
void po_ajustado_init(po_ajustado_t *s);
void po_ajustado_reset(po_ajustado_t *s);
double po_ajustado_update(po_ajustado_t *s, double Vpv_V, double Ipv_A);
#ifdef __cplusplus
}
#endif
#endif
