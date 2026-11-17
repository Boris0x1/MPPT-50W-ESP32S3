function [D,step,holdFlag,diag]=adaptive_meng_mppt(V,I,t)
% Adaptive Meng. Fixed configuration of the archived Simulink reference.
% Inputs: panel voltage V [V], current I [A], local time t [s].
% Call every 0.002 s, starting at t=0. Reset with clear FUNCTION_NAME.
% Duty D is normalized. No irradiance/temperature/load inputs.
cfg=[0 0 0.032000000000000001 0 0.40000000000000002 0 0.69999999999999996 0.02 10 1 7 1 1 0.040000000000000001 18.25/2.74];
algorithm=0;
%#codegen
% MPPT at 2 ms. V and I are the only measurements. t is a local clock.
% Diagnostic phases: 0 wait, 1 scan, 2 recover, 3 hold, 4 refine.
% cfg(9)=0 is reserved for an ablation without global search.
% cfg(10) is the scout interval in seconds. Zero disables only scouts.
% diag(5) gives the exploration type: 1 full, 2 short scout, 3 deep scout.
persistent priorLocalDP pausedLast resumeDirection failedScoutCount
persistent phase entered d vRef beforeEventV pendingAudit lastAudit
persistent lastScout kind shortNext lowerVoltageNext
persistent previousV previousP vFilt pFast iFast pSlow iSlow eventCount
persistent localStarted direction ringV ringP ringD ringNext ringCount
persistent incumbentV incumbentP incumbentD firstEdge secondEdge leg edgeCount
persistent bestScore bestV bestD measuredD targetD testPeak approach confirmCount confirmPower

% cfg(11): 0 PO, 1 INC, 2 Meng literal, 3 original scout, 4 integrated.
% cfg(12): two-step gate; cfg(13): adaptive V/I step.
% cfg(14): integration step cap (literal Meng always uses 0.05).
% mode 5: pilot with a 0.00035 floor; mode 6 retains the paper floor 0.002.
% Both are normalized by nameplate resistance cfg(15); pauses last one tick.
if cfg(11)==7
    [D,step,holdFlag,diag]=fastLocal(V,I,t,cfg);
    return
end
if cfg(11)<3
    [D,step,holdFlag,diag]=localArticle(V,I,t,cfg);
    return
end
lower=cfg(6);
upper=floor(1024*cfg(7))/1024;
if isempty(phase)
    priorLocalDP=0;pausedLast=false;resumeDirection=-1;failedScoutCount=0;phase=0;entered=t;d=cfg(5);vRef=0;beforeEventV=0;
    pendingAudit=false;lastAudit=0;
    lastScout=0;kind=0;shortNext=true;lowerVoltageNext=true;
    previousV=V;previousP=V*I;vFilt=V;
    pFast=V*I;iFast=I;pSlow=pFast;iSlow=I;eventCount=0;
    localStarted=false;direction=-1;
    ringV=zeros(32,1);ringP=zeros(32,1);ringD=zeros(32,1);
    ringNext=1;ringCount=0;
    incumbentV=0;incumbentP=0;incumbentD=d;
    firstEdge=0;secondEdge=0;leg=1;edgeCount=0;
    bestScore=0;bestV=0;bestD=d;measuredD=d;targetD=d;
    testPeak=false;approach=false;confirmCount=0;confirmPower=0;
end
P=V*I;
D=d;step=0;holdFlag=double(phase==3);
lowRegion=vRef>0 && vRef<=8.5;
eligibleRegion=vRef>0 && (vRef<15 || d>=.58);
diag=[min(phase,4) double(phase==1) vRef 0 kind double(eligibleRegion)];
if phase==5,diag(1)=2;end
if algorithm<0 || ~isfinite(P) || V<.10 || I<.001,return;end

oldD=d;
oldVFilt=vFilt;
vFilt=vFilt+.30*(V-vFilt);
pFast=pFast+.20*(P-pFast);
iFast=iFast+.20*(I-iFast);
trigger=0;
if phase==0 && t>=cfg(8)
    phase=4;entered=t;
