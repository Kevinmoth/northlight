#!/usr/bin/env python3
"""Turn this macOS client's renderer on/off; never launches a process and never
patches wow.exe (any wow.exe is accepted). "on" = renderer/migrate_mac_proxy.py
--apply (mods/d3d9.dll preloaded from dlls.txt; only our old frd9 patch, if
present, is reverted: 2 bytes), "off" = its restore (wow.exe is not re-patched),
"status" = read-only state. Add --dry-run to see the steps without writing."""
from pathlib import Path
import argparse
import importlib.util
import json
import sys

WORK = Path(__file__).resolve().parent
ROOT = WORK.parent
BUILD = WORK / 'renderer'
spec = importlib.util.spec_from_file_location('migrate_mac_proxy', BUILD/'migrate_mac_proxy.py')
migrate = importlib.util.module_from_spec(spec); spec.loader.exec_module(migrate)

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['on','off','status'],default='status',nargs='?')
    parser.add_argument('--dry-run',action='store_true')
    args=parser.parse_args(argv)
    base=['--client',str(ROOT),'--dll',str(BUILD/'frd9.dll')]
    if args.action=='status':
        print(json.dumps(migrate.status(ROOT),indent=2));return
    migrate.main(base+(['--restore'] if args.action=='off' else [])+([] if args.dry_run else ['--apply']))

if __name__=='__main__':
    try:main()
    except Exception as e:
        print('ERROR:',e,file=sys.stderr);sys.exit(1)
