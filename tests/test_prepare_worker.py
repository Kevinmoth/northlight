#!/usr/bin/env python3
# northlight-test: requires=cxx,client,stormlib
"""0.3.177 (r83 a1-prepare): the actor prepare worker. Native test of prepare_worker.h
(test_prepare_worker.cpp: handover at every index, the threaded worker and join with random delays and
stops, exception and watchdog takeover, cache independence, and the counterfactuals: state reset at the
handover, unfiltered records, the live declaration cache after an eviction, the camera at the join),
built O2, ASan+UBSan and TSan; and a wiring audit of the renderer side: the capture fill and publish, the
join first in the selection, quiesce before recycling, the deferred cache clear, one prepare function, the
frozen camera, the enable rule, the quarantine of an abandoned worker's replays, RenderProfile-only
diagnostics. No game or GPU."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import client_fixtures  # the real client programs, from the tester's client
import ast,re,subprocess,tempfile
HERE=Path(__file__).resolve().parent
def literal(path,name):
    return next(ast.literal_eval(n.value) for n in ast.parse(path.read_text()).body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets))
stub=literal(HERE/'test_terrain_snapshot.py','stub')
deformation=literal(HERE/'test_actor_deformation.py','harness')
code=deformation[deformation.index('std::vector<Word> code(unsigned major){'):deformation.index('int main(')]
worker=re.sub(r'/\*.*?\*/','',re.sub(r'//[^\n]*','',fp.src('prepare_worker.h').read_text()),flags=re.S)
checks={
 'the worker header touches no renderer state (actorPrograms, declarationCache, replays, the device)':
    not any(n in worker for n in ('actorPrograms','declarationCache','replays','d->','IDirect3DDevice9','context.')),
}
w=fp.src('world_renderer.h').read_text();x=fp.src('world_shadow_experiment.inl').read_text();g=fp.src('world_memory_guard.inl').read_text()
def body(text,head):
    start=text.index(head);depth=0;i=text.index('{',start)
    while True:
        depth+={'{':1,'}':-1}.get(text[i],0)
        if depth==0:return text[start:i+1]
        i+=1
capture=body(w,'    void captureModel(D3DPRIMITIVETYPE type,')
select=body(x,'    void selectShadowReplays(){')
end=body(w,'    void endFrame(bool retainPool=true){')
release=w[w.index('    void releaseGPU(){'):];release=release[:release.index('\n')]
reset=w[w.index('    void reset(){'):];reset=reset[:reset.index('\n')]
trim=body(g,'    MemoryTrim trimMemory(){')
register=body(w,'    void registerShader(IDirect3DVertexShader9* shader,uint64_t hash){')
checks.update({
 'capture: the inputs filled just before a selected skinned draw joins replays, published right after':
    'if(handoff)prepareFill(*p,metadata); /* 0.3.177: the stable selection\'s inputs */\n        replays.emplace_back(p.release());preparePublish();' in capture,
 'the join is the first statement of selectShadowReplays':select.split('{',1)[1].lstrip().startswith('prepareJoin();'),
 'quiesce before recycling: endFrame, reset (endFrame, releaseGPU), releaseGPU, trimMemory':
    end.index('prepareQuiesce();prepareEndFrame();')<end.index('recycleReplay(')
    and release.index('prepareQuiesce();')<release.index('prepareCaches->sampled.clear()')<release.index('releaseReplayGPU();') and reset.index('endFrame();')<reset.index('releaseGPU();')
    and trim.index('prepareQuiesce();')<trim.index('freeReplays.clear()'),
 'registerShader defers the cache clear (the owner clears at its next open)':
    'prepareCachesStale=true;' in register and 'prepareCaches->sampled.clear()' not in register and 'prepareCaches->bones.clear()' not in register,
 'the caches are cleared by their owner only: at open (stale), releaseGPU after quiesce':
    w.count('prepareCaches->sampled.clear()')==1 and x.count('prepareCaches->sampled.clear()')==1
    and 'if(prepareCachesStale.exchange(false)){prepareCaches->sampled.clear();prepareCaches->bones.clear();' in x,
 'one prepare function: the worker (Prepare), the join tail, the self-check and inline all call prepareRecord':
    fp.src('prepare_worker.h').read_text().count('prepareRecord(s,p,index,c,f.inverseView,f.camera,out);')==1
    and x.count('NorthlightActorPrepare::prepareRecord(')==3 and 'NorthlightActorPrepare::prepareRecord(state,p,index,*prepareCaches,context.inverseView,context.camera,out);prepareConsume(out);' in x,
 'the stable loop body exists only in prepareRecord (the legacy loop has no stable identity branch)':
    'if(tuning.stableIdentity){' not in x and 'if(tuning.stableIdentity)prepareStable(distanceTests,distanceReused);' in x,
 'the worker frame copies the camera at open, and the join tail uses that copy':
    'std::memcpy(frame.inverseView,context.inverseView,sizeof frame.inverseView);std::memcpy(frame.camera,context.camera,sizeof frame.camera);' in x
    and 'prepareWorker.index(k),*prepareCaches,frame.inverseView,frame.camera,outputs[k]);' in x,
 'offload only with 6+ cores, ActorShadows=1, the stable tuning and no watchdog; RenderProfile A/B windows':
    'bool prepareOffload()const{return prepareCores>=6&&quality.actorShadows&&selectionTuning().stableIdentity&&!prepareWorker.abandoned();}' in x
    and 'const bool abInline=NorthlightRenderThreadProbe::profiling()&&(GetTickCount()/10000u)%2u==1u;' in x,
 'an abandoned worker: its replays quarantined (recycle, resize, clear) until it settles':
    'if(prepareUnsettled()){try{prepareQuarantine.emplace_back(raw);}catch(...){} return;}' in w
    and 'if(prepareUnsettled())for(size_t i=fit;i<replays.size();++i)recycleReplay(replays[i].release());' in w
    and 'if(prepareUnsettled()){for(auto& p:replays)recycleReplay(p.release());for(auto& p:heldShadowReplays)recycleReplay(p.release());}' in w,
 'watchdog re-arm: only in endFrame, after the settled worker released its quarantine and caches; 10 s after the abandon, at most 4 a session; counted in the log and the fields':
    (lambda e:e.index('prepareAbandonedCaches.reset();')<e.index('prepareWorker.rearm()'))(body(x,'    void prepareEndFrame(){'))
    and 'if(prepareRearms<PrepareRearms&&GetTickCount()-prepareAbandonTick>=PrepareRearmAfterMs&&prepareWorker.rearm()){' in x
    and 'static constexpr unsigned PrepareRearms=4;static constexpr DWORD PrepareRearmAfterMs=10000;' in x and x.count('prepareWorker.rearm()')==1
    and '(re-arms %u of %u)' in x and 'prepareRearms=%u' in x,
 'T1 (0.3.179): prepareFill takes the capture metadata\'s program: no actorPrograms lookup, no shared_ptr copy':
    (lambda f:'actorPrograms' not in f and 'std::shared_ptr' not in f and 'p.program=metadata.program.get();' in f)(body(x,'    void prepareFill(Replay& p,const CaptureShader& metadata){'))
    and 'const NorthlightActorDeformation::Program* program=nullptr;' in w and 'metadata.program=std::make_shared<const NorthlightActorDeformation::Program>(std::move(program));\n                actorPrograms.emplace(shader,metadata.program);' in w,
 'T1: registerShader retires erased programs; they are released only after the recycle, settled, with no quarantine':
    'retireProgram(std::move(old->second.program));' in register and 'retireProgram(std::move(program->second));actorPrograms.erase(program);' in register
    and w.count('retiredPrograms.clear()')+x.count('retiredPrograms.clear()')==1
    and (lambda f:f.index('if(prepareUnsettled()||!prepareQuarantine.empty()){')<f.index('return;}')<f.index('retiredPrograms.clear();prepareQuarantinedDecls.clear();'))(body(x,'    void prepareFrameRelease(){'))
    and end.index('recycleReplay(')<end.index('prepareFrameRelease(); /* 0.3.179: after the recycle */')
    and 'if(replays.empty()&&heldShadowReplays.empty())prepareFrameRelease();' in release,
 'T2 (0.3.179): prepareFill reads the declaration through the frame arena (the per-record copy when full); the arena is reset only in prepareFrameRelease, swapped when unsettled':
    'prepareDecls->find(p.decl,' in x and 'if(copy){p.declCopy=copy;p.declared=copy->declared;}' in x
    and x.count('prepareDecls->reset()')==1 and 'prepareQuarantinedDecls.push_back(std::move(prepareDecls));' in body(x,'    void prepareFrameRelease(){')
    and 'const NorthlightActorPrepare::DeclCopy* declCopy=nullptr;' in w and 'program=nullptr;declCopy=nullptr;' in w
    and w.index('std::unique_ptr<NorthlightActorPrepare::DeclArena> prepareDecls=')<w.index('NorthlightActorPrepare::Worker<Replay> prepareWorker;'),
 'T3 (0.3.179): a release publish; the seq_cst fence only at the wake threshold, and before the worker sleeps':
    (lambda h:'slots_[n]={record,index};published_.store(n+1,std::memory_order_release);\n        if(n+1-done_.load(std::memory_order_relaxed)>=wakeBatch_){std::atomic_thread_fence(std::memory_order_seq_cst);if(wakeable_.exchange(false))notify();}' in h
        and 'blocked_=true;wakeable_.store(true,std::memory_order_relaxed);std::atomic_thread_fence(std::memory_order_seq_cst);\n            wake_.wait(lock,' in h
        and h.count('std::atomic_thread_fence(std::memory_order_seq_cst)')==2 and 'published_.store(n+1,std::memory_order_seq_cst)' not in h)(fp.src('prepare_worker.h').read_text()),
 'M1/M2 (0.3.179): one span per sampled record around fill and publish in captureModel; the meter and the warm re-run gated by profileSampled()':
    'const bool handoff=p->shadowSelected&&p->shadowSkinned,timedHandoff=handoff&&prepareHandoffSample();' in capture
    and capture.index('prepareMeter.start()')<capture.index('prepareFill(*p,metadata);')<capture.index('replays.emplace_back(p.release());preparePublish();if(timedHandoff)prepareMeter.stop(handoffStart);')
    and 'prepareMeter.beginFrame(profileSampled(),prepareFrameSerial);' in x and 'if(profileSampled()&&r.published)prepareWarmHandoff(r.published);' in x
    and 'steady_clock' not in body(x,'    void prepareFill(Replay& p,const CaptureShader& metadata){') and 'steady_clock' not in body(x,'    void preparePublish(){')
    and 'handoffUs=%.1f handoffRawUs=%.1f pairNs=%.1f handoffSamples=%u handoffWarmUs=%.1f' in x,
 'diagnostics only with RenderProfile (the fields and every clock)':
    'if(NorthlightRenderThreadProbe::profiling())std::snprintf(prepareFields,' in x and 'prepareTimed=profileSampled();' in x,
 'the worker is destroyed (joined) before the replays, caches and quarantine it reads':
    w.index('std::vector<std::unique_ptr<Replay>> replays,')<w.index('std::unique_ptr<NorthlightActorPrepare::Caches> prepareCaches=')<w.index('std::vector<std::unique_ptr<Replay>> prepareQuarantine;')<w.index('NorthlightActorPrepare::Worker<Replay> prepareWorker;'),
})
for name,ok in checks.items():print(('PASS ' if ok else 'FAIL ')+name)
assert all(checks.values())
with tempfile.TemporaryDirectory(prefix='northlight-prepare-worker-') as tmp:
    p=Path(tmp);(p/'d3d9.h').write_text(stub);client_fixtures.actor_client_programs(p)
    xs=fp.src('world_shadow_experiment.inl').read_text()
    block=xs[xs.index('    // ---- 0.3.177 (r83 a1-prepare)'):xs.index('    // Stable per-actor quota (see actor_shadow_selection.h).')]
    wr=fp.src('world_renderer.h').read_text();retire=body(wr,'    void retireProgram(std::shared_ptr<const NorthlightActorDeformation::Program>&& program){')
    (p/'test_prepare_worker.cpp').write_text((HERE/'test_prepare_worker.cpp').read_text().replace('/*SYNTHETIC_PROGRAMS*/',code).replace('/*PREPARE_BLOCK*/',block).replace('/*RETIRE_PROGRAM*/',retire))
    four=str(client_fixtures.four_bone_vs3())
    for label,flags,mode in (('O2',['-O2'],'full'),('asan',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'],'full'),('tsan',['-O1','-g','-fsanitize=thread'],'tsan')):
        exe=p/('test-'+label)
        subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-UNDEBUG',*flags,'-I',str(p),*fp.test_include_flags(),str(p/'test_prepare_worker.cpp'),str(fp.src('world_gi.cpp')),'-o',str(exe)],check=True)
        out=subprocess.run([str(exe),four,mode],capture_output=True,text=True)
        if out.returncode or 'WARNING' in out.stderr or 'ERROR' in out.stderr:
            sys.exit(f'{label} failed ({out.returncode}):\n{out.stdout}{out.stderr[-6000:]}')
        print(f'[{label}]',out.stdout.strip(),flush=True)
        if label=='asan': # 0.3.179 (T1) counterfactual: a program freed at the re-register is read after it
            bad=subprocess.run([str(exe),four,'lifetime-counterfactual'],capture_output=True,text=True)
            assert bad.returncode!=0 and 'heap-use-after-free' in bad.stderr,('the freed-program counterfactual must fail under ASan',bad.returncode,bad.stderr[-2000:])
            print('[asan] counterfactual: a program freed at the re-register: heap-use-after-free, as expected',flush=True)
print('PASS prepare worker: model, threads and counterfactuals')
