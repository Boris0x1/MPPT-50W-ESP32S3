function [Pm,Vm,Im,curve]=referencia_psc(G,p)
% Static series I-V with identical 11-cell substrings and physical bypasses.
% Validated against a separate swept-voltage Simulink test bench.
I=linspace(0,p.IL*max(G)/1000*1.001,30001)';V=zeros(size(I));
a=p.nI*11*1.380649e-23*298.15/1.602176634e-19;
for j=1:3
    iph=p.IL*G(j)/1000;rs=p.Rs/3;rsh=p.Rsh/3;
    u=linspace(-2,p.Voc/3+1,30001)';ii=iph-p.I0*expm1(u/a)-u/rsh;vv=u-ii*rs;
    vs=interp1(flipud(ii),flipud(vv),I,'linear','extrap');
    bypass=-.3-.001*max(I-iph,0);
    V=V+max(vs,bypass);
end
P=V.*I;P(V<0)=-Inf;[Pm,k]=max(P);Vm=V(k);Im=I(k);
ix=V>=0;curve=struct('V',flipud(V(ix)),'I',flipud(I(ix)),'P',flipud(P(ix)));
end
