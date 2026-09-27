#!/usr/bin/env python3
# northlight-test: requires=cxx,client
"""Offline exact-PE camera proof + native unit tests; never starts a game/device."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import hashlib,json,pathlib,re,struct,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parent
exe=fp.client_root()/'WoW.exe';data=exe.read_bytes()
pe=struct.unpack_from('<I',data,60)[0];n=struct.unpack_from('<H',data,pe+6)[0];opt=struct.unpack_from('<H',data,pe+20)[0]
base=struct.unpack_from('<I',data,pe+24+28)[0];assert base==0x400000
sections=[struct.unpack_from('<8sIIII',data,pe+24+opt+i*40) for i in range(n)]
def at(address,size):
 for _,_,rva,raw,offset in sections:
  delta=address-base-rva
  if 0<=delta and delta+size<=raw:return data[offset+delta:offset+delta+size]
 raise AssertionError(hex(address))
def fnv(raw):
 h=14695981039346656037
 for b in raw:h=((h^b)*1099511628211)&((1<<64)-1)
 return h
header=fp.src('world_camera.h').read_text();proof=[]
for address,length,expected in re.findall(r'\{(0x[0-9a-f]+), (\d+), UINT64_C\((0x[0-9a-f]+)\)\}',header):
 raw=at(int(address,16),int(length));assert fnv(raw)==int(expected,16),(address,length)
 proof.append(dict(address=address,size=int(length),fnv64=expected,sha256=hashlib.sha256(raw).hexdigest()))
assert len(proof)==11
assert struct.unpack('<4I',at(0xa1ea54,16))==(0x5ff500,0x600c20,0x600cc0,0x600d60)
assert struct.unpack('<I',at(0xa2e718+0xa4,4))[0]==0x6a9e00
with tempfile.TemporaryDirectory(prefix='northlight-camera-') as tmp:
 binary=str(pathlib.Path(tmp)/'test')
 subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',*fp.test_include_flags(),str(ROOT/'test_world_camera.cpp'),'-o',binary],check=True)
 results=json.loads(subprocess.check_output([binary],text=True))
report=dict(status='pass',runtime_gpu_test=False,game_process_started=False,exe_sha256=hashlib.sha256(data).hexdigest(),signatures=proof,cpu_tests=results,
 limitations=['No live validation: compare agreesWithTerrain on recognized terrain draws in a game test.', 'Camera transport attachment at +0xa0/+0xa4 fails closed; its additional object rotation is not reconstructed.', 'Only active camera vtable0xa1ea54 and D3D device vtable0xa2e718 are audited.', 'Caller must still validate known world shader projection, full viewport and main world depth target; camera context does not identify a draw pass.', 'Camera reader supplies no lighting/fog values. Those require independent shader register evidence.'],
 source_sha256={name:hashlib.sha256(fp.tracked(name).read_bytes()).hexdigest() for name in ['world_camera.h','test_world_camera.cpp','validate_world_camera.py']})
output=fp.output_dir()/'world-camera-validation.json';output.write_text(json.dumps(report,indent=2)+'\n');print(output);print(json.dumps(results))
