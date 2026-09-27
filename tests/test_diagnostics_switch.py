#!/usr/bin/env python3
# northlight-test:
"""Diagnostics=0 source audit (0.3.141). Every production logf() is either behind a
diagnostics gate (sampled()/diagnostics()/NorthlightDiagnostics::enabled()/captureSampled,
itself false when Diagnostics=0) or in the labelled allow-list below (start-up,
settings, errors/warnings, capped first-N rejects, one-off events, user-triggered,
or reachable only from a gated caller). A new ungated periodic line fails this test.
Also checks the non-log diagnostic work is gated and the functional mirror audit is
not. Prints the full inventory. Static source analysis only; nothing is run."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import re,sys
HERE=Path(__file__).resolve().parent
FILES=['renderer.cpp','world_renderer.h','world_shadow_experiment.inl','world_point_rendering.inl','celestial_disc_renderer.h',
       'shadow_blob_filter.h','water_renderer.h','gpu_profile.h','world_diagnostics.h','world_persistent_casters.inl','world_replay_probe.inl']
# 0.3.149: profiling()/profileSampled() = RenderProfile, which requires Diagnostics=1 (NorthlightQuality::renderProfile).
GATES=('NorthlightDiagnostics::enabled()','diagnostics()','sampled()','captureSampled','if(diagnostics)','shadowFate.active()','sampledFrame','profiling()','profileSampled()')
# Ungated lines that stay with Diagnostics=0: format prefix -> label.
KEEP={
 'LOGGER intervalMs':'indirect: reportLogCost() runs only in the gated MIRROR block',
 'EXTENSION fault':'error','DISABLED:':'error','Resources ':'one-off: resource (re)creation',
 'VIEWPORT GATE':'capped: first 8','WORLD skipped frame':'capped: first 8 (periodic tail gated)',
 'FIRST EFFECT FRAME':'one-off','MIRROR mismatch':'error (the audit itself is functional and ungated)',
 'WORLD non-caster draw rejected':'capped: first 4 (periodic tail gated)','Projection rejected':'capped: first',
 'D3D9 device wrapped':'start-up','MEMORY async sampler':'error','MEMORY guard':'warning: low address space (pressure/trim/after-trim/recovery, cooldown-limited)',
 'LOG previous session':'start-up: previous log rotation result','DEVICE lifetime':'one-off: device create/destroy',
 'Reset HRESULT':'one-off: Reset','Effects components':'user-triggered: Ctrl+Shift+F7..F9','Effects %s':'user-triggered: Ctrl+Shift+F10',
 'World debug':'user-triggered: Ctrl+Shift+F12','MEMORY frame':'gated: diagnostics() in the same condition','MEMORY sample failed':'gated: diagnostics() in the same condition',
 'Effects retry':'user-triggered','MIRROR fallback':'one-off error','SHADOWBLOB draw signature':'capped: first 4',
 'Backend capabilities':'start-up','UNSUPPORTED BACKEND':'error','CreateDevice HRESULT':'start-up','DXVK compatibility':'start-up',
 'Northlight renderer':'start-up version line','Direct3D9Ex requested':'start-up',
 'BACKEND candidate':'start-up: one line per backend load attempt','BACKEND selected':'start-up: loaded backend','BACKEND SELF-LOAD REFUSED':'start-up error',
 'BACKEND RECURSION':'one-off: first re-entered export','HOST exe':'start-up: wow.exe identity','PROXY module':'start-up: proxy location',
 'PROXY WARNING':'start-up warning','GAME d3d9.dll':'start-up: game-folder d3d9.dll identity','GAME WARNING':'start-up warning',
 'WORLD shadow cache VERIFY MISMATCH':'error (debug verify)','GEOMETRY MEMORY':'warning: allocation deferral',
 'STATIC SHADOW request deferred':'warning: allocation failure','STATIC SHADOW upload deferred':'warning: allocation failure',
 'QUALITY':'settings','WORLD replacement deferred':'warning','WORLD pending mesh released':'event: orphaned staged upload released (0.3.156), at most once per geometry snapshot','WORLD geometry stalled':'warning: once per generation-admission stall episode (0.3.156 watchdog)','WORLD terrain allocation requestMiB':'warning: allocation deferred',
 'GI actor BVH rejected':'warning','WORLD DISABLED':'error','WORLD streaming retry':'capped: first 12','SHADOW experiment':'settings / error',
 'CELESTIAL profiles loaded':'start-up settings','SHADOW regional terrain loaded':'start-up settings','WORLD explicit recovery':'user-triggered',
 'WORLD worker stopped':'error','WORLD context validated':'one-off','WORLD cache: %s':'worker error message',
 'TERRAIN SHADOW patched':'capped: first 4','TERRAIN UP snapshot rejected':'capped: first 12','TERRAIN snapshot rejected':'capped: first 12',
 'TERRAIN projection':'capped: first 12','STATIC SHADOW draw retry':'capped: first 8','MODEL snapshot rejected':'capped: first 12',
 'WORLD GPU diagnostic':'user-triggered GPU capture (F12 debug)','WORLD slow submission':'capped: first 12','POINT pass skipped':'capped: first 12',
 'PERSISTENT casters disabled':'error: allocation failure','PERSISTENT caster draw failed':'error',
 'PERSISTENT near':'gated: only in persistentDiagnostics(), called only under persistentDiag() (PersistentRigidProps && NorthlightDiagnostics::enabled(); audited below)',
 'CELESTIAL disabled':'error','CELESTIAL native texture identity':'error','CELESTIAL early draw skipped':'capped: first 4',
 'SHADOWBLOB candidate':'capped: first 4','SHADOWBLOB identified':'capped: first 8','WATER disabled':'error','WATER explicit recovery':'user-triggered',
 'WATER registered':'one-off: shader registration','WATER mask patch skipped':'capped: first 8 (patch rejected or patched hash mismatch; that shader only)','GPU profile':'gated: no sample opens when off (beginFrame/poll gated)','%s':'gpu_profile report: gated as above',
}
def conditions(s,pos):
    out=[];j=max(s.rfind(';',0,pos),s.rfind('{',0,pos),s.rfind('}',0,pos));out.append(s[j+1:pos])
    depth=0;k=pos
    while k>0:
        k-=1;c=s[k]
        if c=='}':depth+=1
        elif c=='{':
            if depth==0:j=max(s.rfind(';',0,k),s.rfind('{',0,k),s.rfind('}',0,k));out.append(s[j+1:k])
            else:depth-=1
    return out
gated,kept,unknown=[],[],[]
for f in FILES:
    s=fp.src(f).read_text()
    for m in re.finditer(r'\blogf\(',s):
        if re.search(r'(void|Include after the renderer\'s|Include after)\s*$',s[max(0,m.start()-40):m.start()]):continue
        fmt=re.match(r'logf\("([^"]{0,60})',s[m.start():]);fmt=fmt.group(1) if fmt else s[m.start():m.start()+30].replace('\n',' ')
        if fmt.startswith('const char*'):continue # comment text
        where=f'{f}:{s.count(chr(10),0,m.start())+1}'
        if any(g in c for c in conditions(s,m.start())[:6] for g in GATES):gated.append((where,fmt));continue
        label=next((v for k,v in KEEP.items() if fmt.startswith(k)),None)
        (kept if label else unknown).append((where,fmt,label))
print(f'GATED (off with Diagnostics=0): {len(gated)} log sites');[print('  LOG ',w,f) for w,f in gated]
print(f'KEPT with Diagnostics=0: {len(kept)}');[print('  ',w,f,'--',l) for w,f,l in kept]
assert not unknown,'ungated, unlabelled log sites:\n'+'\n'.join(f'{w} {f}' for w,f,_ in unknown)
# Non-log diagnostic work (MEASUREMENT / COUNTER) must be gated; functional work must not be.
r=fp.src('renderer.cpp').read_text();w=fp.src('world_renderer.h').read_text()
checks={
 'CpuScope timings use sampled()':all('CpuScope' not in l or 'sampled' in l or 'diagnostics()' in l for l in r.splitlines() if 'CpuScope ' in l and '(' in l and 'struct' not in l),
 'world capture: raw sample frame in, diagnostics gated inside':r.count(',vs,frame%120==0,NorthlightRenderThreadProbe::sampleFrame(frame));captureWater(')==4 and w.count('constantSelfCheck=selfCheck;sample=sample&&NorthlightDiagnostics::enabled();')==2,
 'constant self-check keeps its 0.3.140 cadence':'constantSelfCheck?&constantEpochStats:nullptr' in w,
 'GPU timestamp queries only when on':'if(diagnostics())gpuProfile->beginFrame(frame,NorthlightRenderThreadProbe::sampleFrame(frame))' in r and 'if(diagnostics())gpuProfile->poll();' in r,
 'async memory sampler always on (memory guard), periodic line only when on':'try{memoryDiagnostics=std::make_unique<NorthlightMemoryDiagnostics::Sampler>(&queryAddressSpace);}' in r and 'if(diagnostics())try{memoryDiagnostics=' not in r and 'if(decision.report&&diagnostics())logf("MEMORY frame=' in r and 'else if(diagnostics())logf("MEMORY sample failed' in r,
 'frame interval sampling only when on':'if(diagnostics()&&QueryPerformanceCounter(&intervalTick)&&frameIntervals.sample(' in r,
 'mirror audit stays functional (ungated)':'mirrorAuditSchedule.afterWorldCapture(frame,' in r and not re.search(r'diagnostics\(\)[^;]*mirrorAuditSchedule',r),
 'Diagnostics read once at quality load':'NorthlightDiagnostics::configure(quality.diagnostics!=0);' in w,
 'fate tracker: Diagnostics=0 wins':'shadowFateDiagnostics=NorthlightQuality::shadowFate(quality);' in w,
 'streaming phase clocks gated':'const bool on=NorthlightDiagnostics::enabled();' in fp.src('streaming_phase_profile.h').read_text(),
 'persistent near diagnostics: one gated caller, registry recording off unless gated':(lambda t:t.count('persistentDiagnostics(')==2 and 'if(persistentDiag())try{persistentDiagnostics(now,pivot);}catch(...){persistentWatch.clear();}' in t
   and 'bool persistentDiag()const{return quality.persistentRigidProps&&NorthlightDiagnostics::enabled();}' in t
   and all(t.index('void persistentDiagnostics(')<m.start()<t.index('// Cache-slot drawing') for m in re.finditer(r'logf\("PERSISTENT near',t))
   and 'persistentCasters.diagnostics(persistentDiag()?PersistentDiagRadius*1.6f:0.f,pivot);' in t and 'if(hold!=NorthlightPersistentCasters::Registry::Free&&persistentDiag())persistentDiagHold(c,hold);' in t)(fp.src('world_persistent_casters.inl').read_text()),
 'RenderProfile needs Diagnostics (its log gates count as diagnostics gates)':'inline bool renderProfile(const Settings& s){return s.diagnostics&&s.renderProfile;}' in fp.src('quality_settings.h').read_text(),
 'point/envelope diagnostics gated':'const bool diagnostics=NorthlightDiagnostics::enabled()&&(frames==0||frames%120==0);' in fp.src('world_point_rendering.inl').read_text(),
}
for name,ok in checks.items():print(('PASS ' if ok else 'FAIL ')+name)
assert all(checks.values())
print('PASS Diagnostics=0 audit: every periodic line gated, only start-up/settings/error/capped/event/user-triggered lines remain')