end

if phase==4
    % Local hill climb during acquisition and after confirmed disturbance.
    % A 64 ms window screens drift before committing to a voltage reference.
    ringV(ringNext)=V;ringP(ringNext)=P;ringD(ringNext)=d;
    ringNext=mod(ringNext,32)+1;
    ringCount=min(32,ringCount+1);
    deltaP=P-previousP;
    newLocal=~localStarted;
    if cfg(11)>=8
        if newLocal
            localStarted=true;priorLocalDP=0;pausedLast=false;resumeDirection=-1;
        end
        [direction,localStep,pausedLast,resumeDirection]=fastRule( ...
            V,I,previousV,previousP,priorLocalDP,pausedLast,resumeDirection,newLocal,cfg);
    else
    deltaP=P-previousP;
    newLocal=~localStarted;
    if ~localStarted
        direction=-1;
        localStarted=true;
        priorLocalDP=0;pausedLast=false;resumeDirection=-1;
    elseif deltaP~=0
        if (P>previousP)==(V>previousV),direction=-1;else,direction=1;end
        % Literal Algorithm 2: all four branches require prior deltaP>0.
        if cfg(11)==4 && cfg(12)>0 && priorLocalDP<=0,direction=0;end
    else
        direction=0;
    end
    % Integration guard: the literal two-step pause lasts at most one tick.
    % A fresh physical perturbation is then required; a flat quantized sample
    % alone is not evidence that a maximum has been found.
    if cfg(11)>=5 && ~newLocal && cfg(12)>0
        if pausedLast
            if direction==0,direction=resumeDirection;end
            pausedLast=false;
        elseif priorLocalDP<=0
            if direction~=0,resumeDirection=direction;end
            direction=0;pausedLast=true;
        end
    end
    if direction~=0,resumeDirection=direction;end
    localStep=cfg(3);
    if cfg(11)==4 && cfg(13)>0
        localStep=clamp((V/max(I,.001))*cfg(3),.002,cfg(14));
    elseif cfg(11)>=5 && cfg(13)>0
        minimum=.00035;if cfg(11)==6,minimum=.002;end
        localStep=clamp((V/max(I,.001))/cfg(15)*cfg(3),minimum,cfg(14));
    end
    end
    priorLocalDP=deltaP;
    d=clamp(d+direction*localStep,lower,upper);
    minimumRefine=.12;if cfg(11)>=8,minimumRefine=.08;end
    if ringCount==32 && t-entered>=minimumRefine
        [quiet,centerV,centerP,centerD]=quietWindow(ringV,ringP,ringD,ringNext);
        if quiet
            pendingAudit=beforeEventV>0 && ...
                abs(centerV-beforeEventV)>max(1.0,.18*beforeEventV);
            beforeEventV=0;
            vRef=centerV;d=centerD;pSlow=centerP;iSlow=centerP/centerV;
            pFast=P;iFast=I;eventCount=0;
            phase=3;entered=t;kind=0;
        end
    end
