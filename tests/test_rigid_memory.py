#!/usr/bin/env python3
# northlight-test: requires=cxx,client,stormlib,world-cache
"""0.3.172 rigid memory: native model test of rigid_memory.h and rigid_geometry.h (clang++, plain and
ASan/UBSan): settling, sticky mobile/held/static, identity without the snapshot pointer, absence and the
in-view despawn test, caps, the rebase round trip, the real client one-influence program with a Stormwind
sign end to end and the static-doodad flood on real world-cache placements. Wiring audit of the renderer
side (world_rigid_memory.inl): observed before retainSelected, injected after it and before the bounds
kick and upload, copies own their constant banks and hold no texture when opaque, cleared on device
loss/reset/trim/map change/shadows off, never touches the static cache. No game or GPU."""
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
checks['at most 4096 replays, outside quota/radius/fate']=('if(replays.size()+e.payload.draws.size()>=4096' in inject and 'p->fateSlot=-1;' in inject
    and 'p->shadowSkinned=p->shadowSelected=true;' in inject)
checks['cleared in releaseGPU (reset() calls it), trimMemory, on a map change and with shadows off']=('staticCasters.settle();rigidMemoryClear();' in w
    and re.search(r'void reset\(\)\{[^\n]*releaseGPU\(\);\}',w) is not None and 'rigidMemoryClear(); /* 0.3.172' in g[g.index('MemoryTrim trimMemory(){'):g.index('void setMemoryPressure')]
    and 'if(lastRequest.map!=rigidMap){rigidMemoryClear();' in m and 'if(!effects.shadows){rigidMemoryClear();return;}' in m)
checks['never the static cache']=all(n not in m for n in ('staticCasters','shadowCacheKey','invalidateShadowCache','staticSignature'))
checks['observes every captured skinned group; bodies are the non-rigid ones; identity by mixShape']=('if(!p.shadowSkinned)continue;' in m and 'rigidBodies.insert(' in m
    and 'NorthlightRigidMemory::mixShape(shape,p.originalShader,p.decl,p.mesh().vertexCount,p.mesh().primitiveCount,p.mesh().byteSize());' in m and 'shared.get()' not in m)
checks['shortfall frames record and draw; only the despawn test needs a complete frame']='!captureShortfall,' in m and m.count('captureShortfall')==1
checks['logged on sampled frames']='if(captureSampled){const auto& s=rigidMemory.stats();' in inject and 'logf("RIGID memory tracks=%zu entries=%zu injected=%u seen=%zu held=%zu static=%zu mobile=%zu droppedInView=' in inject
capture=w[w.index('    void captureModel(D3DPRIMITIVETYPE type,'):w.index('    // One directional replay draw in the 0.3.142 order')]
drawn='if(!rigidDrawKeys.empty()&&rigidDrawKeys.contains(current,count))rigidMemoryDrawn(current,count);'
checks['0.3.173 drawn test: only with entries, right after the shader lookup, before the 4096 cap, budget, blend and projection checks']=(capture.count(drawn)==1
    and capture.index('const auto& metadata=it->second;')<capture.index(drawn)<capture.index('if(replays.size()>=4096)')<capture.index('if(replaySnapshots.captureExhausted(priority))')
    and capture.index(drawn)<capture.index('D3DRS_ALPHABLENDENABLE')<capture.index('kind==1?4:2,q,4'))
checks['drawn test reuses the mirror-answered palette rows of drawRoot (one read site), bone 0 at c31']=(w.count('GetVertexShaderConstantF(UINT(program->second.paletteBase),rows,3)')==1
    and 'float rows[12];const auto* program=paletteRows(shader,rows);if(!program)return false;' in w and 'const auto* program=paletteRows(shader,rows);' in m and 'program->paletteBase!=31' in m)
observe=m[m.index('    void rigidMemoryObserve(){'):m.index('    void rigidMemoryInject(){')]
checks['key set rebuilt after store every capture frame, cleared with the memory; drawn marks per frame']=(observe.index('rigidMemory.store(rigidObservations[n],rigidCopy(n),now);')<observe.index('rigidDrawKeysRebuild();')
    and 'rigidDrawKeys.clear();}' in m and 'rigidMemory.clearDrawn(); /* 0.3.173' in w[w.index('    void endFrame('):])
checks['LiveUnselected: captured non-small draws only (small at capture never observed), no copy']=('if(!p.shadowSelected){if(p.shadowSmall)continue;' in m and 'u.selected=false;' in m
    and 'p->shadowSmall=smallShadow;' in w)
checks['RIGID event lines: Diagnostics only, 20 per second, 2000 a session']=('if(NorthlightDiagnostics::enabled()){rigidMemory.takeEvents(rigidEvents);' in m and 'RigidEventsPerSecond=20,RigidEventLines=2000;' in m
    and 'if(!rigidEventTokens||rigidEventLines>=RigidEventLines){++rigidEventSuppressed;continue;}' in m and 'rigidMemory.events(NorthlightDiagnostics::enabled());' in m)
checks['counters reset on a map change; doodad bodies from the static index']=('if(lastRequest.map!=rigidMap){rigidMemoryClear();rigidMemory.resetStats();' in m and 'return rigidStaticBody(root);' in m)
checks['placement index stepped while incomplete with tracks (doodad bodies after a revision change), same per-frame step']=(
    'if(rigidMemory.screening()||(rigidMemory.stats().tracks&&!rigidIndexCurrent()))rigidIndexStep();' in observe
    and observe.index('rigidMemory.frame(')<observe.index('rigidIndexStep();') and 'static constexpr size_t RigidIndexStep=2048;' in m
    and m.count('rigidIndexStep()')==2 and 'if(!rigidIndexCurrent())return false;const auto& x=rigidIndex;' in m
    and re.search(r'bool rigidIndexCurrent\(\)const\{\s*return staticScene&&staticScene->map==lastRequest.map&&rigidIndex.scene==staticScene.get\(\)&&rigidIndex.revision==rigidSceneRevision\(\*staticScene\)&&rigidIndex.complete;\}',m) is not None)
hdr=fp.src('rigid_memory.h').read_text()
checks['tracks: new ones sorted and merged into the ordered survivors (no full sort), partial reindex']=('std::inplace_merge(tracks_.begin(),middle,tracks_.end(),trackBefore);' in hdr
    and 'std::sort(middle,tracks_.end(),trackBefore);' in hdr and 'std::sort(tracks_.begin(),tracks_.end()' not in hdr and 'for(std::size_t i=start;i<tracks_.size();++i)trackIndex_[tracks_[i].serial]=i;' in hdr)
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
