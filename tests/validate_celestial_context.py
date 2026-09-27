#!/usr/bin/env python3
# northlight-test: requires=cxx,client
"""Offline PE proof and native CPU tests. Never starts a client/Wine/device."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import hashlib,json,pathlib,re,struct,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parent
exe=fp.client_root()/'Wow.exe';data=exe.read_bytes()
pe=struct.unpack_from('<I',data,60)[0]
n=struct.unpack_from('<H',data,pe+6)[0];opt=struct.unpack_from('<H',data,pe+20)[0]
base=struct.unpack_from('<I',data,pe+24+28)[0];assert base==0x400000
sections=[struct.unpack_from('<8sIIII',data,pe+24+opt+i*40) for i in range(n)]
def at(address,size):
    for _,_,rva,raw,offset in sections:
        delta=address-base-rva
        if 0<=delta and delta+size<=raw:return data[offset+delta:offset+delta+size]
    raise AssertionError(hex(address))
header=fp.src('celestial_context.h').read_text()
arrays={name:bytes(int(x,16) for x in re.findall(r'0x[0-9a-f]+',values)) for name,values in re.findall(r'unsigned char (\w+)\[\]=\{([^}]+)\}',header)}
proof=[]
for address,name in re.findall(r'\{(0x[0-9a-f]+),(\w+),sizeof \w+\}',header):
    expected=arrays[name];assert at(int(address,16),len(expected))==expected,(address,name)
    proof.append(dict(address=address,name=name,bytes=expected.hex()))
for address,text in [(0xa41b90,b'Textures\\sunCenter.blp\0'),(0xa41b7c,b'Textures\\moon.blp\0'),(0xa41b68,b'Textures\\moon02.blp\0')]:
    assert at(address,len(text))==text
constants={hex(a):struct.unpack('<f',at(a,4))[0] for a in [0xa1047c,0xa393e8,0x9f9878,0xa1ea74]}
assert constants['0xa1047c']==12 and constants['0xa393e8']==1440
assert abs(constants['0x9f9878']-1/1440)<1e-10
# Prove the fixed reference sizes used by the renderer, independently of the
# native clock curve's animated size field. Sun initializes with fld1.
assert at(0x7f296c,8)==bytes.fromhex('d9 e8 d9 15 40 8e d3 00')
assert at(0x7f2989,12)==bytes.fromhex('d9 05 74 ea a1 00 d9 1d 60 8e d3 00')
assert constants['0xa1ea74']==1.75
with tempfile.TemporaryDirectory(prefix='northlight-celestial-') as tmp:
    binary=str(pathlib.Path(tmp)/'test')
    command=['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',*fp.test_include_flags(),str(ROOT/'test_celestial_context.cpp'),'-o',binary]
    subprocess.run(command,check=True)
    test=json.loads(subprocess.check_output([binary],text=True))
report=dict(status='pass',runtime_gpu_test=False,exe_sha256=hashlib.sha256(data).hexdigest(),signatures=proof,constants=constants,cpu_tests=test,
    limitations=['Static byte proof plus synthetic CPU tests; requires user in-game validation.', 'Radiance split and horizon fade are extension policy, not recovered Blizzard photometry.', 'Replacement skyboxes can suppress celestial billboards; raw orbit snapshot does not prove sky render visibility.'],
    source_sha256={name:hashlib.sha256(fp.tracked(name).read_bytes()).hexdigest() for name in ['celestial_context.h','test_celestial_context.cpp','validate_celestial_context.py']})
out=fp.output_dir()/'celestial-context-validation.json';out.write_text(json.dumps(report,indent=2)+'\n');print(out);print(json.dumps(test))
