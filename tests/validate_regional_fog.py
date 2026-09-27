# northlight-test: requires=cxx,client
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,subprocess,tempfile
here=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='northlight-regional-fog-') as temp:
 binary=str(Path(temp)/'test')
 subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',*fp.test_include_flags(),str(here/'test_regional_fog.cpp'),'-o',binary],check=True)
 report=json.loads(subprocess.check_output([binary,str(fp.client_root()/'world-cache/fog')],text=True))
report['source_sha256']={n:hashlib.sha256(fp.tracked(n).read_bytes()).hexdigest() for n in ['regional_fog.h','regional_fog_builder.py','test_regional_fog.cpp','validate_regional_fog.py']}
report['game_or_device_launched']=False
(fp.output_dir()/'regional-fog-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
