#!/usr/bin/env python3
"""Turn this macOS client's renderer on/off; never launches a process, and wow.exe
is never read or written (any wow.exe is accepted). "on" = renderer/migrate_mac_proxy.py
--apply (mods/d3d9.dll preloaded from dlls.txt), "off" = its restore,
"status" = read-only state. Add --dry-run to see the steps without writing.
The client is --client PATH, else NORTHLIGHT_CLIENT, else northlight.local.ini
[paths] client, else the repository's parent folder."""
from pathlib import Path
import argparse
import importlib.util
import json
import sys

WORK = Path(__file__).resolve().parent
BUILD = WORK / 'renderer'
sys.path.insert(0, str(WORK)); import northlight_paths
spec = importlib.util.spec_from_file_location('migrate_mac_proxy', BUILD/'migrate_mac_proxy.py')
migrate = importlib.util.module_from_spec(spec); spec.loader.exec_module(migrate)

def client(path):
    """--client PATH, else northlight_paths.client_root(); a clear error when neither is a client."""
    if path is not None:
        path=path.expanduser()
        if not northlight_paths.is_client(path):raise ValueError(f'--client {path}: not a WoW client (no Wow.exe and Data/ there)')
        return path.resolve()
    try:return northlight_paths.client_root().resolve()
    except northlight_paths.Missing as e:
        configured=northlight_paths.setting('client')
        raise ValueError(str(e)+(f' The configured client {configured} has no Wow.exe and Data/.' if configured else '')+' Or pass --client PATH.') from None

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['on','off','status'],default='status',nargs='?')
    parser.add_argument('--dry-run',action='store_true')
    parser.add_argument('--client',type=Path,metavar='PATH',help='game folder (default: NORTHLIGHT_CLIENT, northlight.local.ini, then the repository\'s parent)')
    args=parser.parse_args(argv)
    root=client(args.client)
    base=['--client',str(root),'--dll',str(BUILD/'frd9.dll')]
    if args.action=='status':
        print(json.dumps(migrate.status(root),indent=2));return
    migrate.main(base+(['--restore'] if args.action=='off' else [])+([] if args.dry_run else ['--apply']))

if __name__=='__main__':
    try:main()
    except Exception as e:
        print('ERROR:',e,file=sys.stderr);sys.exit(1)
