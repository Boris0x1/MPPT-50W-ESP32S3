#include "adaptive_meng_mppt.h"

void adaptive_meng_mppt_init(adaptive_meng_mppt_t *s) { mppt_init(s,MPPT_ADAPTIVE_MENG); }
void adaptive_meng_mppt_reset(adaptive_meng_mppt_t *s) { mppt_reset(s); }
double adaptive_meng_mppt_update(adaptive_meng_mppt_t *s,double V,double I) { return mppt_update(s,V,I); }
