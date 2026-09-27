#!/usr/bin/env python3
# northlight-test:
"""graphics-work/renderer_status.py drives the d3d9.dll proxy migration and can
never write the patched wow.exe: a source audit of every exe-writing tool (no code
builds the frd9 import rename) and on/off/status on a temporary client copy of
the tool layout with every wow.exe write recorded. No Wine or game."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
import hashlib,importlib.util,io,json,re,shutil,tempfile,unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch
HERE=Path(__file__).resolve().parent
STATUS=fp.REPO/'renderer_status.py'
TOOLS=[STATUS,fp.src('migrate_mac_proxy.py'),fp.src('windows-package/install.py')]
sha=lambda b:hashlib.sha256(b).hexdigest()

class NoExePatch(unittest.TestCase):
    def test_source_has_no_patch_path(self):
        for path in TOOLS:
            src=path.read_text()
            # frd9 bytes literals only in comparisons (recognising our old patch), never built into data.
            for line in src.splitlines():
                if re.search(r"b['\"]frd9",line):self.assertTrue(re.search(r"==\s*b'frd9\.dll\\0'",line),(path.name,line))
            self.assertIsNone(re.search(r"b['\"]fr['\"]",src),path.name)   # the only exe byte literal written is b'd3'
            for forbidden in ["+b'frd9.dll'",'+b"frd9.dll"','patch_exe','original_exe','inspect_bootstrap','wow-before-renderer.exe']:
                self.assertNotIn(forbidden,src,(path.name,forbidden))
        # No tool gates on the exe identity: known hashes are informational only.
        for path in TOOLS[1:]:
            self.assertIsNone(re.search(r"(!=|==|not in|in) *\(?m?\.?(ORIGINAL|PATCHED)\b",path.read_text()),path.name)

@unittest.skipUnless(STATUS.is_file(),'graphics-work/renderer_status.py not beside this renderer tree')
class OnOff(unittest.TestCase):
    INI=None   # northlight.local.ini content for the fake tree (test_local_ini.py passes broken ones)
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();base=Path(self.tmp.name)
        self.client=base/'game';work=self.client/'graphics-work';(work/'renderer/windows-package').mkdir(parents=True);(self.client/'mods').mkdir()
        shutil.copy2(STATUS,work/'renderer_status.py');shutil.copy2(fp.src('migrate_mac_proxy.py'),work/'renderer/migrate_mac_proxy.py')
        shutil.copy2(fp.src('windows-package/install.py'),work/'renderer/windows-package/install.py')
        shutil.copy2(fp.REPO/'northlight_paths.py',work/'northlight_paths.py')   # migrate_mac_proxy imports it
        if self.INI is not None:(work/'northlight.local.ini').write_bytes(self.INI)
        self.original=b'head\0d3d9.dll\0\0\0\0-tail';self.patched=b'head\0frd9.dll\0\0\0\0-tail';self.dxvk=b'MZ DXVK: \0v1.10.3\0'
        for n,d in {'wow.exe':self.patched,'d3d9.dll':self.dxvk,'frd9.dll':b'MZ Northlight renderer 0.3.147;','dlls.txt':b'mods/winerosetta.dll\n','libDllLdr.dll':b'ldr','DivxDecoder.dll':b'patched','DivxDecoder.dll.bak':b'original',
                    'graphics-work/renderer/frd9.dll':b'MZ Northlight renderer 0.3.148; PROXY module=%ls root=%ls'}.items():(self.client/n).write_bytes(d)
        spec=importlib.util.spec_from_file_location('status_copy',work/'renderer_status.py')
        self.rs=importlib.util.module_from_spec(spec)
        self.saved=sys.modules.pop('northlight_paths',None)   # the copy must load the fake tree's northlight_paths
        try:spec.loader.exec_module(self.rs)
        finally:
            self.loaded=sys.modules.pop('northlight_paths',None)
            if self.saved:sys.modules['northlight_paths']=self.saved
        self.assertEqual(Path(self.rs.migrate.northlight_paths.__file__).resolve().parent,work.resolve())
        mig=self.rs.migrate;self.writes=[];real=mig.write
        def spy(path,data):
            if path.name=='wow.exe':self.writes.append(sha(data))
            return real(path,data)
        self.patches=[patch.multiple(mig,OFFSET=5,ORIGINAL=sha(self.original),PATCHED=sha(self.patched)),
                      patch.object(mig,'WOWSILICON_DXVK',sha(self.dxvk)),patch.object(mig,'REFERENCE_EXE',base/'no-reference/wow.exe'),patch.object(mig,'VERSIONS',base/'versions.json'),patch.object(mig,'running',lambda:[]),patch.object(mig,'write',spy)]
        for p in self.patches:p.start()
    def tearDown(self):
        for p in reversed(self.patches):p.stop()
        self.tmp.cleanup()
    def run_status(self,*args):
        out=io.StringIO()
        with redirect_stdout(out):self.rs.main(list(args))
        return out.getvalue()
    def test_on_off_status_never_patch(self):
        before=(self.client/'dlls.txt').read_bytes()
        self.assertIn('Dry run',self.run_status('on','--dry-run'));self.assertEqual((self.client/'wow.exe').read_bytes(),self.patched)
        self.assertTrue(json.loads(self.run_status('status'))['wow_exe_old_frd9_patch'])
        self.run_status('on')
        state=json.loads(self.run_status('status'))
        self.assertEqual((state['wow_exe_old_frd9_patch'],state['proxy_preloaded'],state['backend_matches_game_d3d9']),(False,True,True))
        self.run_status('off')
        self.assertEqual((self.client/'wow.exe').read_bytes(),self.original);self.assertEqual((self.client/'dlls.txt').read_bytes(),before)
        self.assertFalse((self.client/'mods/d3d9.dll').exists())
        self.run_status('on');self.run_status('off')   # repeatable
        self.assertEqual(self.writes,[sha(self.original)])
        self.assertNotIn(sha(self.patched),self.writes)

if __name__=='__main__':unittest.main()