elseif phase==3
    % Voltage servo and a fast/slow detector. Gradual drift updates its anchors.
    % A queued event is relevant only near the quiet point that armed it.
    if t-entered>.250,pendingAudit=false;end
    e=vFilt-vRef;
    if cfg(11)>=8
        d=voltageStep(d,e,vRef,.0030,.0060,lower,upper);
    else
        d=voltageStep(d,e,vRef,.0016,.0035,lower,upper);
    end
    relativeP=abs(pFast-pSlow)/max(.5,.105*pSlow);
    relativeI=abs(iFast-iSlow)/max(.045,.13*iSlow);
    deviation=max(abs(e)/.80,min(relativeP,relativeI));
    if deviation>=1 && t-entered>=.050
        eventCount=eventCount+1;
    elseif deviation<.65
        eventCount=0;
    else
        eventCount=max(0,eventCount-1);
    end
    if eventCount<2
        pSlow=pSlow+.035*(P-pSlow);
        iSlow=iSlow+.035*(I-iSlow);
    end
    if eventCount>=4
        if cfg(11)>=8,failedScoutCount=0;end
        beforeEventV=vRef;
        eventCount=0;entered=t;
        ringCount=0;ringNext=1;localStarted=false;
        % Opposing V/I movement is consistent with restoring this reference.
        % This test does not identify the physical cause of the disturbance.
        if abs(e)>.65 && abs(iFast-iSlow)>.020 && e*(iFast-iSlow)<0
            phase=5;
        else
            phase=4;
        end
    elseif cfg(9)>0 && t-entered>=.050 && deviation<.65
        voltageReady=abs(e)<max(.18,.02*vRef);
        % The voltage target may be unreachable at the upper PWM limit.
        % Stable measured P/I permit probing toward lower duty from that limit.
        clampedReady=oldD>=upper-.002 && e>0 && ...
            abs(iFast-iSlow)<max(.015,.03*iSlow);
        auditInterval=4.0;effectiveScout=cfg(10);
        if cfg(11)>=8
            auditInterval=6.0;
            effectiveScout=cfg(10)*min(3,1+floor(failedScoutCount/2));
        end
        fullDue=voltageReady && ...
            (t-lastAudit>=auditInterval || (pendingAudit && t-lastAudit>=1.20));
        scoutDue=cfg(10)>0 && eligibleRegion && ~pendingAudit && t-lastScout>=effectiveScout && ...
            (voltageReady || clampedReady) && abs(pFast-pSlow)<max(.12,.03*pSlow) && ...
            (~lowRegion || oldD-lower>=.06);
        if fullDue || scoutDue
            phase=1;entered=t;lastScout=t;trigger=1;
            pendingAudit=false;
            incumbentV=vRef;incumbentP=pSlow;incumbentD=oldD;
            leg=1;edgeCount=0;bestScore=0;bestV=0;bestD=d;
            if fullDue
                kind=1;lastAudit=t;
                if d-lower<=upper-d
                    firstEdge=lower;secondEdge=upper;
                else
                    firstEdge=upper;secondEdge=lower;
                end
            elseif lowRegion
                kind=3;
                if shortNext,kind=2;end
                shortNext=~shortNext;
                firstEdge=lower;
            else
                kind=3;
                if (lowerVoltageNext && upper-d>=.06) || d-lower<.06
                    firstEdge=upper;
                else
                    firstEdge=lower;
                end
                lowerVoltageNext=~lowerVoltageNext;
            end
        end
    end
elseif phase==5
    % Brief, bounded recovery around the last reference with voltage lead.
    e=vFilt-vRef;
    lead=e+3*(vFilt-oldVFilt);
    d=voltageStep(d,lead,vRef,.0030,.0060,lower,upper);
    if (t-entered>=.018 && abs(e)<.30) || t-entered>=.082
        phase=4;entered=t;
    end
elseif phase==1
    % A full audit covers both ends. A scout visits just one voltage side.
    separated=abs(V-incumbentV)>=max(.45,.12*incumbentV);
    nearby=abs(V-previousV)<=max(.30,.065*V);
    score=min(P,previousP);
    if t-entered>=.010 && separated && nearby && score>bestScore
        bestScore=score;bestV=.5*(V+previousV);bestD=.5*(d+measuredD);
    end
    edge=firstEdge;
    if leg==2,edge=secondEdge;end
    scanSlew=.018;if cfg(11)>=8,scanSlew=.030;end
    d=d+clamp(edge-d,-scanSlew,scanSlew);
    if abs(d-edge)<1e-9,edgeCount=edgeCount+1;else,edgeCount=0;end
    % A shallow scout samples beyond the local lobe, then returns if no
    % advantage is visible. Its paired deep scout covers the rest of that side.
    shallowDecline=V-incumbentV>=.50 && score<.80*incumbentP;
    shallowExtent=V-incumbentV>=max(2.4,.35*incumbentV);
    shallowDone=kind==2 && (shallowDecline || shallowExtent) && ...
        bestScore<=incumbentP+margin(incumbentP);
    if edgeCount>=7 || shallowDone
        if kind==1 && leg==1
            leg=2;edgeCount=0;
        else
            phase=2;entered=t;
            testPeak=bestScore>incumbentP+margin(incumbentP);
            if testPeak
                vRef=bestV;targetD=bestD;
            else
                vRef=incumbentV;targetD=incumbentD;
            end
            approach=true;confirmCount=0;confirmPower=0;
        end
    end
