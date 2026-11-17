function p=cargar_perfil(nombre,panel)
persistent cache cachePanel
if isempty(cachePanel) || ~isequaln(cachePanel,panel),cache=struct;cachePanel=panel;end
if isfield(cache,nombre),p=cache.(nombre);return;end
root=fileparts(mfilename('fullpath'));
q=load(fullfile(root,'fuentes','perfiles_50w.mat'),'profiles');
if isfield(q.profiles,nombre)
 p=q.profiles.(nombre);p.t=(0:.001:p.duration)';p.G=double(p.G);p.R=double(p.R);p.reference=double(p.reference);p.events=double(p.events);cache.(nombre)=p;return
end
if strcmp(nombre,'E1_escalones_articulo')
 duration=1;events=[0 .3 .5 1];gt=[0 500 500 500;.3 1000 1000 1000;.5 800 800 800;1 800 800 800];rt=[0 20;1 20];
elseif strcmp(nombre,'E2_rampa20_articulo') || strcmp(nombre,'E3_rampa50_articulo')
 duration=1;events=[0 .3 1];p.t=(0:.001:1)';rise=20;if startsWith(nombre,'E3'),rise=50;end
 g=1000+rise*max(p.t-.3,0)/.7;p.G=repmat(g,1,3);p.R=20*ones(size(g));
 p.duration=duration;p.events=events;p.reference=referenceFor(p.G,panel);cache.(nombre)=p;return
else
 scenes=jsondecode(fileread(fullfile(root,'fuentes','perfiles_reservados.json')));
 j=find(strcmp({scenes.perfiles.nombre},nombre),1);assert(~isempty(j));
 scene=scenes.perfiles(j);duration=scene.duracion_s;events=scene.eventos_s;gt=scene.G;rt=scene.R;
end
p.t=(0:.001:duration)';p.G=interp1(gt(:,1),gt(:,2:4),p.t,'previous');
p.R=interp1(rt(:,1),rt(:,2),p.t,'previous');p.duration=duration;p.events=events;
p.reference=referenceFor(p.G,panel);cache.(nombre)=p;
end
function powers=referenceFor(G,panel)
[patterns,~,ix]=unique(G,'rows');peaks=zeros(size(patterns,1),1);
for k=1:numel(peaks),peaks(k)=referencia_psc(patterns(k,:),panel);end
powers=peaks(ix);
end
