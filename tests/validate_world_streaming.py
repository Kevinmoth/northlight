#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native staging policy/resource ownership regression; no game or D3D device."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import hashlib,json,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='northlight-staging-') as tmp:
    binary=str(pathlib.Path(tmp)/'test')
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',*fp.test_include_flags(),str(root/'test_world_streaming.cpp'),str(fp.src('world_mesh_plan.cpp')),'-o',binary],check=True)
    tests=json.loads(subprocess.check_output([binary],text=True))
report={'status':'pass','runtime_gpu_test':False,'tests':tests,'source_sha256':{name:hashlib.sha256(fp.tracked(name).read_bytes()).hexdigest() for name in ['world_streaming.h','test_world_streaming.cpp','validate_world_streaming.py','world_mesh_plan.cpp']}}
out=fp.output_dir()/'world-streaming-validation.json';out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
