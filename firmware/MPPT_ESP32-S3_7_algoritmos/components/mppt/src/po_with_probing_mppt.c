#include "po_with_probing_mppt.h"
// Direct C adaptation of po_with_probing_mppt.m. Persistent fields are kept
// in the existing per-controller memory; only the fixed cfg(11)=6 path is used.
#include "additional_mppt.h"
#include "mppt_unificado_50w.h"
#include <math.h>
#include <string.h>
static double clamp(double x,double lo,double hi){return fmin(hi,fmax(lo,x));}
static double voltage_step(double d,double error,double reference,double gain,double slew)
{
    return fabs(error)>fmax(.035,.0025*reference) ? clamp(d+clamp(gain*error,-slew,slew),0,.69921875):d;
}
static double margin(double P){return fmax(.08,.012*P);}

double po_with_probing_reference(mppt_referenceStackData *sd,double V,double I,double t,
                       double *step,double *hold,double diag[6])
{
    mppt_referencePersistentData *m=sd->pd;
    if(!m->phase_not_empty) {
        m->priorLocalDP=0;m->pausedLast=false;m->resumeDirection=-1;
        m->phase=0;m->entered=t;m->c_d=.4;m->vRef=0;m->beforeEventV=0;
        m->pendingAudit=false;m->lastAudit=0;m->lastScout=0;m->kind=0;
        m->shortNext=true;m->lowerVoltageNext=true;
        m->b_previousV=V;m->b_previousP=V*I;m->vFilt=V;
        m->pFast=V*I;m->iFast=I;m->pSlow=m->pFast;m->iSlow=I;m->eventCount=0;
        m->localStarted=false;
        memset(m->ringV,0,sizeof(m->ringV));memset(m->ringP,0,sizeof(m->ringP));memset(m->ringD,0,sizeof(m->ringD));
        m->ringNext=1;m->ringCount=0;m->incumbentV=0;m->incumbentP=0;m->incumbentD=m->c_d;
        m->firstEdge=0;m->secondEdge=0;m->leg=1;m->edgeCount=0;
        m->bestScore=0;m->bestV=0;m->bestD=m->c_d;m->measuredD=m->c_d;m->targetD=m->c_d;
        m->testPeak=false;m->approach=false;m->confirmCount=0;m->confirmPower=0;m->phase_not_empty=true;
    }
    const double P=V*I;
    *step=0;*hold=m->phase==3;
    const bool low_region=m->vRef>0 && m->vRef<=8.5;
    const bool eligible=m->vRef>0 && (m->vRef<15 || m->c_d>=.58);
    diag[0]=m->phase==5 ? 2:fmin(m->phase,4);diag[1]=m->phase==1;diag[2]=m->vRef;
    diag[3]=0;diag[4]=m->kind;diag[5]=eligible;
    if(!isfinite(P) || V<.1 || I<.001)return m->c_d;
    const double old_d=m->c_d,old_v=m->vFilt;
    m->vFilt=m->vFilt+.30*(V-m->vFilt);
    m->pFast=m->pFast+.20*(P-m->pFast);m->iFast=m->iFast+.20*(I-m->iFast);
    int trigger=0;
    if(m->phase==0 && t>=.02){m->phase=4;m->entered=t;}
    if(m->phase==4) {
        const unsigned index=(unsigned)m->ringNext-1;
        m->ringV[index]=V;m->ringP[index]=P;m->ringD[index]=m->c_d;
        m->ringNext=fmod(m->ringNext,32)+1;m->ringCount=fmin(32,m->ringCount+1);
        const double delta=P-m->b_previousP;
        double direction=0;
        if(!m->localStarted) {
            direction=-1;m->localStarted=true;m->priorLocalDP=0;m->pausedLast=false;m->resumeDirection=-1;
        } else if(delta!=0)direction=((P>m->b_previousP)==(V>m->b_previousV)) ? -1:1;
        // cfg(12)=cfg(13)=0: no two-step pause and no V/I adaptive gain here.
        if(direction!=0)m->resumeDirection=direction;
        m->priorLocalDP=delta;m->c_d=clamp(m->c_d+direction*.002,0,.69921875);
        if(m->ringCount==32 && t-m->entered>=.12) {
            double center_v,center_p,center_d;
            if(quietWindow(m->ringV,m->ringP,m->ringD,m->ringNext,&center_v,&center_p,&center_d)) {
                m->pendingAudit=m->beforeEventV>0 && fabs(center_v-m->beforeEventV)>fmax(1,.18*m->beforeEventV);
                m->beforeEventV=0;m->vRef=center_v;m->c_d=center_d;m->pSlow=center_p;m->iSlow=center_p/center_v;
                m->pFast=P;m->iFast=I;m->eventCount=0;m->phase=3;m->entered=t;m->kind=0;
            }
        }
    } else if(m->phase==3) {
        if(t-m->entered>.250)m->pendingAudit=false;
        const double e=m->vFilt-m->vRef;
        m->c_d=voltage_step(m->c_d,e,m->vRef,.0016,.0035);
        const double relative_p=fabs(m->pFast-m->pSlow)/fmax(.5,.105*m->pSlow);
        const double relative_i=fabs(m->iFast-m->iSlow)/fmax(.045,.13*m->iSlow);
        const double deviation=fmax(fabs(e)/.80,fmin(relative_p,relative_i));
        if(deviation>=1 && t-m->entered>=.050)m->eventCount=m->eventCount+1;
        else if(deviation<.65)m->eventCount=0;
        else m->eventCount=fmax(0,m->eventCount-1);
        if(m->eventCount<2){m->pSlow=m->pSlow+.035*(P-m->pSlow);m->iSlow=m->iSlow+.035*(I-m->iSlow);}
        if(m->eventCount>=4) {
            m->beforeEventV=m->vRef;m->eventCount=0;m->entered=t;
            m->ringCount=0;m->ringNext=1;m->localStarted=false;
            m->phase=(fabs(e)>.65 && fabs(m->iFast-m->iSlow)>.020 && e*(m->iFast-m->iSlow)<0) ? 5:4;
        } else if(t-m->entered>=.050 && deviation<.65) {
            const bool voltage_ready=fabs(e)<fmax(.18,.02*m->vRef);
            const bool clamped_ready=old_d>=.69921875-.002 && e>0 && fabs(m->iFast-m->iSlow)<fmax(.015,.03*m->iSlow);
            const bool full_due=voltage_ready && (t-m->lastAudit>=4 || (m->pendingAudit && t-m->lastAudit>=1.20));
            const bool scout_due=eligible && !m->pendingAudit && t-m->lastScout>=1 &&
                (voltage_ready || clamped_ready) && fabs(m->pFast-m->pSlow)<fmax(.12,.03*m->pSlow) &&
                (!low_region || old_d>=.06);
            if(full_due || scout_due) {
                m->phase=1;m->entered=t;m->lastScout=t;trigger=1;m->pendingAudit=false;
                m->incumbentV=m->vRef;m->incumbentP=m->pSlow;m->incumbentD=old_d;
                m->leg=1;m->edgeCount=0;m->bestScore=0;m->bestV=0;m->bestD=m->c_d;
                if(full_due) {
                    m->kind=1;m->lastAudit=t;
                    if(m->c_d<=.69921875-m->c_d){m->firstEdge=0;m->secondEdge=.69921875;}
                    else {m->firstEdge=.69921875;m->secondEdge=0;}
                } else if(low_region) {
                    m->kind=m->shortNext ? 2:3;m->shortNext=!m->shortNext;m->firstEdge=0;
                } else {
                    m->kind=3;
                    m->firstEdge=((m->lowerVoltageNext && .69921875-m->c_d>=.06) || m->c_d<.06) ? .69921875:0;
                    m->lowerVoltageNext=!m->lowerVoltageNext;
                }
            }
        }
    } else if(m->phase==5) {
        const double e=m->vFilt-m->vRef,lead=e+3*(m->vFilt-old_v);
        m->c_d=voltage_step(m->c_d,lead,m->vRef,.0030,.0060);
        if((t-m->entered>=.018 && fabs(e)<.30) || t-m->entered>=.082){m->phase=4;m->entered=t;}
    } else if(m->phase==1) {
        const bool separated=fabs(V-m->incumbentV)>=fmax(.45,.12*m->incumbentV);
        const bool nearby=fabs(V-m->b_previousV)<=fmax(.30,.065*V);
        const double score=fmin(P,m->b_previousP);
        if(t-m->entered>=.010 && separated && nearby && score>m->bestScore) {
            m->bestScore=score;m->bestV=.5*(V+m->b_previousV);m->bestD=.5*(m->c_d+m->measuredD);
        }
        const double edge=m->leg==2 ? m->secondEdge:m->firstEdge;
        m->c_d=m->c_d+clamp(edge-m->c_d,-.018,.018);
        m->edgeCount=fabs(m->c_d-edge)<1e-9 ? m->edgeCount+1:0;
        const bool shallow_decline=V-m->incumbentV>=.50 && score<.80*m->incumbentP;
        const bool shallow_extent=V-m->incumbentV>=fmax(2.4,.35*m->incumbentV);
        const bool shallow_done=m->kind==2 && (shallow_decline || shallow_extent) && m->bestScore<=m->incumbentP+margin(m->incumbentP);
        if(m->edgeCount>=7 || shallow_done) {
            if(m->kind==1 && m->leg==1){m->leg=2;m->edgeCount=0;}
            else {
                m->phase=2;m->entered=t;m->testPeak=m->bestScore>m->incumbentP+margin(m->incumbentP);
                m->vRef=m->testPeak ? m->bestV:m->incumbentV;m->targetD=m->testPeak ? m->bestD:m->incumbentD;
                m->approach=true;m->confirmCount=0;m->confirmPower=0;
            }
        }
    } else if(m->phase==2) {
        if(m->approach) {
            m->c_d=m->c_d+clamp(m->targetD-m->c_d,-.018,.018);m->approach=fabs(m->c_d-m->targetD)>.001;
        } else {
            const double e=m->vFilt-m->vRef,lead=e+3*(m->vFilt-old_v);
            m->c_d=voltage_step(m->c_d,lead,m->vRef,.0025,.0060);
            if(fabs(e)<fmax(.18,.018*m->vRef) && fabs(m->vFilt-old_v)<.07){m->confirmCount=m->confirmCount+1;m->confirmPower=m->confirmPower+P;}
            else {m->confirmCount=0;m->confirmPower=0;}
        }
        const bool confirmed=m->confirmCount>=8,expired=t-m->entered>=.260;
        if(confirmed || expired) {
            if(m->testPeak && (!confirmed || m->confirmPower/fmax(1,m->confirmCount)<=m->incumbentP+margin(m->incumbentP))) {
                m->vRef=m->incumbentV;m->targetD=m->incumbentD;m->testPeak=false;m->approach=true;m->entered=t;
                m->confirmCount=0;m->confirmPower=0;
            } else if(confirmed && !m->testPeak && fabs(m->confirmPower/m->confirmCount-m->incumbentP)<=margin(m->incumbentP)) {
                m->phase=3;m->entered=t;m->kind=0;m->pSlow=m->confirmPower/m->confirmCount;m->iSlow=I;
                m->pFast=P;m->iFast=I;m->eventCount=0;
            } else {
                m->phase=4;m->entered=t;m->ringCount=0;m->ringNext=1;m->localStarted=false;m->beforeEventV=0;
            }
        }
    }
    m->measuredD=old_d;m->b_previousV=V;m->b_previousP=P;
    m->c_d=clamp(m->c_d,0,.69921875);*step=m->c_d-old_d;*hold=m->phase==3;
    diag[0]=m->phase==5 ? 2:m->phase;diag[1]=m->phase==1;diag[2]=m->vRef;
    diag[3]=trigger;diag[4]=m->kind;diag[5]=eligible;
    return m->c_d;
}

void po_with_probing_mppt_init(po_with_probing_mppt_t *s) { mppt_init(s,MPPT_PO_WITH_PROBING); }
void po_with_probing_mppt_reset(po_with_probing_mppt_t *s) { mppt_reset(s); }
double po_with_probing_mppt_update(po_with_probing_mppt_t *s,double V,double I) { return mppt_update(s,V,I); }
