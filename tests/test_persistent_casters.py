#!/usr/bin/env python3
# northlight-test: requires=cxx,client,stormlib,world-cache slow
"""0.3.141 persistent casters: transform accuracy vs the replay VS emulation (real
client four-bone SM3 shader), conversion gates, registry lifecycle, memory cap,
cache-slot invalidation, worker thread (native clang++, plain, ASan/UBSan, TSan);
rigid props (PersistentRigidProps): lifecycle, held-item rule, cutout parity, renderer wiring;
the real client one-influence program (signs): exact template, sign pipeline, static-doodad flood;
diagnostics (PERSISTENT near lines): explain() reasons, removal events, recording off = identical decisions, renderer wiring."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import client_fixtures  # the real client programs, from the tester's client
from pathlib import Path
import ast,subprocess,tempfile
HERE=Path(__file__).resolve().parent
def literal(path,name):
 return next(ast.literal_eval(n.value) for n in ast.parse(path.read_text()).body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets))
stub=literal(HERE/'test_terrain_snapshot.py','stub')
with tempfile.TemporaryDirectory(prefix='northlight-persistent-') as tmp:
 p=Path(tmp);(p/'d3d9.h').write_text(stub)
 # 0.3.150: the 0.3.149 registry beside this one (track order differential, test_persistent_casters.cpp trackOrder()).
 reference=(fp.FIXTURES/'reference/persistent_casters-0.3.149.h').read_text();assert reference.count('namespace NorthlightPersistentCasters {')==1
 (p/'persistent_casters_0149.h').write_text(reference.replace('namespace NorthlightPersistentCasters {','namespace NorthlightPersistentCasters0149 {'))
 client_fixtures.actor_client_programs(p)   # actor_client_programs.h: the one-influence client programs
 client_fixtures.rigid_placements(p)   # stormwind_rigid_placements.inc: real doodad placements from the world cache
 for label,flags in (('O2',['-O2']),('asan',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']),('tsan',['-O1','-g','-fsanitize=thread'])):
  exe=p/('test-'+label)
  subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-UNDEBUG','-I',str(p),*fp.test_include_flags(),str(HERE/'test_persistent_casters.cpp'),str(fp.src('world_gi.cpp')),'-o',str(exe)],check=True)
  print(f'[{label}]',flush=True);subprocess.run([str(exe),str(client_fixtures.four_bone_vs3())],check=True)
# Renderer wiring of the rigid-prop class (source audit: the .inl needs the device).
src=fp.src('world_persistent_casters.inl').read_text();world=fp.src('world_renderer.h').read_text();state=fp.src('replay_draw_state.h').read_text()
for needle in ('(quality.persistentCasters||quality.persistentRigidProps)','persistentCasters.classes(quality.persistentCasters!=0,quality.persistentRigidProps!=0);',
 # rigid props: complete captures, this map's static scene (<=15 s wait), not static-cache covered, not held by a body
 'if(o.rigid){if(captureShortfall||((!staticScene||staticScene->map!=lastRequest.map)&&now-persistentMapMs<15000)){persistentCasters.deferRigid(o,now,false);continue;}',
 'const int covered=persistentStaticCovered(o);if(covered<0)continue; /* scene still being indexed */if(covered){persistentCasters.covered(o,now);continue;}',
 'const auto hold=persistentBodyHeld(c);if(hold!=NorthlightPersistentCasters::Registry::Free){persistentCasters.deferRigid(o,now,true,hold==NorthlightPersistentCasters::Registry::Held);continue;}}',
 # rootWorld/basis failures are located holders too
 'if(!audited)other();if(!head||!audited)continue;','context.inverseView,o.root)){other();continue;}','if(!finite){other();continue;}',
 # body box: budget -> later (defer), owned/invalid/no POSITION -> never (held within attachReach); unaudited groups within attachReach, unlocated or overflow -> undecided
 'if(p.shared&&(cached==persistentBones.end()||cached->second.owner.lock()!=p.shared))body.known=R::BodyLater;return body.known;',
 'if(g.located!=1)return R::Undecided;','if(hold==R::Held)return hold;if(persistentOthersOverflow)return R::Undecided;','const float r=persistentCasters.tuning().attachReach;',
 'if(quality.persistentRigidProps)persistentIndexStep();','x.complete=x.next==all.size();','else if(persistentCasters.attachedToActor(obs,c,',
 'if(place.category==1||place.category==3)x.add(','NorthlightPersistentCasters::staticPlacement(o.root,o.axes,t,place.matrix)',
 'persistentJob(o,*job,textureBytes,alpha,o.rigid)','if(p.cutoff>=0&&!NorthlightPersistentCasters::AlphaCutouts&&!cutouts){alpha=true;return false;}',
 'NorthlightPersistentCasters::envelope(boxes,worlds,bones,l,h)',
 # cutouts: the <=128 px copy of the replay's texture with the replay's cutoff, address modes and sampler
 'if(uv==actorUVPrograms.end()||!actorMaterial(p.texture,packet.texture)){alpha=true;return false;}',
 'packet.material.alphaCutoff=p.cutoff;packet.material.addressU=p.addressU;packet.material.addressV=p.addressV;',
 'd->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);',
 'float material[]={1,1,1,opaque?-2.f:cutoff};','d->SetTexture(0,opaque?nullptr:g.textures[b.material]);',
 # one audit gate for observation, pose, body boxes and jobs: the four-weight blend or the one-influence program
 'audited=persistentAudited.emplace(shader,(unsigned char)(NorthlightReplayBounds::SkinEnvelope::supports(p)&&p.paletteBase==31?4:NorthlightPersistentCasters::oneBoneTemplate(p)?1:0)).first;',
 'NorthlightReplayBounds::SkinEnvelope::supports(','unsigned lanes=4;persistentProgram(p.originalShader,&lanes);if(lanes!=1)lanes=4;','found->second.decl==p.decl&&found->second.lanes==lanes',
 'if(lanes==1)weight=nullptr;','ok=NorthlightPersistentCasters::referencedSlots(i,w,lanes,','unsigned lanes=0;const auto* program=persistentProgram(head->originalShader,&lanes);',
 'const auto* program=persistentProgram(p.originalShader);const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;',
 # static screen once per still track, before needsPose; unknown (no scene yet within 15 s, indexing) asks again
 'if(quality.persistentRigidProps)persistentCasters.screen(o.key,o.root,[&]{return (!staticScene||staticScene->map!=lastRequest.map)?(now-persistentMapMs<15000?-1:0):persistentStaticCovered(o);});\n                // Full pose only',
 'rigidScreened=%llu rigidWorn=%llu observedOneBone=%zu unaudited=%zu%s',
):
 assert src.count(needle)==1,needle
assert 'D3DSAMP_MINFILTER,D3DTEXF_LINEAR);d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);' in world # the replay pass
assert 'SetSamplerState(0,D3DSAMP_ADDRESSU,p.addressU)' in state and 'p->cutoff=alpha?(float(ref)+(alphaFunc==D3DCMP_GREATER?.5f:0.f))/255.f:-1.f;' in world
print('PASS renderer wiring: either class enables; rigid props on complete frames, after the static scene, not static-covered, not held; cutouts: <=128 px copy of the replay texture with its sampler/address/cutoff')
# Diagnostics wiring: recording and the near lines only with Diagnostics=1 and PersistentRigidProps=1; the
# diagnostic code reads the registry (explain/diagnosis/tracksNear) and never makes a decision.
diag=src[src.index('    void persistentDiagnostics('):src.index('    // Cache-slot drawing')]
hold=src[src.index('    void persistentDiagHold('):src.index('    NorthlightPersistentCasters::Registry::Hold persistentBodyHeldRule(')]
for needle in ('bool persistentDiag()const{return quality.persistentRigidProps&&NorthlightDiagnostics::enabled();}',
 'persistentCasters.diagnostics(persistentDiag()?PersistentDiagRadius*1.6f:0.f,pivot); /* records near tracks only while on */',
 'if(persistentDiag())try{persistentDiagnostics(now,pivot);}catch(...){persistentWatch.clear();} /* diagnostics never disable the casters */',
 'NorthlightPersistentCasters::Registry::Hold persistentBodyHeld(size_t c){const auto hold=persistentBodyHeldRule(c);if(hold!=NorthlightPersistentCasters::Registry::Free&&persistentDiag())persistentDiagHold(c,hold);return hold;}'):
 assert src.count(needle)==1,needle
assert src.index('persistentCasters.diagnostics(persistentDiag()')<src.index('persistentCasters.screen(o.key,o.root,')<src.index('persistentCasters.frame(obs,now,pivot,')
import re
for body in (diag,hold):
 assert not re.search(r'persistentCasters\.(frame|begin|accept|ready|defer|deferRigid|covered|reject|clear|screen|vertex|heldByBody|attachedToActor|classes|takeEvents|takeRemoved)\(',body)
 assert not re.search(r'\breg\.(frame|begin|accept|ready|defer|deferRigid|covered|reject|clear|screen|vertex|heldByBody)\(',body)
assert 'persistentCasters.noteHold(' in hold and 'noteHold' not in diag
print('PASS diagnostics wiring: gated by Diagnostics and PersistentRigidProps, recording set before screen()/frame(), read-only (explain/diagnosis/tracksNear/noteHold)')
