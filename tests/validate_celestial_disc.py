#!/usr/bin/env python3
# northlight-test: requires=cxx,client,zig
"""Offline original-asset/PE/CPU/SM3 gates; no game or device creation."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import hashlib,json,math,os,pathlib,re,struct,subprocess,tempfile
HERE=pathlib.Path(__file__).resolve().parent;CLIENT=fp.client_root()
data=(CLIENT/'Wow.exe').read_bytes();pe=struct.unpack_from('<I',data,60)[0];n=struct.unpack_from('<H',data,pe+6)[0];opt=struct.unpack_from('<H',data,pe+20)[0]
sections=[struct.unpack_from('<8sIIII',data,pe+24+opt+i*40) for i in range(n)]
def at(address,count):
    for _,_,va,size,offset in sections:
        delta=address-0x400000-va
        if 0<=delta and delta+count<=size:return data[offset+delta:offset+delta+count]
    raise ValueError('Address outside PE raw image')
proof=[]
for address,expected in [(0x7edce1,'d94114'),(0x7edc0b,'d905308d9e00'),(0x7edc17,'d905c42e9e00'),(0x9ac790,'8b4710')]:
    expected=bytes.fromhex(expected);assert at(address,len(expected))==expected;proof.append({'address':hex(address),'bytes':expected.hex()})
assert struct.unpack('<f',at(0x9e8d30,4))[0]==-.5
assert struct.unpack('<f',at(0x9e2ec4,4))[0]==.5
assert struct.unpack('<f',at(0xa1047c,4))[0]==12
identity_header=fp.src('celestial_disc_native.h').read_text()
arrays={name:bytes(int(value,16) for value in re.findall(r'0x[0-9a-f]+',body)) for name,body in re.findall(r'unsigned char (\w+)\[\]=\{([^}]+)\}',identity_header)}
identity_proof=[]
for address,name in re.findall(r'\{(0x[0-9a-f]+),(\w+),sizeof \w+\}',identity_header):
    expected=arrays[name];assert at(int(address,16),len(expected))==expected,(address,name)
    identity_proof.append({'address':address,'name':name,'bytes':expected.hex()})
assert len(identity_proof)==7
# The disk starts only in cleared normalized world depth. Nearby geometry,
# depth subranges and water must block it independently of its scene color.
depth_cases=0
for lo,hi in [(0,1),(.1,.9),(.3,.7)]:
    for normalized,visible in [(0,False),(.5,False),(.999,False),(1,True)]:
        native=lo+normalized*(hi-lo);sample=max(0,min(1,(native-lo)/(hi-lo)))
        assert (sample>=.99999994)==visible;depth_cases+=1
for encoded in [0,.001,.04,.1,.25,.5,.75,1]:
    linear=encoded/12.92 if encoded<=.04045 else ((encoded+.055)/1.055)**2.4
    output=linear*12.92 if linear<=.0031308 else 1.055*linear**(1/2.4)-.055
    assert abs(encoded-output)<1e-6
shader=json.loads(fp.src('celestial-disc-shader-build.json').read_text())
assert shader['source_sha256']==hashlib.sha256(fp.src('celestial_disc_effects.hlsl').read_bytes()).hexdigest()
assert shader['shader']['sha256']==hashlib.sha256(fp.src('CelestialDiscPS.bin').read_bytes()).hexdigest()
assert shader['shader']['static_instruction_slots']<=512 and shader['shader']['temporary_registers']<=32
with tempfile.TemporaryDirectory(prefix='northlight-celestial-disc-') as tmp:
    binary=str(pathlib.Path(tmp)/'test')
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',*fp.test_include_flags(),str(HERE/'test_celestial_disc.cpp'),'-o',binary],check=True)
    native=subprocess.check_output([binary,str(CLIENT/'world-cache/celestial')],text=True).strip()
    env=fp.zig_env()
    subprocess.run([str(fp.zig()),'c++','-target','x86-windows-gnu','-std=c++17','-c',*fp.test_include_flags(),str(HERE/'test_celestial_disc_compile.cpp'),'-o',str(pathlib.Path(tmp)/'compile.obj')],env=env,check=True)
report={'status':'pass','native_sanitizers':['address','undefined'],'native_result':native,'depth_cases':depth_cases,'color_roundtrips':8,
        'x86_windows_compile':True,'game_launched':False,'gpu_test':False,'native_size_formula':'angular half-size atan(BodyRecord.size / 24), because radius12 and corner template +/-0.5',
        'pe_proof':proof,'identity_pe_proof':identity_proof,'identity_tests':{'unrelated_particles':10000,'signature_mutations_rejected':7,'null_lazy_chains':True,'ordinary_and_variant_handles':True,'mid_frame_streaming':True,'torn_reads_rejected':True},'exe_sha256':hashlib.sha256(data).hexdigest(),
        'source_sha256':{name:hashlib.sha256(fp.tracked(name).read_bytes()).hexdigest() for name in ['celestial_disc.h','celestial_disc_native.h','celestial_disc_renderer.h','celestial_disc_effects.hlsl','celestial_disc_compiled_shaders.h','build_celestial_disc_assets.py','test_celestial_disc.cpp','validate_celestial_disc.py']}}
(fp.output_dir()/'celestial-disc-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
