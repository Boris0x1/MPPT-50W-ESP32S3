#include "adaptive_meng_with_probing_mppt.h"

void adaptive_meng_with_probing_mppt_init(adaptive_meng_with_probing_mppt_t *s) { mppt_init(s,MPPT_ADAPTIVE_MENG_WITH_PROBING); }
void adaptive_meng_with_probing_mppt_reset(adaptive_meng_with_probing_mppt_t *s) { mppt_reset(s); }
double adaptive_meng_with_probing_mppt_update(adaptive_meng_with_probing_mppt_t *s,double V,double I) { return mppt_update(s,V,I); }
