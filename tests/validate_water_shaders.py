#!/usr/bin/env python3
# northlight-test: requires=cxx,zig,wine,client,stormlib
"""Offline D3D disassembly and resource gate; no graphics device/game launch.
0.3.161: the mask variants are built here from the tester's client originals (patch(),
byte-identical to the DLL's runtime patch, test_water_shaders.py) and everything is
written under the test output directory, never into tests/fixtures."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
from pathlib import Path
import os,subprocess,json,re,hashlib
from extract_water_shaders import accepted_originals,patch
from build_environment import wine_build_prefix, zig_env, ZIG
from compile_world_shaders import validate_assembly,HERE,WINE_ROOT   # HERE: scripts/shaders (the tool sources)

def main():
    binaries=fp.output_dir()/'water-mask-bin';out=fp.output_dir()/'water-mask-assembly'
    for d in (binaries,out):d.mkdir(exist_ok=True)
    for record,original in accepted_originals():(binaries/(record['hash']+'.bin')).write_bytes(patch(original)[0])
    env=zig_env()
    subprocess.run([str(ZIG),'c++','-target','x86-windows-gnu','-O2','-s','-static',*fp.include_flags(),str(HERE/'disassemble_water_shaders.cpp'),'-o',str(HERE/'disassemble_water_shaders.exe')],check=True,env=env)
    env.update(WINEPREFIX=str(wine_build_prefix()),WINEDEBUG='-all',DYLD_LIBRARY_PATH=str(WINE_ROOT/'lib/external'))
    subprocess.run([str(WINE_ROOT/'bin/wine'),str(HERE/'disassemble_water_shaders.exe'),str(binaries),str(out)],check=True,env=env,timeout=120)
    report=json.loads(fp.src('water-shader-identities.json').read_text());results=[]
    for entry in report['accepted']:
        path=out/(entry['hash']+'.asm');assembly=path.read_text();stats=validate_assembly(path,'vs_3_0' if entry['vertex'] else 'ps_3_0')
        cubes=set(re.findall(r'dcl_cube\s+s(\d+)',assembly))
        cubeReads=sum(1 for line in assembly.splitlines() if re.match(r'\s*texld\b',line) and any(re.search(r'\bs'+s+r'\b',line) for s in cubes))
        stats['static_instruction_slots']+=3*cubeReads
        if stats['static_instruction_slots']>(256 if entry['vertex'] and entry['model']==2 else 512):raise ValueError('slot overflow '+entry['hash'])
        if entry['model']==2 and not entry['vertex']:
            textures=sum(stats['instructions'].get(op,0) for op in ('texld','texldp','texldb','texkill'))
            arithmetic=stats['static_instruction_slots']-textures-3*cubeReads
            if arithmetic>64 or textures>32:raise ValueError('PS2 arithmetic/texture overflow '+entry['hash'])
            stats.update(arithmetic_slots=arithmetic,texture_instructions=textures)
        results.append({'hash':entry['hash'],'vertex':entry['vertex'],'model':entry['model'],**stats})
    checks={'game_launched':False,'graphics_device_created':False,'disassembled_variants':len(results),'variants':results,'files':{p:hashlib.sha256(fp.tracked(p).read_bytes()).hexdigest() for p in ['water_renderer.h','water_effects.hlsl','water_shader_identities.h','water_shader_patch.h','water_compiled_shaders.h','extract_water_shaders.py','test_water_shaders.py']}}
    (fp.output_dir()/'water-validation.json').write_text(json.dumps(checks,indent=2)+'\n');print('All water mask shader resource gates passed:',len(results))
if __name__=='__main__':main()
