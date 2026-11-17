function model=preparar_simulacion(control,perfil,repo)
% Prepare a working copy. This does not start a simulation or edit published SLX.
% control: one of the seven function names in INDICE_ALGORITMOS.csv.
if nargin<1,control='adaptive_meng_with_probing_mppt';end
if nargin<2,perfil='X1_escalones_y_carga';end
here=fileparts(mfilename('fullpath'));repro=fileparts(here);
if nargin<3,repo=fileparts(repro);end
addpath(fullfile(repro,'apoyo'),fullfile(repro,'apoyo','fuentes'));
meta=jsondecode(fileread(fullfile(repro,'configuracion','PARAMETROS_CONTROLADORES.json')));
idx=find(strcmp({meta.function},control),1);assert(~isempty(idx),'Unknown controller. Use the seven published MATLAB function names.');
m=meta(idx);cfg=reshape(m.config,1,[]);
model='MPPT_50W_Unificado';
if cfg(11)==7,model='MPPT_50W_Extendido';end
if cfg(11)==8,model='MPPT_50W_Combinacion_Mejorada';end
assert(~bdIsLoaded(model),'Close the model before preparing a separate working copy.');
src=fullfile(repo,'simulacion','modelos',[model '.slx']);
kernel=fullfile(repo,'simulacion','nucleos_simulink',[m.original_function '.m']);
assert(isfile(src)&&isfile(kernel),'Extract the simulation package into the repository first.');
assert(strcmp(filehash(kernel),m.source_sha256),'The published kernel differs from the frozen reference.');
work=fullfile(repro,'resultados','modelos_trabajo');if ~isfolder(work),mkdir(work);end
dest=fullfile(work,[model '.slx']);copyfile(src,dest,'f');
load_system(dest);branch=[model '/Hibrido_VI_Supervisor'];
mw=get_param(model,'ModelWorkspace');panel=getVariable(mw,'Panel50W');
assert(abs(panel.Pm-50.005)<1e-8 && panel.Ncell==33);
assert(abs(panel.Vm-18.25)<1e-8 && abs(panel.Im-2.74)<1e-8);
assert(abs(getVariable(mw,'TsMPPT')-.002)<1e-12);
rt=sfroot;chart=rt.find('-isa','Stateflow.EMChart','Path',[branch '/MPPT']);
assert(numel(chart)==1);chart.Script=fileread(kernel);
preparar_configuracion_variable(model,branch,mw);fijar_configuracion(mw,cfg);
if strcmp(perfil,'R9')
    scene=jsondecode(fileread(fullfile(repro,'referencias','perfil_R9.json')));
    assert(strcmp(filehash(fullfile(repro,'referencias','cierre_final.json')),scene.cierre_sha256));
    p.t=(0:.001:scene.duracion_s)';p.G=interp1(scene.G(:,1),scene.G(:,2:4),p.t,'previous');
    p.R=interp1(scene.R(:,1),scene.R(:,2),p.t,'previous');
    p.duration=scene.duracion_s;p.events=scene.eventos_s;
    [patterns,~,ix]=unique(p.G,'rows');peaks=zeros(size(patterns,1),1);
    for j=1:numel(peaks),peaks(j)=referencia_psc(patterns(j,:),panel);end
    p.reference=peaks(ix);
else
    p=cargar_perfil(perfil,panel);
end
asignar_perfil(mw,p);
assignin(mw,'TsPower',2e-6);assignin(mw,'AverageN',500);
set_param([branch '/PV_y_Boost/Ventana_raw'],'Time',num2str(p.duration-.2));
set_param(model,'StopTime',num2str(p.duration),'SimulationMode','normal');
save_system(model,dest);
if usejava('desktop'),open_system(model);end
fprintf('Prepared %s / %s; original published files unchanged.\n',m.name,perfil);
fprintf('Working copy: %s\nPress Run only if you want a new switched-plant simulation.\n',dest);
end

function h=filehash(p)
f=fopen(p,'rb');assert(f>0);c=onCleanup(@()fclose(f));b=fread(f,Inf,'*uint8');
md=java.security.MessageDigest.getInstance('SHA-256');md.update(b);
h=lower(reshape(dec2hex(typecast(md.digest(),'uint8'),2).',1,[]));
end
