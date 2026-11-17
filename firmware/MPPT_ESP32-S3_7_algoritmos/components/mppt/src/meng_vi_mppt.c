#include "meng_vi_mppt.h"

void meng_vi_mppt_init(meng_vi_mppt_t *s) { mppt_init(s,MPPT_MENG_VI); }
void meng_vi_mppt_reset(meng_vi_mppt_t *s) { mppt_reset(s); }
double meng_vi_mppt_update(meng_vi_mppt_t *s,double V,double I) { return mppt_update(s,V,I); }
