#!/usr/bin/env python3
"""Tracked copies of the client-root profile files must be byte-identical to the live ones.

The renderer reads celestial-profiles.ini and shadow-range-profiles.ini from the client
root (and renderer/build_packages.py packages them from there). The client root is the
live copy people tune; this folder only mirrors it so git versions it.

  python3 check_client_config.py           exit 1 if a copy differs; SKIP (exit 0) when the
                                           client-root files are absent (scratch copies)
  python3 check_client_config.py --update  copy client root -> this folder (never the reverse)
"""
from pathlib import Path
import hashlib, shutil, sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import northlight_paths  # noqa: E402
CLIENT = northlight_paths.client_root(required=False) or HERE.parents[1]
FILES = ('celestial-profiles.ini', 'shadow-range-profiles.ini')

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else 'missing'

def main():
    if '--update' in sys.argv:
        for name in FILES:
            shutil.copyfile(CLIENT / name, HERE / name)
            print('updated', name, sha(HERE / name))
        return 0
    if not all((CLIENT / n).is_file() for n in FILES):
        print('SKIP client-root profiles not found under', CLIENT, '(scratch copy?)')
        return 0
    bad = [n for n in FILES if sha(CLIENT / n) != sha(HERE / n)]
    for n in FILES:
        print(('DIFFERS ' if n in bad else 'OK      ') + n, 'client', sha(CLIENT / n)[:16], 'tracked', sha(HERE / n)[:16])
    if bad:
        print('The client root is the live copy: copy root -> client-config/ '
              '(python3 client-config/check_client_config.py --update) and commit.')
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
