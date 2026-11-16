#include "po_ajustado.h"
void po_ajustado_init(po_ajustado_t *s) { mppt_init(s,MPPT_PO); }
void po_ajustado_reset(po_ajustado_t *s) { mppt_reset(s); }
double po_ajustado_update(po_ajustado_t *s,double V,double I) { return mppt_update(s,V,I); }
