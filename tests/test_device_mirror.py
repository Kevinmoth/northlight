#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native tests of actual production mirror/SavedState methods against a mock device."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
HERE=Path(__file__).resolve().parent
OUT=fp.output_dir()
FILES=['extension_raw_methods.inl','mirror_guard.h','generate_extension_raw_methods.py','forwarders.h','device_mirror.h','captured_constant_epoch.h','mirror_audit_schedule.h','saved_state.h','test_device_mirror.cpp','test_device_mirror_api.h','test_device_mirror.py']
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
for path,digest in report['source_sha256'].items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,path
OUT.mkdir(exist_ok=True)
(OUT/'device-mirror-validation.json').write_text(json.dumps(report,indent=2)+'\n')
