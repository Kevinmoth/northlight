#!/usr/bin/env python3
# northlight-test: requires=cxx slow
"""0.3.141 water mask capture == verbatim 0.3.140 (SavedState/D3DSBT_ALL) on full-state fake D3D9
backends through the production device mirror (native clang++, plain and ASan/UBSan). No game or driver."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import hashlib,subprocess,tempfile
HERE=Path(__file__).resolve().parent
LEGACY=fp.SUPPORT/'water_mask/water_renderer_0_3_140.h'
# 0.3.140 water_renderer.h with only its identifiers renamed (r52); the reference must never drift.
assert hashlib.sha256(LEGACY.read_bytes()).hexdigest()=='d82ed19724e988c1336cde23010fb27a7730a9601837fc3659b4ca480950f0f2',LEGACY
with tempfile.TemporaryDirectory(prefix='water-mask-') as tmp:
    for label,flags in [('O2',['-O2']),('asan',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
        exe=Path(tmp)/('test-'+label)
        subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-I',str(fp.SUPPORT/'water_mask'),*fp.test_include_flags(),
                        str(HERE/'test_water_mask_state.cpp'),'-o',str(exe)],check=True)
        print(label,subprocess.check_output([str(exe)],cwd=HERE,text=True),end='',flush=True)