elseif phase==2
    % Revisit the measured candidate. A settled confirmation prevents
    % accepting a transient as an improved equilibrium point.
    if approach
        returnSlew=.018;if cfg(11)>=8,returnSlew=.030;end
        d=d+clamp(targetD-d,-returnSlew,returnSlew);
        approach=abs(d-targetD)>.001;
    else
        e=vFilt-vRef;
        lead=e+3*(vFilt-oldVFilt);
        if cfg(11)>=8
            d=voltageStep(d,lead,vRef,.0040,.0100,lower,upper);
        else
            d=voltageStep(d,lead,vRef,.0025,.0060,lower,upper);
        end
        if abs(e)<max(.18,.018*vRef) && abs(vFilt-oldVFilt)<.07
            confirmCount=confirmCount+1;confirmPower=confirmPower+P;
        else
            confirmCount=0;confirmPower=0;
        end
    end
    confirmed=confirmCount>=8;
    expired=t-entered>=.260;
    if confirmed || expired
        if cfg(11)>=8 && kind>=2
            if testPeak && confirmed && confirmPower/max(1,confirmCount)>incumbentP+margin(incumbentP)
                failedScoutCount=0;
            elseif ~testPeak && confirmed
                failedScoutCount=min(6,failedScoutCount+1);
            end
        end
        if testPeak && (~confirmed || confirmPower/max(1,confirmCount)<=incumbentP+margin(incumbentP))
            vRef=incumbentV;targetD=incumbentD;
            testPeak=false;approach=true;entered=t;
            confirmCount=0;confirmPower=0;
        elseif confirmed && ~testPeak && abs(confirmPower/confirmCount-incumbentP)<=margin(incumbentP)
            phase=3;entered=t;kind=0;pSlow=confirmPower/confirmCount;iSlow=I;
            pFast=P;iFast=I;eventCount=0;
        else
            phase=4;entered=t;
            ringCount=0;ringNext=1;localStarted=false;beforeEventV=0;
        end
    end
end

measuredD=oldD;previousV=V;previousP=P;
D=clamp(d,lower,upper);d=D;step=D-oldD;
holdFlag=double(phase==3);
reportedPhase=phase;
if phase==5,reportedPhase=2;end
diag=[reportedPhase double(phase==1) vRef trigger kind double(eligibleRegion)];
end

function [quiet,V,P,D]=quietWindow(v,p,d,next)
sumOldV=0;sumOldP=0;sumV=0;sumP=0;sumD=0;
for k=0:15
    earlier=mod(next-1+k,32)+1;
    later=mod(next+15+k,32)+1;
    sumOldV=sumOldV+v(earlier);sumOldP=sumOldP+p(earlier);
    sumV=sumV+v(later);sumP=sumP+p(later);sumD=sumD+d(later);
end
V=sumV/16;P=sumP/16;D=sumD/16;
quiet=abs(P-sumOldP/16)<=max(.09,.011*P) && ...
    abs(V-sumOldV/16)<=max(.07,.011*V) && ...
    max(d)-min(d)<=.016 && max(v)-min(v)<=.80;
end

function next=voltageStep(d,error,reference,gain,slew,lo,hi)
if abs(error)>max(.035,.0025*reference)
    next=clamp(d+clamp(gain*error,-slew,slew),lo,hi);
else
    next=d;
end
end

function required=margin(P)
required=max(.08,.012*P);
end

function bounded=clamp(x,lo,hi)
bounded=min(hi,max(lo,x));
end

