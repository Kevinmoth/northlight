#!/usr/bin/env python3
# northlight-test:
"""graphics-work/renderer_status.py drives the d3d9.dll proxy migration and never
reads or writes wow.exe: on/off/status on a temporary client copy of the tool
layout with every wow.exe read and write recorded. No Wine or game."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
import importlib.util,io,json,os,shutil,tempfile,unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch
HERE=Path(__file__).resolve().parent
STATUS=fp.REPO/'renderer_status.py'
def tool_tree(work,ini=None):
    """Copy renderer_status.py and what it loads into work/ (a fake repository)."""
    (work/'renderer/windows-package').mkdir(parents=True)
    shutil.copy2(STATUS,work/'renderer_status.py');shutil.copy2(fp.src('migrate_mac_proxy.py'),work/'renderer/migrate_mac_proxy.py')
    shutil.copy2(fp.src('windows-package/install.py'),work/'renderer/windows-package/install.py')
    shutil.copy2(fp.REPO/'northlight_paths.py',work/'northlight_paths.py')   # renderer_status imports it
    if ini is not None:(work/'northlight.local.ini').write_bytes(ini)

def load(work):
    """Import work/renderer_status.py with work/'s own northlight_paths."""
    spec=importlib.util.spec_from_file_location('status_copy',work/'renderer_status.py')
    rs=importlib.util.module_from_spec(spec)
    saved=sys.modules.pop('northlight_paths',None)
    try:spec.loader.exec_module(rs)
    finally:
        sys.modules.pop('northlight_paths',None)
        if saved:sys.modules['northlight_paths']=saved
    return rs

def fake_client(path):
    (path/'Data').mkdir(parents=True);(path/'wow.exe').write_bytes(b'MZ')
    return path

NO_CLIENT_ENV={k:v for k,v in os.environ.items() if k!='NORTHLIGHT_CLIENT'}

@unittest.skipUnless(STATUS.is_file(),'graphics-work/renderer_status.py not beside this renderer tree')
class OnOff(unittest.TestCase):
    INI=None   # northlight.local.ini content for the fake tree (test_local_ini.py passes broken ones)
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();base=Path(self.tmp.name)
        self.client=base/'game';work=self.client/'graphics-work';tool_tree(work,self.INI);(self.client/'mods').mkdir();(self.client/'Data').mkdir()
        self.exe=b'MZ any wow.exe build';self.dxvk=b'MZ DXVK: \0v1.10.3\0'
        for n,d in {'wow.exe':self.exe,'d3d9.dll':self.dxvk,'dlls.txt':b'mods/winerosetta.dll\n','libDllLdr.dll':b'ldr','DivxDecoder.dll':b'patched','DivxDecoder.dll.bak':b'original',
                    'graphics-work/renderer/frd9.dll':b'MZ Northlight renderer 0.3.148; PROXY module=%ls root=%ls'}.items():(self.client/n).write_bytes(d)
        self.rs=load(work)
        self.assertEqual(Path(self.rs.northlight_paths.__file__).resolve().parent,work.resolve())
        mig=self.rs.migrate;self.exe_access=[]
        def spy(real):
            def call(path,*data):
                if path.name.lower()=='wow.exe':self.exe_access.append(real.__name__)
                return real(path,*data)
            return call
        self.patches=[patch.dict(os.environ,NO_CLIENT_ENV,clear=True),   # the default: the repository's parent
                      patch.object(mig,'VERSIONS',base/'versions.json'),patch.object(mig,'running',lambda:[]),
                      patch.multiple(mig,read=spy(mig.read),write=spy(mig.write))]
        for p in self.patches:p.start()
    def tearDown(self):
        for p in reversed(self.patches):p.stop()
        self.tmp.cleanup()
    def run_status(self,*args):
        out=io.StringIO()
        with redirect_stdout(out):self.rs.main(list(args))
        return out.getvalue()
    def test_on_off_status_never_touch_exe(self):
        before=(self.client/'dlls.txt').read_bytes()
        self.assertIn('Dry run',self.run_status('on','--dry-run'))
        self.run_status('on')
        state=json.loads(self.run_status('status'))
        self.assertEqual((state['proxy_preloaded'],state['backend_matches_game_d3d9']),(True,True))
        self.run_status('off')
        self.assertEqual((self.client/'dlls.txt').read_bytes(),before);self.assertFalse((self.client/'mods/d3d9.dll').exists())
        self.run_status('on');self.run_status('off')   # repeatable
        self.assertEqual(self.exe_access,[]);self.assertEqual((self.client/'wow.exe').read_bytes(),self.exe)

@unittest.skipUnless(STATUS.is_file(),'graphics-work/renderer_status.py not beside this renderer tree')
class ClientChoice(unittest.TestCase):
    """A repository outside any client: --client, then NORTHLIGHT_CLIENT, then northlight.local.ini."""
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();base=Path(self.tmp.name)
        self.work=base/'repo';tool_tree(self.work);self.rs=load(self.work)
        self.a=fake_client(base/'a').resolve();self.b=fake_client(base/'b').resolve();self.empty=base/'empty';self.empty.mkdir()
        self.seen=[]
        self.patches=[patch.dict(os.environ,NO_CLIENT_ENV,clear=True),
                      patch.object(self.rs.migrate,'status',lambda c:self.seen.append(('status',c)) or {}),
                      patch.object(self.rs.migrate,'main',lambda argv:self.seen.append(('main',argv)))]
        for p in self.patches:p.start()
    def tearDown(self):
        for p in reversed(self.patches):p.stop()
        self.tmp.cleanup()
    def chosen(self,*args):
        self.seen.clear()
        with redirect_stdout(io.StringIO()):self.rs.main(list(args))
        return self.seen
    def test_client_flag_env_ini(self):
        self.assertEqual(self.chosen('status','--client',str(self.a)),[('status',self.a)])
        self.assertEqual(self.chosen('on','--dry-run','--client',str(self.a)),[('main',['--client',str(self.a),'--dll',str(self.work.resolve()/'renderer/frd9.dll')])])
        os.environ['NORTHLIGHT_CLIENT']=str(self.b)
        self.assertEqual(self.chosen('status'),[('status',self.b)])
        self.assertEqual(self.chosen('off','--client',str(self.a))[0][1][:2],['--client',str(self.a)])   # --client wins
        del os.environ['NORTHLIGHT_CLIENT']
        (self.work/'northlight.local.ini').write_text(f'[paths]\nclient = {self.b}\n');self.rs.northlight_paths._ini_cache=None
        self.assertEqual(self.chosen('status'),[('status',self.b)])
    def test_no_client_is_a_clear_error(self):
        with self.assertRaisesRegex(ValueError,'NORTHLIGHT_CLIENT.*--client PATH'):self.chosen('status')
        os.environ['NORTHLIGHT_CLIENT']=str(self.empty)
        with self.assertRaisesRegex(ValueError,'configured client .*empty has no Wow.exe'):self.chosen('on')
        with self.assertRaisesRegex(ValueError,'--client .*empty: not a WoW client'):self.chosen('status','--client',str(self.empty))
        self.assertEqual(self.seen,[])

if __name__=='__main__':unittest.main()
