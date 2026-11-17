function metricas=ejecutar_caso(control,perfil,repo)
% A new closed-loop plant run, only when explicitly invoked by the reviewer.
here=fileparts(mfilename('fullpath'));repro=fileparts(here);
if nargin<1,control='adaptive_meng_with_probing_mppt';end
if nargin<2,perfil='X1_escalones_y_carga';end
if nargin<3,repo=fileparts(repro);end
model=preparar_simulacion(control,perfil,repo);
o=sim(model);q=o.mppt_log;
if isa(q,'timeseries')
    t=q.Time;y=squeeze(q.Data);if size(y,1)~=numel(t),y=y.';end
elseif isstruct(q)
    t=q.time;y=q.signals.values;
else,error('Unsupported mppt_log format. Inspect the returned SimulationOutput.');
end
mw=get_param(model,'ModelWorkspace');reference=getVariable(mw,'Pref_profile');
ref=interp1(reference(:,1),reference(:,2),t,'previous');
E=trapz(t,y(:,1));B=trapz(t,ref);assert(B>0);
metricas=struct('controller',control,'profile',perfil,'energy_J',E, ...
    'reference_J',B,'eta_E_pct',100*E/B,'matlab',version, ...
    'scope','New numerical run; not an experimental measurement.');
out=fullfile(repro,'resultados','simulaciones');if ~isfolder(out),mkdir(out);end
stamp=char(datetime('now','Format','yyyyMMdd_HHmmss_SSS'));
file=fullfile(out,[control '__' perfil '__' stamp '.mat']);
save(file,'t','y','ref','metricas','-v7');
fprintf('New run saved: %s\neta_E = %.9f %%\n',file,metricas.eta_E_pct);
end
