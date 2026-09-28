#!/usr/bin/env python3
# northlight-test: requires=cxx,client,stormlib,world-cache
"""0.3.172 rigid memory: native model test of rigid_memory.h and rigid_geometry.h (clang++, plain and
ASan/UBSan): settling, sticky mobile/held/static, identity without the snapshot pointer, absence and the
in-view despawn test, caps, the rebase round trip, the real client one-influence program with a Stormwind
sign end to end and the static-doodad flood on real world-cache placements. Wiring audit of the renderer
side (world_rigid_memory.inl): observed before retainSelected, injected after it and before the bounds
kick and upload, copies own their constant banks and hold no texture when opaque, cleared on device
loss/reset/trim/map change/shadows off, never touches the static or persistent cache. No game or GPU."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import client_fixtures  # the real client programs and placements, from the tester's client and world cache
import ast,re,subprocess,tempfile
HERE=Path(__file__).resolve().parent
def literal(path,name):
 return next(ast.literal_eval(n.value) for n in ast.parse(path.read_text()).body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets))
stub=literal(HERE/'test_terrain_snapshot.py','stub')
m=fp.src('world_rigid_memory.inl').read_text();w=fp.src('world_renderer.h').read_text();x=fp.src('world_shadow_experiment.inl').read_text();g=fp.src('world_memory_guard.inl').read_text()
checks={}
select=x[x.index('    void selectShadowReplays(){'):x.index('    // Called once per frame after selection')]
checks['observed before retainSelected, injected right after it, both inside the selection try']=(select.count('rigidMemoryObserve();')==1 and select.count('rigidMemoryInject();')==1
    and select.index('try{')<select.index('rigidMemoryObserve();')<select.index('NorthlightReplayShadowPolicy::retainSelected(replays,heldShadowReplays);')<select.index('rigidMemoryInject();')<select.index('}catch(...){'))
render=w[w.index('    bool render(IDirect3DSurface9* targetSurface'):]
checks['injection before the bounds kick and the replay upload']=render.index('if(replayShadows)selectShadowReplays();')<render.index('replayBoundsKick();')<render.index('return uploadReplay();')
inject=m[m.index('    void rigidMemoryInject(){'):]
checks['copies own their constant bank (constants==constantStorage), rebased bone rows, unique groups']=('NorthlightReplayCaptureConstants::reset(*p);' in inject
    and 'std::memcpy(p->constantStorage,c.constants.data(),sizeof p->constantStorage);' in inject and 'std::memcpy(p->constantStorage+4*(31+3*e.payload.bone),rows,sizeof rows);' in inject
    and 'NorthlightRigidMemory::rebase(e.world,context.inverseView,rows)' in inject and 'p->constantGroup=++group;' in inject and 'p->constantStamp' not in inject.replace('reset(*p)',''))
checks['opaque copies hold no texture; no game VB/IB is held']=('c.texture=RigidRef<IDirect3DBaseTexture9>(p.cutoff>=0?p.texture:nullptr);' in m and re.search(r'(p|c)(\.|->)(stream|index)\b',m) is None)
checks['at most 4096 replays, outside quota/radius/fate/persistent']=('if(replays.size()+e.payload.draws.size()>=4096' in inject and 'p->persistentId=0;' in inject and 'p->fateSlot=-1;' in inject
    and 'p->shadowSkinned=p->shadowSelected=true;' in inject)
checks['cleared in releaseGPU (reset() calls it), trimMemory, on a map change and with shadows off']=('releasePersistentGPU();rigidMemoryClear();' in w
    and re.search(r'void reset\(\)\{[^\n]*releaseGPU\(\);\}',w) is not None and 'rigidMemoryClear(); /* 0.3.172' in g[g.index('MemoryTrim trimMemory(){'):g.index('void setMemoryPressure')]
    and 'if(lastRequest.map!=rigidMap){rigidMemoryClear();' in m and 'if(!effects.shadows){rigidMemoryClear();return;}' in m)
checks['never the static or persistent cache']=all(n not in m for n in ('persistentSignature','persistentCasters','staticCasters','shadowCacheKey','invalidateShadowCache','staticSignature'))
checks['observes every captured skinned group; bodies are the non-rigid ones; identity by mixShape']=('if(!p.shadowSkinned||p.persistentId)continue;' in m and 'rigidBodies.insert(' in m
    and 'NorthlightRigidMemory::mixShape(shape,p.originalShader,p.decl,p.mesh().vertexCount,p.mesh().primitiveCount,p.mesh().byteSize());' in m and 'shared.get()' not in m)
checks['shortfall frames record and draw; only the despawn test needs a complete frame']='!captureShortfall,' in m and m.count('captureShortfall')==1
checks['logged on sampled frames']='if(captureSampled){const auto& s=rigidMemory.stats();' in inject and 'logf("RIGID memory tracks=%zu entries=%zu injected=%u seen=%zu held=%zu static=%zu mobile=%zu droppedInView=' in inject
for name,ok in checks.items():print(('PASS ' if ok else 'FAIL ')+name)
assert all(checks.values())
with tempfile.TemporaryDirectory(prefix='northlight-rigid-memory-') as tmp:
 p=Path(tmp);(p/'d3d9.h').write_text(stub)
 client_fixtures.actor_client_programs(p);client_fixtures.rigid_placements(p)
 for label,flags in (('O2',['-O2']),('asan',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])):
  exe=p/('test-'+label)
  subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-UNDEBUG','-I',str(p),*fp.test_include_flags(),str(HERE/'test_rigid_memory.cpp'),str(fp.src('world_gi.cpp')),'-o',str(exe)],check=True)
  print(f'[{label}]',flush=True);subprocess.run([str(exe),str(client_fixtures.four_bone_vs3())],check=True)
print('PASS rigid memory: model and wiring')
