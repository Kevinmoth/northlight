# northlight-test: requires=client
"""The tracked client-config/*.ini copies match the client-root files byte for byte."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import subprocess, sys
HERE = Path(__file__).resolve().parent
r = subprocess.run([sys.executable, str(fp.REPO/'client-config/check_client_config.py')], capture_output=True, text=True)
print(r.stdout.strip())
if r.returncode:
    raise SystemExit('FAIL client-config copies differ from the client root')
print('PASS client-config profiles byte-identical to the client root')
