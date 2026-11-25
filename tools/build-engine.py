#!/usr/bin/env python3
"""Compile native engine objects using the audited compilation manifest."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, default=root/'.deps/native/ps5-payload-sdk')
    parser.add_argument('--verify-reference', action='store_true', help='Require exact reference object checksums')
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    if not (sdk/'bin/prospero-lld').is_file():
        parser.error('PS5 payload SDK not found; initialize dependencies first')
    objroot = root/'engine/native-objects'
    records = json.loads((objroot/'compile-plan.json').read_text())
    manifest = (objroot/'objects.txt').read_text().splitlines()
    if len(manifest)!=77 or len(set(manifest))!=77 or manifest!=[r['object'] for r in records]:
        raise SystemExit('Invalid engine manifest')
    env = dict(os.environ, PS5_PAYLOAD_SDK=str(sdk))
    plans=[]
    for r in records:
        name=r['object']
        if not re.fullmatch(r'engine/native-objects/[A-Za-z0-9_./-]+\.c\.o',name) or '..' in Path(name).parts:
            raise SystemExit(f'Invalid object path: {name}')
        source='engine/upstream/'+name.removeprefix('engine/native-objects/').split('/',1)[1][:-2]
        cmd=[a.replace('${PROJECT_ROOT}',str(root)) for a in r['command']]
        if cmd[:2]!=['sh',str(root/'tooling/prospero-clang18')] or cmd.count('-c')!=1 or cmd.count('-o')!=1 or cmd[-4:]!=['-c',str(root/source),'-o',str(root/name)]:
            raise SystemExit(f'Unexpected command: {name}')
        if not (root/source).is_file(): raise SystemExit(f'Missing source: {source}')
        if not re.fullmatch('[0-9a-f]{64}',r['sha256']): raise SystemExit('Invalid checksum')
        for a in cmd[2:-4]:
            if a.startswith('-I'+str(root)+'/engine/upstream/') and Path(a[2:]).is_dir() and Path(a[2:]).resolve().is_relative_to(root/'engine/upstream'): continue
            if a not in {'-O3','-DNDEBUG','-std=gnu11','-fPIC','-fno-strict-aliasing','-Wall','-Wextra','-DORUNNERS_CONSOLE','-DORUNNERS_VERSION="1.3.2"'}:
                raise SystemExit(f'Unexpected compiler argument: {a}')
        plans.append((r,cmd))
    identical=warnings=0
    # Stage every object before replacing any output. A failed compile never promotes a partial set.
    with tempfile.TemporaryDirectory(prefix='.engine-build-',dir=objroot) as td:
        staging=Path(td)
        for i,(r,cmd) in enumerate(plans,1):
            dst=staging/str(i)
            print(f'[{i}/77] {r["object"]}',flush=True)
            result=subprocess.run(cmd[:-1]+[str(dst)],cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
            if result.stdout: print(result.stdout,end='',flush=True)
            result.check_returncode()
            warnings+=result.stdout.count('warning:')
            data=dst.read_bytes()
            if data[:6]!=b'\x7fELF\x02\x01' or data[16:20]!=b'\x01\x00\x3e\x00': raise SystemExit('Unexpected object format')
            actual=hashlib.sha256(data).hexdigest()
            match=actual==r['sha256']; identical+=match
            print(f'  {"IDENTICAL" if match else "DIFFERENT"} {actual}',flush=True)
        print(f'Compiled: 77; identical: {identical}; warnings: {warnings}',flush=True)
        if args.verify_reference and (identical!=77 or warnings): raise SystemExit('Reference verification failed; no objects promoted')
        for i,(r,_) in enumerate(plans,1):
            dst=root/r['object']
            dst.parent.mkdir(parents=True,exist_ok=True)
            os.replace(staging/str(i),dst)
    print('Engine build complete.')


if __name__=='__main__':
    main()
