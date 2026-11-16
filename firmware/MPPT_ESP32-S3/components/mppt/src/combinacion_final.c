#include "combinacion_final.h"
void combinacion_final_init(combinacion_final_t *s) { mppt_init(s,MPPT_COMBINACION_FINAL); }
void combinacion_final_reset(combinacion_final_t *s) { mppt_reset(s); }
double combinacion_final_update(combinacion_final_t *s,double V,double I) { return mppt_update(s,V,I); }
