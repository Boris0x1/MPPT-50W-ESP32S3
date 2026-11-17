#include "po_mppt.h"

void po_mppt_init(po_mppt_t *s) { mppt_init(s,MPPT_PO); }
void po_mppt_reset(po_mppt_t *s) { mppt_reset(s); }
double po_mppt_update(po_mppt_t *s,double V,double I) { return mppt_update(s,V,I); }
