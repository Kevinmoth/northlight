#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Diagnostic-only GPU changes: exact counters on the current (0.3.124+ LRU, 4096-entry) cache.

The .122 scan-memo differential and the 2048-entry limit checks were superseded in
0.3.124 by test_replay_gpu_maintenance.py (baseline123/reference4096 differential)."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import hashlib,json,subprocess,tempfile
HERE=Path(__file__).resolve().parent
OUT=fp.output_dir()
# The shared native fake-driver/content fixture (also used by test_replay_gpu_maintenance.py).
import replay_gpu_fixture as gpu
stub=gpu.stub
harness=gpu.fixture+gpu.helpers+r'''
static void diagnosticCounters(){
 // Both limits reached together: exclusive attempt reason retains old order.
 {NorthlightReplayGPU::Cache c;Device d;Bindings b;std::vector<Owner> ms;
  for(unsigned i=0;i<17;++i)ms.push_back(sized(256*1024,i));
  c.beginFrame();for(auto& m:ms)assert(!gpuBind(c,d,m,b,false));c.beginFrame();
  for(unsigned i=0;i<16;++i){assert(gpuBind(c,d,ms[i],b,false));b.clear();}
  assert(!gpuBind(c,d,ms[16],b,false));assert(c.uploaded()==4*MiB&&c.stats().uploadDeferred==1&&c.stats().attemptDeferred==1&&c.stats().uploadByteDeferred==0);
  c.beginFrame();assert(c.stats().uploadDeferred==0&&c.stats().attemptDeferred==0&&c.stats().uploadByteDeferred==0);
  assert(gpuBind(c,d,ms[16],b,false));b.clear();
 }
 assert(!alive);
 // Byte ceiling reached before the attempt ceiling; clear accounting survives
 // caller's clear-then-beginFrame fallback and includes probation records.
 {NorthlightReplayGPU::Cache c;Device d;Bindings b;std::vector<Owner> ms={sized(2*MiB),sized(2*MiB),sized(1)};
  c.beginFrame();for(auto& m:ms)assert(!gpuBind(c,d,m,b,false));c.beginFrame();
  assert(gpuBind(c,d,ms[0],b,false));b.clear();assert(gpuBind(c,d,ms[1],b,false));
  assert(!gpuBind(c,d,ms[2],b,false));assert(c.stats().uploadDeferred==1&&c.stats().attemptDeferred==0&&c.stats().uploadByteDeferred==1);
  c.clear();assert(c.clearStats().calls==1&&c.clearStats().entries==3&&c.clearStats().bytes==4*MiB);
  verify(ms[1],b,true);assert(alive==2*MiB);b.clear();c.beginFrame();
  assert(c.clearStats().calls==1&&c.clearStats().entries==3&&c.clearStats().bytes==4*MiB&&c.stats().expiredEntries==0&&c.stats().uploadByteDeferred==0);
  c.clear();assert(c.clearStats().calls==2&&c.clearStats().entries==3&&c.clearStats().bytes==4*MiB);
 }
 assert(!alive);
 // Failed device allocations still consume attempts/bytes exactly as before.
 {NorthlightReplayGPU::Cache c;Device d;Bindings b;auto m=sized(256*1024);c.beginFrame();assert(!gpuBind(c,d,m,b,false));c.beginFrame();
  for(unsigned i=0;i<16;++i){d.failAt=d.calls+1;assert(!gpuBind(c,d,m,b,false));}
  assert(d.calls==16&&!c.uploaded()&&!alive);assert(!gpuBind(c,d,m,b,false));
  assert(c.stats().attemptDeferred==1&&c.stats().uploadByteDeferred==0&&c.stats().uploadDeferred==1&&d.calls==16);
 }
 // beginFrame purges weak owners after counters reset: resident and probation
 // removals count separately from ordinary room() capacity eviction.
 {NorthlightReplayGPU::Cache c;Device d;Bindings b;auto resident=sized(64),probation=sized(128);c.beginFrame();
  assert(!gpuBind(c,d,resident,b));assert(!gpuBind(c,d,probation,b));c.beginFrame();assert(gpuBind(c,d,resident,b));
  std::weak_ptr<const Mesh> first=resident,second=probation;resident.reset();probation.reset();assert(first.expired()&&second.expired());
  c.beginFrame();assert(c.stats().expiredEntries==2&&c.stats().expiredBytes==64&&c.stats().evictions==0&&c.population().entries==0&&c.bytes()==0);
  assert(c.clearStats().calls==0&&alive==64);b.clear();assert(!alive);c.beginFrame();assert(c.stats().expiredEntries==0&&c.stats().expiredBytes==0);
 }
 // Ordinary capacity eviction does not inflate weak-expiry accounting.
 {NorthlightReplayGPU::Cache c;Device d;Bindings b;std::vector<Owner> ms;for(unsigned i=0;i<4097;++i)ms.push_back(sized(4,i));
  c.beginFrame();for(unsigned i=0;i<4096;++i)assert(!gpuBind(c,d,ms[i],b));c.beginFrame();assert(!gpuBind(c,d,ms[4096],b));
  assert(c.stats().evictions==1&&c.stats().expiredEntries==0&&c.stats().expiredBytes==0);
 }
 assert(!alive);
}
int main(){NorthlightReplayGPU::createBudgetMs()=1e30; /* exact 16-attempt/4 MiB limits; the create time budget is not under test here */
 assert(NorthlightReplayGPU::Cache::entryLimit()==4096);diagnosticCounters();
 std::printf("PASS GPU diagnostics: exclusive attempt/byte deferrals including simultaneous ceilings and allocation failures; reset; resident+probation weak expiry and payload accounting; ordinary eviction separation at 4096 entries; cumulative clear accounting with held output refs.\n");
}
'''
source=fp.src('replay_gpu_cache.h').read_text()
files=[fp.tracked(name) for name in ['replay_gpu_cache.h','draw_snapshot.h','capture_buffer_metadata.h','test_replay_gpu_diagnostics.py','replay_gpu_fixture.py','test_replay_gpu_cache.py','test_terrain_snapshot.py']]
report={'command':'python3 tests/test_replay_gpu_diagnostics.py','scope':'Actual production GPU diagnostics counters, fake D3D only; no game, Wine or real driver run.','source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},'counter_contract':{'attemptDeferred':'Exclusive 16-attempt guard; wins when both quotas block.','uploadByteDeferred':'Exclusive attempted-upload-byte guard when fewer than16 attempts. Sum with attemptDeferred equals existing uploadDeferred.','expiredEntries':'Weak-owner removals during beginFrame, resident plus zero-byte probation, counted after reset.','expiredBytes':'Resident GPU payload bytes removed by those expirations; COM output refs may retain physical allocations.','clearStats':'Lifetime calls/entries/payload bytes removed by clear(); not reset by beginFrame or clear; caller knows reason. Empty clears increment calls only.'},'runs':[]}
with tempfile.TemporaryDirectory(prefix='gpu-diagnostics-') as tmp:
 root=Path(tmp);(root/'d3d9.h').write_text(stub);(root/'test.cpp').write_text(harness)
 for flags in [['-O2'],['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']]:
  subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-I'+str(root),*fp.test_include_flags(),str(root/'test.cpp'),'-o',str(root/'test')],check=True)
  result=subprocess.run([str(root/'test')],text=True,capture_output=True);print(result.stdout,result.stderr,flush=True);result.check_returncode()
  report['runs'].append({'flags':flags,'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
(OUT/'gpu-diagnostics-validation.json').write_text(json.dumps(report,indent=2)+'\n')
