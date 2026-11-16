#include "model_output.h"
#include <math.h>
void model_output_init(model_output_t *s){s->initialized=false;s->filtered_V=0;}
double model_output_update(model_output_t *s,double V,double d){
    if(!s->initialized){s->filtered_V=V;s->initialized=true;}
    double a=exp(-.002/.002);
    s->filtered_V=a*s->filtered_V+(1-a)*V;
    double trim=.0040000000000000001*(V-s->filtered_V);
    if(trim<-.04)trim=-.04;
    if(trim>.04)trim=.04;
    double out=d+trim;if(out<0)out=0;if(out>.70)out=.70;
    return out;
}
uint16_t model_duty_count(double d){
    if(!isfinite(d)||d<=0)return 0;
    double n=floor(1024*d+.5);if(n>716)n=716;
    return (uint16_t)n;
}
