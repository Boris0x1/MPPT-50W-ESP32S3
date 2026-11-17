// Direct C adaptation of localArticle in inc_mppt.m and meng_vi_mppt.m.
// Frozen configurations from PARAMETROS.json; V/I and local time only.
#include "additional_mppt.h"
#include <math.h>

double local_article_reference(mppt_referenceStackData *sd,double V,double I,double t,int literal,
                             double *step,double *hold,double diag[6])
{
    mppt_referencePersistentData *m=sd->pd;
    if(!m->d_not_empty) {
        m->d=.4;m->v1=V;m->p1=V*I;m->previousDP=m->p1;m->started=false;m->d_not_empty=true;
    }
    const double P=V*I,dp=P-m->p1,dp_previous=m->p1-m->previousDP,dv=V-m->v1;
    const double old=m->d;
    double direction=0,h=literal ? .002:.004;
    if(t>=.02 && isfinite(P) && V>=.1 && I>=.001) {
        if(!m->started) {direction=-1;m->started=true;}
        else if(!literal) {
            if(fabs(dv)>1e-4) {
                const double slope=I+V*(I-m->p1/fmax(m->v1,.1))/dv;
                if(slope>.002)direction=-1;else if(slope<-.002)direction=1;
            } else {
                const double di=I-m->p1/fmax(m->v1,.1);
                if(di>1e-5)direction=-1;else if(di<-1e-5)direction=1;
            }
        } else if(dp!=0) {
            direction=((dp>0)==(dv>0)) ? -1:1;
            if(dp_previous<=0)direction=0;
        }
        if(literal)h=fmin(.05,fmax(.002,V/fmax(I,.001)*.002));
        m->d=fmin(.69921875,fmax(0,m->d+direction*h));
    }
    m->previousDP=m->p1;m->p1=P;m->v1=V;
    *step=m->d-old;*hold=m->started && direction==0;
    diag[0]=4;for(int i=1;i<6;++i)diag[i]=0;
    return m->d;
}
