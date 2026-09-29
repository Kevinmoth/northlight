#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native tests of actual production mirror/SavedState methods against a mock device."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
HERE=Path(__file__).resolve().parent
OUT=fp.output_dir()
FILES=['extension_raw_methods.inl','mirror_guard.h','generate_extension_raw_methods.py','forwarders.h','device_mirror.h','captured_constant_epoch.h','mirror_audit_schedule.h','saved_state.h','test_device_mirror.cpp','test_device_mirror_api.h','test_device_mirror.py',
       'mirror_guarded_device.h','generate_mirror_guarded_device.py','mirror_resources.h','mirror_resource_forwarders.h','tracked_buffers.h','renderer.cpp']
report={'command':'python3 tests/test_device_mirror.py','scope':'Actual production device_mirror.h and saved_state.h with a native independent mock backend and minimal API declarations. Not a real D3D driver, Wine, or game execution. Native timings are informational; expected backend query elimination is the measured structural result.','source_sha256':{str(fp.tracked(n)):hashlib.sha256(fp.tracked(n).read_bytes()).hexdigest() for n in FILES},'runs':[]}
import generate_extension_raw_methods
assert generate_extension_raw_methods.generated() == fp.src('extension_raw_methods.inl').read_text()
with tempfile.TemporaryDirectory(prefix='device-mirror-') as tmp:
 for flags in [['-O2'],['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']]:
  executable=Path(tmp)/'test'
  command=['clang++','-std=c++17','-Wall','-Wextra','-Werror','-pthread',*flags,*fp.test_include_flags(),str(HERE/'test_device_mirror.cpp'),'-o',str(executable)]
  subprocess.run(command,check=True)
  run=subprocess.run([str(executable)],capture_output=True,text=True)
  print(run.stdout,run.stderr,flush=True);run.check_returncode()
  report['runs'].append({'compile_command':command,'flags':flags,'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr})
 # 0.3.180 (D0): the census and the threaded tests under TSan; the census keyed on held_ must fail.
 executable=Path(tmp)/'tsan'
 subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-pthread','-O1','-g','-fsanitize=thread',*fp.test_include_flags(),str(HERE/'test_device_mirror.cpp'),'-o',str(executable)],check=True)
 run=subprocess.run([str(executable),'threads'],capture_output=True,text=True)
 print(run.stdout,run.stderr,flush=True);run.check_returncode();assert 'ThreadSanitizer' not in run.stderr
 report['runs'].append({'flags':['-fsanitize=thread'],'mode':'threads','exit_code':run.returncode,'stdout':run.stdout})
 executable=Path(tmp)/'census-by-held'
 subprocess.run(['clang++','-std=c++17','-pthread','-O2','-DNORTHLIGHT_GATE_CENSUS_BY_HELD=1',*fp.test_include_flags(),str(HERE/'test_device_mirror.cpp'),'-o',str(executable)],check=True)
 run=subprocess.run([str(executable),'census'],capture_output=True,text=True)
 missed=run.returncode!=0 and 'Assertion failed' in run.stderr
 print('census counterfactual (keyed on held_): '+('misses the foreign calls (expected failure)' if missed else 'NOT DETECTED'),flush=True);assert missed,(run.returncode,run.stdout,run.stderr)
 report['runs'].append({'flags':['-DNORTHLIGHT_GATE_CENSUS_BY_HELD=1'],'mode':'census','exit_code':run.returncode,'expected':'failure'})
# 0.3.180 (D0) needles: every gate lock site is a MirrorGuard; the census sits in the real-acquisition branch.
import re
proxy={q.name:q.read_text() for q in [*fp.src('renderer.cpp').parent.glob('*.h'),fp.src('renderer.cpp'),*fp.GENERATED.glob('mirror_*.h'),fp.GENERATED/'extension_raw_methods.inl']}
guard=proxy['mirror_guard.h'];ctor=guard[guard.index('explicit MirrorGuard('):guard.index('~MirrorGuard()')]
import generate_mirror_guarded_device
generated=fp.GENERATED/'mirror_guarded_device.h';before=generated.read_text();generate_mirror_guarded_device.generate();regenerated=generated.read_text();generated.write_text(before)
checks={
 'no lock_guard/unique_lock on a recursive mutex in src/proxy or generated':not any(re.search(r'(lock_guard|unique_lock)<std::recursive_mutex>',t) for t in proxy.values()),
 'the only recursive mutexes: MirrorGate::mutex and the private lockNs bench lock':sorted((n,l.strip()) for n,t in proxy.items() for l in t.splitlines() if 'std::recursive_mutex ' in l)==
   [('mirror_guard.h','std::recursive_mutex mutex;'),('renderer.cpp','std::recursive_mutex gateBenchLock; /* private, uncontended: the microbenchmark\'s lock */')],
 'no direct lock/unlock of a gate outside mirror_guard.h':not any(re.search(r'(\bgate(_|\(\))?|\bmutex)\.(lock|unlock|try_lock)\(',t) for n,t in proxy.items() if n!='mirror_guard.h'),
 'both generators emit MirrorGuard':'MirrorGuard lock(m->gate,MirrorSite::Device);' in fp.tracked('generate_mirror_guarded_device.py').read_text()
   and "prefix='MirrorGuard lock(gate,MirrorSite::Resource); '" in fp.tracked('generate_mirror_resource_forwarders.py').read_text()
   and proxy['mirror_guarded_device.h'].count('MirrorGuard lock(m->gate,MirrorSite::Device);')==78 and 'MirrorGuard lock(gate,MirrorSite::Resource);' in proxy['mirror_resource_forwarders.h'],
 'mirror_guarded_device.h is up to date with its generator':regenerated==before,
 'MirrorGate has ownerTid, set by the Device constructor':'std::uint32_t ownerTid;' in guard and 'mirrorState.gate.ownerTid=MirrorGuard::threadId();' in proxy['renderer.cpp'],
 'census only in the real-acquisition branch':ctor.count('gate.entered(')==1 and ctor.index('if(kMirrorSingleGate&&previous_==&gate)return;')<ctor.index('gate.mutex.lock();')<ctor.index('gate.entered('),
 'census site classes at every site':proxy['mirror_resources.h'].count('MirrorSite::Registry')==10 and proxy['mirror_resources.h'].count('MirrorSite::Resource')==6
   and proxy['device_mirror.h'].count('MirrorSite::StateBlock')==4 and 'lock(owner.m->gate,MirrorSite::Raw)' in proxy['device_mirror.h']
   and proxy['renderer.cpp'].count('MirrorGuard lock(resources->gate(),MirrorSite::SwapChain);')==3,
 'tracked buffers report Lock/Unlock/Release':proxy['tracked_buffers.h'].count('if(census)census->noteBuffer();')==3 and proxy['renderer.cpp'].count('&bufferEscape,&mirrorState.gate)')==4,
}
for name,ok in checks.items():print(('PASS ' if ok else 'FAIL ')+name)
assert all(checks.values())
report['checks']=checks
for path,digest in report['source_sha256'].items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,path
OUT.mkdir(exist_ok=True)
(OUT/'device-mirror-validation.json').write_text(json.dumps(report,indent=2)+'\n')
