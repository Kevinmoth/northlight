#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native CPU gates only; no game, Wine, or graphics device."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import hashlib,json,subprocess,tempfile
here=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='northlight-worker-test-') as td:
    source=[*fp.test_include_flags(),str(here/'test_worker_optimization.cpp'),str(fp.src('world_gi.cpp'))]
    binary=str(Path(td)/'test')
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',*source,'-o',binary],check=True)
    sanitizer=json.loads(subprocess.check_output([binary],text=True))
    subprocess.run(['c++','-std=c++17','-O2',*source,'-o',binary],check=True)
    native=json.loads(subprocess.check_output([binary],text=True))
    gi=str(Path(td)/'gi')
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',*fp.test_include_flags(),str(here/'test_world_gi.cpp'),str(fp.src('world_gi.cpp')),'-o',gi],check=True)
    gi_test=subprocess.check_output([gi],text=True,cwd=td)
report={'status':'pass','sanitizer':sanitizer,'native_benchmark':native,'world_gi':gi_test.strip().splitlines()[-1],
        'limitations':'CPU synthetic benchmark, not observed in-game FPS; no graphics device was created.',
        'source_sha256':{n:hashlib.sha256(fp.tracked(n).read_bytes()).hexdigest() for n in ['world_gi.h','world_gi.cpp','worker_actor_memo.h','test_worker_optimization.cpp','validate_worker_optimization.py']}}
(fp.output_dir()/'worker-optimization-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
