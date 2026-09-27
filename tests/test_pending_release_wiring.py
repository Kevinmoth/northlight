#!/usr/bin/env python3
# northlight-test:
"""0.3.156 orphaned staged-upload release and generation-stall watchdog: source wiring.
A staged upload (pendingMesh) whose snapshot is no longer active never commits but keeps
a geometry generation alive; with an out-of-range published build that filled
Generations<BVH,2> and the builder deferred for ever (0.3.155 game log). The release rule
must run where upload() cannot: before render()'s ready() gate and at the out-of-range
active.reset() site, both on the D3D thread. The watchdog only logs. Writes nothing.
The behaviour itself runs in test_concurrent_geometry_build.py case (d)."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import re
from pathlib import Path
HERE=Path(__file__).resolve().parent
w=fp.src('world_renderer.h').read_text();r=fp.src('renderer.cpp').read_text();g=fp.src('geometry_memory.h').read_text()
t=(HERE/'test_concurrent_geometry_build.py').read_text()
def method(signature,text=w):
    start=text.index(signature);end=text.index('\n    }\n',start);return text[start:end]
render=method('    bool render(IDirect3DSurface9* targetSurface');upload=method('    bool upload(NorthlightStreaming::Budget& streamBudget){')
context=method('    void updateWorldContext(const char* map,const float* camera,const NorthlightWmoContext::Lighting* global=nullptr){');helper=method('    void releaseOrphanedPending(const char* site){')
gate='if(!ready()||!resources(w,h,fmt)||!upload(streamBudget)){'
reset='active.reset();}'
admission='if(!generations.canAdmit()){++generationDeferrals;'
checks={
 'one release rule: the upload() predicate as a static helper, shared by the fixture':
  w.count('static bool pendingOrphaned(')==1 and 'return pending&&(!active||pending->map!=active->map||pending->bvh!=active->bvh);' in w
  and 'pendingMesh->map!=active->map||pendingMesh->bvh!=active->bvh' not in w and 'Fixture::pendingOrphaned(' in t and "static bool pendingOrphaned('" in t,
 'helper releases exactly as upload() did, with one log line per event':
  'if(!pendingOrphaned(pendingMesh.get(),active.get()))return;' in helper and 'retirePendingCpu();pendingMesh.reset();meshRetry.clear();' in helper
  and helper.count('logf("WORLD pending mesh released orphan=')==1,
 'upload() uses the helper before staging':upload.count('releaseOrphanedPending("upload");')==1 and upload.index('releaseOrphanedPending("upload");')<upload.index('if(uploaded.lock()!=active->bvh&&!pendingMesh'),
 'render(): release before the ready() gate':render.count('releaseOrphanedPending("render");')==1 and render.count(gate)==1 and render.index('releaseOrphanedPending("render");')<render.index(gate),
 'updateWorldContext(): release right after the out-of-range active.reset()':
  context.count(reset)==1 and context.count('releaseOrphanedPending("range");')==1 and context.index(reset)<context.index('releaseOrphanedPending("range");')<context.index('DWORD now=GetTickCount();'),
 'no other pendingMesh release path changed':w.count('retirePendingCpu();pendingMesh.reset();')==3,
 'updateWorldContext runs only from the draw-hook context readers (D3D thread, like render())':
  w.count('updateWorldContext(map,')==2 and 'updateWorldContext(map,camera.camera,globalRead?&light:nullptr);' in method('    bool wmoContext(IDirect3DVertexShader9* shader){') and 'updateWorldContext(map,camera,globalRead?&global:nullptr);' in method('    void terrainContext(){')
  and 'world->wmoContext(vs)' in r and 'world->terrainContext();' in r,
 'builder marks each generation deferral and clears on admission, under the mutex':
  w.count(admission)==1 and re.search(r'if\(!generations\.canAdmit\(\)\)\{\+\+generationDeferrals;\{std::lock_guard<std::mutex> lock\(mutex\);generationStall\.deferred\(GetTickCount\(\)\);\}deferBuild\("generation-deferred",memory,100\);continue;\}\n\s*\{std::lock_guard<std::mutex> lock\(mutex\);generationStall\.admitted\(\);\}',w) is not None,
 'fixture region mirrors the production admission marks':'generationStall.deferred(GetTickCount());}deferBuild("generation-deferred"' in t and 'generationStall.admitted();}' in t,
 'watchdog: render thread, inside the request mutex block, once per episode, log only':
  context.count('generationStall.due(GetTickCount(),GenerationStallLogMs)')==1 and context.count('logf("WORLD geometry stalled ')==1
  and context.index('{std::lock_guard<std::mutex> lock(mutex);')<context.index('generationStall.due(')<context.index('{auto phase=streamingPhases.measure(NorthlightStreaming::PhaseProfile::Retire);')
  and re.search(r'\bpublished(=[^=]|\.reset\(|\s*=\s*nullptr)',re.sub(r'"[^"]*"','""',context[context.index('generationStall.due('):context.index('{auto phase=streamingPhases.measure(NorthlightStreaming::PhaseProfile::Retire);')])) is None
  and 'GenerationStallLogMs=10000' in w,
 'StallWatch is portable and episode-based':'struct StallWatch {' in g and 'void deferred(uint32_t now){if(!armed){armed=true;since=now;}}' in g and 'void admitted(){armed=false;reported=false;}' in g,
 'generation limit unchanged (F3)':'template<class T,size_t Limit=2> class Generations {' in g and 'NorthlightGeometryMemory::Generations<NorthlightGI::BVH> generations;' in w,
}
for k,v in checks.items():print(('PASS ' if v else 'FAIL ')+k)
assert all(checks.values())
print('PASS pending release wiring: one rule before the ready() gate, at the out-of-range site and in upload(); log-only stall watchdog')
