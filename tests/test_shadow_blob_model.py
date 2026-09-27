#!/usr/bin/env python3
# northlight-test: requires=cxx
"""0.3.161 analytic shadow-blob reference (src/shadows/shadow_blob_model.h): 32x32 with 648
opaque pixels, accepted by the filter comparison after a BGRA8 decode round trip; shifted,
rescaled, recoloured, inverted and noise textures rejected. Native clang++ (plain and
ASan/UBSan); no client, game or Wine. The client comparison is test_shadow_blob_client.py."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import subprocess,tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent

def run(*args):
    with tempfile.TemporaryDirectory(prefix='shadow-blob-') as tmp:
        for label,flags in [('O2',['-O2']),('asan',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
            exe=Path(tmp)/('test-'+label)
            subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,*fp.test_include_flags(),str(HERE/'test_shadow_blob_model.cpp'),'-o',str(exe)],check=True)
            print(label,subprocess.check_output([str(exe),*map(str,args)],text=True),end='',flush=True)

if __name__=='__main__':run()
