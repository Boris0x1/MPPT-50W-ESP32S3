#include "inc_mppt.h"

void inc_mppt_init(inc_mppt_t *s) { mppt_init(s,MPPT_INC); }
void inc_mppt_reset(inc_mppt_t *s) { mppt_reset(s); }
double inc_mppt_update(inc_mppt_t *s,double V,double I) { return mppt_update(s,V,I); }
