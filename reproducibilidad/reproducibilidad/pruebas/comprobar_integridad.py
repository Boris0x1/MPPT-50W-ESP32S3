"""Check the published reference files without running a simulation."""
from pathlib import Path
import argparse
import hashlib
import json
import sys

HERE=Path(__file__).resolve().parent
REPRO=HERE.parent

def digest(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo',type=Path,default=REPRO.parent)
    parser.add_argument('--solo-simulacion',action='store_true')
    args=parser.parse_args();repo=args.repo.resolve()
    manifest=json.loads((REPRO/'configuracion/MANIFIESTO_ARCHIVOS.json').read_text(encoding='utf-8'))
    checks=[]
    for item in manifest['files']:
        group=item['group']
        if args.solo_simulacion and group not in ['simulation','reproducibility']:continue
        base=REPRO if group=='reproducibility' else repo
        p=base/item['path']
        if not p.exists() and group=='experimental_data':
            matches=[q for q in (repo/'datos').rglob('*.csv')
                     if digest(q)==item['sha256']] if (repo/'datos').exists() else []
            if matches:p=matches[0]
        if not p.exists():
            status='MISSING' if item['required'] else 'NOT_AVAILABLE'
        else:status='PASS' if digest(p)==item['sha256'] else 'MISMATCH'
        checks.append(dict(group=group,path=str(p),status=status))
    failed=[q for q in checks if q['status'] in ['MISSING','MISMATCH']]
    out=REPRO/'resultados';out.mkdir(exist_ok=True)
    result=dict(passed=not failed,repository=str(repo),checks=checks,
                scope='File identity only; not physical validation or numerical reproduction.')
    (out/'integridad.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    for status in ['PASS','NOT_AVAILABLE','MISSING','MISMATCH']:
        print(status,sum(q['status']==status for q in checks))
    if failed:
        for q in failed:print(q['status'],q['path'])
        return 1
    return 0

if __name__=='__main__':sys.exit(main())