function [D,step,holdFlag,diag]=localArticle(V,I,t,cfg)
% Direct transcription of Algorithm 2 (p.5332), with section 3.3 step.
% Startup seed and singularity guard are implementation conventions.
persistent d v1 p1 p2 started
if isempty(d),d=cfg(5);v1=V;p1=V*I;p2=p1;started=false;end
P=V*I; dp=P-p1; dpPrevious=p1-p2; dv=V-v1;
old=d; direction=0; h=cfg(3);
lo=cfg(6);hi=floor(1024*cfg(7))/1024;
if t>=cfg(8) && isfinite(P) && V>=.10 && I>=.001
    if ~started
        direction=-1; started=true;
    elseif cfg(11)==1
        if abs(dv)>1e-4
            slope=I+V*(I-p1/max(v1,.1))/dv;
            if slope>.002,direction=-1;elseif slope<-.002,direction=1;end
        else
            di=I-p1/max(v1,.1);
            if di>1e-5,direction=-1;elseif di<-1e-5,direction=1;end
        end
    elseif dp~=0
        if (dp>0)==(dv>0),direction=-1;else,direction=1;end
        if cfg(11)==2 && cfg(12)>0 && dpPrevious<=0,direction=0;end
    end
    if cfg(11)==2 && cfg(13)>0,h=clamp(V/max(I,.001)*cfg(3),.002,.05);end
    d=clamp(d+direction*h,lo,hi);
end
p2=p1;p1=P;v1=V;
D=d;step=d-old;holdFlag=double(started && direction==0);
diag=[4 0 0 0 0 0];
end


function [D,step,holdFlag,diag]=fastLocal(V,I,t,cfg)
% Own extension of the two-increment/V-I rule; not the Meng replica.
persistent d previousV previousP previousDP paused resume started
if isempty(d)
    d=cfg(5);previousV=V;previousP=V*I;previousDP=0;
    paused=false;resume=-1;started=false;
end
old=d;P=V*I;direction=0;
if t>=cfg(8) && isfinite(P) && V>=.10 && I>=.001
    [direction,h,paused,resume]=fastRule(V,I,previousV,previousP,previousDP,paused,resume,~started,cfg);
    started=true;
    d=clamp(d+direction*h,cfg(6),floor(1024*cfg(7))/1024);
end
previousDP=P-previousP;previousP=P;previousV=V;
D=d;step=d-old;holdFlag=double(started && direction==0);diag=[4 0 0 0 0 0];
end

function [direction,h,paused,resume]=fastRule(V,I,oldV,oldP,priorDP,paused,resume,newLocal,cfg)
% V/I normalization, measured normalized slope and a bounded trust factor.
% All decisions use current/past V and I plus fixed nameplate constants.
persistent trust lastDirection
if isempty(trust) || newLocal,trust=1;lastDirection=-1;end
P=V*I;dp=P-oldP;dv=V-oldV;
if newLocal
    direction=-1;relativeSlope=1;paused=false;resume=-1;
else
    if abs(dv)>.0025
        slope=dp/dv;
        direction=-sign(slope);
        relativeSlope=min(1.5,abs(slope)/max(I,.05));
    else
        direction=resume;relativeSlope=.025;
    end
    if direction==0,direction=resume;end
    if abs(dp)>max(.5,.12*P),trust=max(trust,.5);end
    if direction~=lastDirection
        trust=max(.03,.55*trust);
    else
        trust=min(1,1.12*trust);
    end
end
resistanceScale=sqrt(clamp((V/max(I,.001))/cfg(15),.25,4));
h=clamp(cfg(3)*resistanceScale*max(.015,relativeSlope)*trust,.00015,.040);
if cfg(13)<=0,h=.004;end
if ~newLocal && cfg(12)>0 && relativeSlope<.50
    if paused
        paused=false;
    elseif dp*priorDP<=0
        resume=direction;direction=0;paused=true;
    end
else
    paused=false;
end
if direction~=0,resume=direction;lastDirection=direction;end
end
