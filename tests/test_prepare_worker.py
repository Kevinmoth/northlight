#!/usr/bin/env python3
# northlight-test: requires=cxx,client,stormlib
"""0.3.177 (r83 a1-prepare): the actor prepare worker. Native test of prepare_worker.h
(test_prepare_worker.cpp: handover at every index, the threaded worker and join with random delays and
stops, exception and watchdog takeover, cache independence, and the counterfactuals: state reset at the
handover, unfiltered records, the live declaration cache after an eviction, the camera at the join),
built O2, ASan+UBSan and TSan; and a source audit: the worker's header touches no renderer state. No game
or GPU."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import client_fixtures  # the real client programs, from the tester's client
import ast,re,subprocess,tempfile
HERE=Path(__file__).resolve().parent
def literal(path,name):
    return next(ast.literal_eval(n.value) for n in ast.parse(path.read_text()).body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets))
stub=literal(HERE/'test_terrain_snapshot.py','stub')
deformation=literal(HERE/'test_actor_deformation.py','harness')
code=deformation[deformation.index('std::vector<Word> code(unsigned major){'):deformation.index('int main(')]
worker=re.sub(r'/\*.*?\*/','',re.sub(r'//[^\n]*','',fp.src('prepare_worker.h').read_text()),flags=re.S)
checks={
 'the worker header touches no renderer state (actorPrograms, declarationCache, replays, the device)':
    not any(n in worker for n in ('actorPrograms','declarationCache','replays','d->','IDirect3DDevice9','context.')),
}
for name,ok in checks.items():print(('PASS ' if ok else 'FAIL ')+name)
assert all(checks.values())
with tempfile.TemporaryDirectory(prefix='northlight-prepare-worker-') as tmp:
    p=Path(tmp);(p/'d3d9.h').write_text(stub);client_fixtures.actor_client_programs(p)
    (p/'test_prepare_worker.cpp').write_text((HERE/'test_prepare_worker.cpp').read_text().replace('/*SYNTHETIC_PROGRAMS*/',code))
    four=str(client_fixtures.four_bone_vs3())
    for label,flags,mode in (('O2',['-O2'],'full'),('asan',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'],'full'),('tsan',['-O1','-g','-fsanitize=thread'],'tsan')):
        exe=p/('test-'+label)
        subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-UNDEBUG',*flags,'-I',str(p),*fp.test_include_flags(),str(p/'test_prepare_worker.cpp'),str(fp.src('world_gi.cpp')),'-o',str(exe)],check=True)
        out=subprocess.run([str(exe),four,mode],capture_output=True,text=True)
        if out.returncode or 'WARNING' in out.stderr or 'ERROR' in out.stderr:
            sys.exit(f'{label} failed ({out.returncode}):\n{out.stdout}{out.stderr[-6000:]}')
        print(f'[{label}]',out.stdout.strip(),flush=True)
print('PASS prepare worker: model, threads and counterfactuals')
