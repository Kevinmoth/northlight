#!/usr/bin/env python3
# northlight-test:
"""A malformed northlight.local.ini never stops a tool: northlight_paths ignores it with a warning, and
renderer_status.py status/on/off still import and run (the full on/off round trip of
test_renderer_status.OnOff, on a temporary client, with a broken ini beside the copied tools)."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import io,os,tempfile,unittest
from contextlib import redirect_stderr
from unittest.mock import patch
from test_renderer_status import OnOff

BROKEN={'percent':b'[paths]\narchive = ~/x%20y\nreference_exe = ~/a%zb/wow.exe\n',
        'duplicate':b'[paths]\narchive = /nonexistent-a\narchive = /nonexistent-b\n',   # not client: renderer_status would rightly refuse it
        'no_section':b'client = /nonexistent\njust garbage\n',
        'binary':b'\xff\xfe\x00[paths\n'}

class Setting(unittest.TestCase):
    def test_broken_ini_is_ignored(self):
        for name,data in BROKEN.items():
            with self.subTest(name),tempfile.TemporaryDirectory() as t,patch.dict(os.environ,{'NORTHLIGHT_REFERENCE_EXE':''}):
                ini=Path(t)/'northlight.local.ini';ini.write_bytes(data)
                saved=(fp.LOCAL_INI,fp._ini_cache);fp.LOCAL_INI,fp._ini_cache=ini,None
                err=io.StringIO()
                try:
                    with redirect_stderr(err):value=fp.setting('reference_exe')
                finally:fp.LOCAL_INI,fp._ini_cache=saved
                if name=='percent':self.assertEqual(value,'~/a%zb/wow.exe')   # no interpolation: '%' is literal
                elif name=='duplicate':self.assertIsNone(value)   # strict=False: the last value wins, no error
                else:self.assertIsNone(value);self.assertIn('ignoring unreadable',err.getvalue())

def broken(name,data):
    return type('OnOff_'+name,(OnOff,),{'INI':data})

for name,data in BROKEN.items():
    globals()['OnOff_'+name]=broken(name,data)
del OnOff   # run only the broken-ini variants here

if __name__=='__main__':unittest.main()
