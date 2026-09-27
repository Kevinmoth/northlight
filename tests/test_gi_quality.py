#!/usr/bin/env python3
# northlight-test: requires=cxx slow
"""GI quality knobs: defaults == 0.3.137, default-path probe identity, GIThreads determinism (native clang++, O2 and ASan/UBSan+TSan)."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import subprocess,tempfile
HERE=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='northlight-gi-quality-') as tmp:
    for label,flags in [('O2',['-O2']),('asan',['-O1','-g','-fsanitize=address,undefined']),('tsan',['-O1','-g','-fsanitize=thread'])]:
        exe=Path(tmp)/('test-'+label)
        subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,*fp.test_include_flags(),str(HERE/'test_gi_quality.cpp'),str(fp.src('world_gi.cpp')),'-o',str(exe)],check=True)
        print(label,subprocess.check_output([str(exe)],cwd=HERE,text=True),end='',flush=True)
