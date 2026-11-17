function verificar_algoritmos(repo)
% Software equivalence only: no new plant simulation or hardware experiment.
here=fileparts(mfilename('fullpath')); root=fileparts(here);
if nargin<1,repo=fileparts(root);end
addpath(fullfile(repo,'simulacion','algoritmos'),fullfile(repo,'simulacion','nucleos_simulink'));
m=jsondecode(fileread(fullfile(root,'configuracion','PARAMETROS_CONTROLADORES.json')));
T=table();found=dir(fullfile(repo,'datos','**','*3_Trazas_X1_X3_5002_Muestras*.csv'));
if numel(found)>1,error('Multiple trace files found; keep one authoritative copy under datos.');end
if numel(found)==1
    T=readtable(fullfile(found.folder,found.name),'VariableNamingRule','preserve','TextType','string');
end
out=fullfile(root,'resultados');if ~isfolder(out),mkdir(out);end
fout=fopen(fullfile(out,'oraculo.bin'),'w','ieee-le');assert(fout>0);
closeOracle=onCleanup(@()fclose(fout));
index=struct([]);cid=0;kinds=[0 3 4 1 5 2 6];
pairs={'V_PO_004_V','I_PO_A';'V_MengVI_Plus_V','I_MengPlus_A'; ...
    'V_Comb_Plus_V','I_Comb_A';'V_Comb_Plus_SinSondeo_V','I_SinSondeo_A'};
cases=struct([]);
if ~isempty(T)
for profile=["X1","X3"]
    q=T(T.Perfil==profile,:);
    for j=1:4
        c=struct('name',char(profile+"_recorded_input_"+j),'kind','recorded_input', ...
            't',q.t_rel_s,'V',q.(pairs{j,1}),'I',q.(pairs{j,2}));
        cases=append(cases,c);
    end
end
end
t=(0:.002:14)';
for j=1:6
    V=12*ones(size(t)); I=2*ones(size(t));
    if j==2
        V(:)=18.25; I(:)=2.74;
    elseif j==3
        V(t>=3&t<4)=10; I(t>=3&t<4)=2.5;
        V(t>=7&t<8)=14; I(t>=7&t<8)=1.5;
    elseif j==4
        V=12+.25*sin(17*t)+.06*sin(79*t);
        I=2+.30*cos(19*t)+.03*cos(113*t);
    elseif j==5
        V=10+mod((0:numel(t)-1)',100)*.05;
        I(:)=.001; I(mod((0:numel(t)-1)',2)==0)=4;
    elseif j==6
        V=12+.00125*mod((0:numel(t)-1)',3);
        I=2+.00025*mod((0:numel(t)-1)',2);
    end
    cases=append(cases,struct('name',sprintf('deterministic_software_test_%d',j), ...
        'kind','software_test','t',t,'V',V,'I',I));
end
results=struct([]);
for a=1:numel(m)
    maxdiff=0; samples=0; phases=zeros(1,6); scans=zeros(1,4);
    lower=0; upper=0; pauses=0;
    for c=1:numel(cases)
        q=cases(c); n=numel(q.t); expected=zeros(n,9); actual=zeros(n,9);
        clear(m(a).original_function,m(a).function);
        for k=1:n
            [D,s,h,g]=feval(m(a).original_function,q.V(k),q.I(k),q.t(k),reshape(m(a).config,1,[]),0);
            expected(k,:)=[D s h reshape(g,1,[])];
        end
        for k=1:n
            [D,s,h,g]=feval(m(a).function,q.V(k),q.I(k),q.t(k));
            actual(k,:)=[D s h reshape(g,1,[])];
        end
        assert(isequaln(expected,actual),'Mismatch: %s, %s',m(a).name,q.name);
        cid=cid+1;
        fwrite(fout,[repmat(cid,n,1) repmat(kinds(a),n,1) q.t q.V q.I expected]','double');
        index=append(index,struct('case',cid,'kind',kinds(a),'name',m(a).name, ...
            'scenario',q.name,'source',q.kind,'samples',n));

        assert(all(isfinite(actual),'all'));
        assert(all(actual(:,1)>=0 & actual(:,1)<=716/1024));
        delta=max(abs(expected-actual),[],'all'); maxdiff=max(maxdiff,delta);
        samples=samples+n; lower=lower+sum(actual(:,1)==0);
        upper=upper+sum(actual(:,1)==716/1024); pauses=pauses+sum(actual(:,3)>0);
        for p=0:5,phases(p+1)=phases(p+1)+sum(actual(:,4)==p);end
        for p=0:3,scans(p+1)=scans(p+1)+sum(actual(:,8)==p);end
    end
    % Invalid-input semantics are compared directly, never reported as safe.
    clear(m(a).original_function,m(a).function);
    vi=[12 2;NaN 2;12 Inf;0 2;12 -1;12 2];
    for k=1:size(vi,1)
        [D,s,h,g]=feval(m(a).original_function,vi(k,1),vi(k,2),(k-1)*.002,reshape(m(a).config,1,[]),0);
        e=[D s h reshape(g,1,[])];
        [D,s,h,g]=feval(m(a).function,vi(k,1),vi(k,2),(k-1)*.002);
        assert(isequaln(e,[D s h reshape(g,1,[])]));
    end
    clear(m(a).original_function,m(a).function);
    first=feval(m(a).function,12,2,0); assert(first==.4);
    clear(m(a).function);
    result=struct('algorithm',m(a).name,'file',m(a).function,'passed',true, ...
        'samples_compared',samples,'cases',numel(cases),'max_abs_difference',maxdiff, ...
        'diagnostic_phase_counts',phases,'scan_type_counts',scans, ...
        'lower_limit_samples',lower,'upper_limit_samples',upper,'hold_samples',pauses, ...
        'invalid_input_matches_original',true,'reset_initial_duty_verified',true);
    results=append(results,result);
    fprintf('PASS %s: %d samples, max difference %.17g\n',m(a).name,samples,maxdiff);
end
assert(isequal(m(6).config([1:9 11:15]),m(7).config([1:9 11:15])));
assert(m(6).config(10)==1 && m(7).config(10)==0);
report=struct('matlab',version,'passed',true,'algorithms',results, ...
    'scope','Exact replay equivalence to archived MATLAB references; not closed-loop validation.', ...
    'new_plant_simulations',0,'hardware_tests_performed',false, ...
    'experimental_trace_rows',height(T),'software_test_signals_are_measurements',false, ...
    'ablation_only_config_index_changed',10);
f=fopen(fullfile(out,'equivalencia_matlab.json'),'w','n','UTF-8');
assert(f>0); cleanup=onCleanup(@()fclose(f));
fprintf(f,'%s\n',jsonencode(report,'PrettyPrint',true));
g=fopen(fullfile(out,'casos.json'),'w','n','UTF-8');assert(g>0);
fprintf(g,'%s\n',jsonencode(struct('matlab',version,'cases',index),'PrettyPrint',true));fclose(g);
fprintf('ALL_SEVEN_MATLAB_REFERENCES_PASS\n');
end

function s=append(s,q)
if isempty(s),s=q;else,s(end+1)=q;end
end
