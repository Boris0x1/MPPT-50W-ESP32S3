function preparar_configuracion_variable(model,branch,mw)
% Use fixed-size runtime input to avoid one compilation per ablation.
fijar_configuracion(mw,[0 0 .002 0 .4 0 .7 .02 10 1 6 1 1 .006 18.25/2.74]);
b=[branch '/Configuracion'];
if strcmp(get_param(b,'BlockType'),'Constant')
 position=get_param(b,'Position');ports=get_param(b,'PortHandles');line=get_param(ports.Outport,'Line');
 destinations=get_param(line,'DstPortHandle');destinations=destinations(destinations>0);
 assert(~isempty(destinations),'La configuración debe llegar al controlador.');
 parents=cell(size(destinations));numbers=zeros(size(destinations));
 for k=1:numel(destinations)
  parents{k}=get_param(destinations(k),'Parent');numbers(k)=get_param(destinations(k),'PortNumber');
 end
 delete_line(line);delete_block(b);
 add_block('simulink/Sources/From Workspace',b,'Position',position,'VariableName','cfg_profile', ...
  'SampleTime','TsMPPT','Interpolate','off','OutputAfterFinalValue','Holding final value');
 sourcePorts=get_param(b,'PortHandles');
 for k=1:numel(parents)
  freshPorts=get_param(parents{k},'PortHandles');add_line(branch,sourcePorts.Outport,freshPorts.Inport(numbers(k)),'autorouting','on');
 end
end
set_param(model,'Description','Evaluacion MPPT de50W. P&O, INC, replica deMeng2025 y supervisor integrado. Configuracion constante por ensayo; Vpv/Ipv son las unicas medidas del control.');
end
