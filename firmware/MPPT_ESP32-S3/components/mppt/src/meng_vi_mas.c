#include "meng_vi_mas.h"
void meng_vi_mas_init(meng_vi_mas_t *s) { mppt_init(s,MPPT_MENG); }
void meng_vi_mas_reset(meng_vi_mas_t *s) { mppt_reset(s); }
double meng_vi_mas_update(meng_vi_mas_t *s,double V,double I) { return mppt_update(s,V,I); }
