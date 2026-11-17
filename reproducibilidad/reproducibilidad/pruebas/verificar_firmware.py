"""Compile the received C sources unchanged and replay MATLAB reference inputs."""
from pathlib import Path
import ctypes
import hashlib
import json
import math
import os
import shutil
import struct
import subprocess
import zipfile

REPRO=Path(__file__).resolve().parent.parent
REPO=Path(os.environ.get('MPPT_REPO_ROOT',str(REPRO.parent))).resolve()
HERE=REPRO/'resultados'
HERE.mkdir(exist_ok=True)
PACK=REPO
FW=REPO/'firmware/MPPT_ESP32-S3_7_algoritmos'
KERNEL=FW/'components/mppt'

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    if not (HERE/'oraculo.bin').exists():
        raise SystemExit('Run verificar_algoritmos in MATLAB first to create resultados/oraculo.bin and casos.json.')
    manifest=json.loads((REPRO/'configuracion/FIRMWARE_SHA256.json').read_text())
    for n,h in manifest['files'].items():assert sha(FW.parent/n)==h,n
    cc=shutil.which('gcc');assert cc,'GCC required'
    sources=sorted((KERNEL/'generated').glob('*.c'))
    sources += sorted((KERNEL/'src').glob('*.c'))
    sources += [FW/'main/mppt_control.c',FW/'main/quiet_window.c']
    sources += [REPRO/'pruebas/host_wrapper.c']
    dll=HERE/('review.dll' if os.name=='nt' else 'review.so')
    cmd=[cc,'-shared','-std=c11','-O2','-Wall','-Wextra','-Werror',
         '-fno-fast-math','-ffp-contract=off','-I'+str(KERNEL/'include'),
         '-I'+str(KERNEL/'generated'),'-I'+str(FW/'main')]+[str(p)for p in sources]+[
         '-Wl,--wrap=quietWindow','-lm','-o',str(dll)]
    if os.name!='nt':cmd.insert(2,'-fPIC')
    q=subprocess.run(cmd,capture_output=True,text=True)
    (HERE/'compilacion_host.log').write_text('COMMAND\n'+'\n'.join(cmd)+'\nSTDOUT\n'+q.stdout+'\nSTDERR\n'+q.stderr,encoding='utf-8')
    assert q.returncode==0,q.stderr
    lib=ctypes.CDLL(str(dll));lib.review_reset.argtypes=[ctypes.c_int]
    lib.review_step.argtypes=[ctypes.c_int,ctypes.c_double,ctypes.c_double,ctypes.c_double,ctypes.POINTER(ctypes.c_double)]
    lib.review_status.argtypes=[ctypes.c_int];lib.review_status.restype=ctypes.c_int
    lib.review_internal_phase.argtypes=[ctypes.c_int];lib.review_internal_phase.restype=ctypes.c_int
    lib.review_control_reset.argtypes=[ctypes.c_int]
    lib.review_control_step.argtypes=[ctypes.c_int,ctypes.c_double,ctypes.c_double,ctypes.POINTER(ctypes.c_double)]
    lib.review_control_step.restype=ctypes.c_int
    lib.review_mode_period.argtypes=[ctypes.c_int];lib.review_mode_period.restype=ctypes.c_int
    lib.review_public_init.argtypes=[ctypes.c_int]
    lib.review_public_reset.argtypes=[ctypes.c_int]
    lib.review_public_update.argtypes=[ctypes.c_int,ctypes.c_double,ctypes.c_double,ctypes.POINTER(ctypes.c_double)]
    out=(ctypes.c_double*9)();data=json.loads((HERE/'casos.json').read_text())
    names={q['kind']:q['name']for q in data['cases']}
    results={k:dict(name=names[k],kind=k,serial_mode=k+2,samples=0,mismatches=0,
                   max_abs_difference=0.,max_abs_difference_per_output=[0.]*9,
                   phase_counts={},scan_type_counts={},internal_phase_counts={},first_mismatches=[])for k in range(7)}
    record=struct.Struct('<14d');previous=None
    with (HERE/'oraculo.bin').open('rb')as f:
        while block:=f.read(record.size):
            assert len(block)==record.size
            row=record.unpack(block);case,kind=int(row[0]),int(row[1]);t,v,i=row[2:5]
            if case!=previous:lib.review_reset(kind);previous=case
            lib.review_step(kind,v,i,t,out);assert lib.review_status(kind)==0
            expected=row[5:];actual=list(out);r=results[kind];r['samples']+=1
            delta=[abs(a-b)for a,b in zip(actual,expected)]
            r['max_abs_difference']=max(r['max_abs_difference'],*delta)
            r['max_abs_difference_per_output']=[max(a,b)for a,b in zip(r['max_abs_difference_per_output'],delta)]
            # Continuous outputs: tolerance 1e-11 absolute + 1e-12 relative.
            # Flags and categorical diagnostics must be exact.
            bad=any((d!=0 if j in [2,3,4,6,7,8] else d>1e-11+1e-12*abs(expected[j]))for j,d in enumerate(delta))
            if bad:
                r['mismatches']+=1
                if len(r['first_mismatches'])<3:r['first_mismatches'].append(dict(case=case,t=t,expected=expected,actual=actual))
            for key,val in [('phase_counts',int(out[3])),('scan_type_counts',int(out[7])),('internal_phase_counts',lib.review_internal_phase(kind))]:
                r[key][str(val)]=r[key].get(str(val),0)+1
    boundary=[]
    for k in range(7):
        lib.review_reset(k);lib.review_step(k,12,2,0,out);assert out[0]==.4
        lib.review_step(k,12,2,.02,out);prev=out[0]
        for t,v,i,status in [(.022,math.nan,2,1),(.024,12,math.inf,1),(.026,0,2,1),(.028,12,-1,1),(.028,12,2,2),(.01,12,2,2)]:
            lib.review_step(k,v,i,t,out);assert lib.review_status(k)==status and out[0]==prev
        lib.review_step(k,12,2,.03,out);assert lib.review_status(k)==0
        boundary.append(dict(kind=k,invalid_inputs_hold_duty=True,nonincreasing_time_rejected=True))
    # Running unrelated controllers must not alter any one's state.
    for k in range(7):lib.review_reset(k)
    parallel=[]
    for j in range(30):
        for k in range(7):
            lib.review_step(k,12+.2*math.sin(j),2+.1*math.cos(j),j*.002,out)
            parallel.append((k,j,list(out)))
    for k in range(7):
        lib.review_reset(k)
        for kind,j,e in parallel:
            if kind==k:
                lib.review_step(k,12+.2*math.sin(j),2+.1*math.cos(j),j*.002,out)
                assert list(out)==e
    integration=[]
    stage_out=(ctypes.c_double*5)()
    for k in range(7):
        assert lib.review_mode_period(k+2)==2000
        lib.review_control_reset(k);lib.review_reset(k)
        for j in range(3001):
            v=12+.4*math.sin(j*.03);i=2+.2*math.cos(j*.02)
            assert lib.review_control_step(k,v,i,stage_out)
            lib.review_step(k,v,i,j*.002,out)
            assert stage_out[0]==out[0]
            assert stage_out[3]==2133 and stage_out[4]==1491
            assert 0<=stage_out[2]<=stage_out[4]
        integration.append(dict(kind=k,samples=3001,mode_mapping=True,period_us=2000,
                                stage_duty_and_native_pwm_limits_verified=True))
    # Compile and invoke all seven named entry APIs, not only the dispatcher.
    public_api=[]
    public_out=(ctypes.c_double*9)()
    for k in range(7):
        lib.review_public_init(k);lib.review_reset(k)
        for j in range(3001):
            v=12+.4*math.sin(j*.03);i=2+.2*math.cos(j*.02)
            lib.review_public_update(k,v,i,public_out)
            lib.review_step(k,v,i,j*.002,out)
            assert list(public_out)==list(out)
        lib.review_public_reset(k);lib.review_public_update(k,12,2,public_out)
        assert public_out[0]==.4
        public_api.append(dict(kind=k,samples=3001,init_update_reset_passed=True))
    for n,h in manifest['files'].items():assert sha(FW.parent/n)==h,n
    passed=all(r['mismatches']==0 for r in results.values())
    report=dict(passed=passed,scope='Received core C controllers versus MATLAB reference replays; no physical/closed-loop test.',
        compiler=subprocess.check_output([cc,'--version'],text=True).splitlines()[0],
        target_build_performed=False,hardware_test_performed=False,
        matlab=data['matlab'],cases=len(data['cases']),rows=sum(r['samples']for r in results.values()),
        algorithms=list(results.values()),boundary_tests=boundary,interleaved_instance_test=True,
        integration_tests=integration,wrapped_quiet_window_included=True,
        public_named_api_tests=public_api,
        original_firmware_files_unchanged=True,oracle_sha256=sha(HERE/'oraculo.bin'),
        source_sha256={str(p):sha(p)for p in sources})
    (HERE/'equivalencia_firmware.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:report[k]for k in ['passed','rows','cases','original_firmware_files_unchanged']}))
    for r in results.values():print(r['name'],r['samples'],r['mismatches'],r['max_abs_difference'])
    assert passed,'Mismatch: see RESULTADOS.json'

if __name__=='__main__':main()
