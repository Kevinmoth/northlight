#!/usr/bin/env python3
# northlight-test: requires=cxx,client,stormlib slow
"""0.3.141 persistent casters: transform accuracy vs the replay VS emulation (real
client four-bone SM3 shader), conversion gates, registry lifecycle, memory cap,
cache-slot invalidation, worker thread (native clang++, plain, ASan/UBSan, TSan); the
0.3.150 track order and whole registry == 0.3.149 (general class); renderer wiring.
PersistentRigidProps (the rigid class and its PERSISTENT near diagnostics) was retired in
0.3.172: rigid props are remembered by rigid_memory.h (test_rigid_memory)."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import client_fixtures  # the real client four-bone program, from the tester's client
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
 for label,flags in (('O2',['-O2']),('asan',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']),('tsan',['-O1','-g','-fsanitize=thread'])):
  exe=p/('test-'+label)
  subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-UNDEBUG','-I',str(p),*fp.test_include_flags(),str(HERE/'test_persistent_casters.cpp'),str(fp.src('world_gi.cpp')),'-o',str(exe)],check=True)
  print(f'[{label}]',flush=True);subprocess.run([str(exe),str(client_fixtures.four_bone_vs3())],check=True)
# Renderer wiring after the rigid class's retirement (source audit: the .inl needs the device).
src=fp.src('world_persistent_casters.inl').read_text();header=fp.src('persistent_casters.h').read_text()
for needle in ('bool persistentEnabled()const{return NorthlightPersistentCasters::Enabled&&quality.persistentCasters&&!persistentDisabled;}',
 'persistentCasters.classes(quality.persistentCasters!=0);','void classes(bool general){',
 # the general class's attachment rule still asks whether a group is rigid (one palette bone)
 'if(persistentCasters.attachedToActor(obs,c,[&](size_t n){return persistentIsRigid(n);})){persistentCasters.defer(o,now,false);continue;}',
 # one audit gate for observation, pose and jobs: the four-weight blend or the one-influence program
 'audited=persistentAudited.emplace(shader,(unsigned char)(NorthlightReplayBounds::SkinEnvelope::supports(p)&&p.paletteBase==31?4:NorthlightPersistentCasters::oneBoneTemplate(p)?1:0)).first;',
 'if(lanes==1)weight=nullptr;','ok=NorthlightPersistentCasters::referencedSlots(i,w,lanes,',
 # opaque only (v1)
 'if(p.cutoff>=0&&!NorthlightPersistentCasters::AlphaCutouts){alpha=true;return false;}'):
 assert (src+header).count(needle)==1,needle
import re
for gone in ('persistentRigidProps','rigidProps','persistentDiag','PERSISTENT near','heldByBody','deferRigid','screen(','staticPlacement','PlacementIndex','persistentIndex','explain(','Teleport','centreInView','envelope('):
 assert gone not in src and gone not in header,gone
print('PASS renderer wiring: PersistentCasters alone enables the class; the attachment rule, audited programs and opaque-only conversion unchanged; no rigid class or near diagnostics left')
