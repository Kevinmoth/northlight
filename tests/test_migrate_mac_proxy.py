#!/usr/bin/env python3
# northlight-test: requires=dll
"""macOS client migration to the dlls.txt-preloaded mods/d3d9.dll proxy
(migrate_mac_proxy.py) on temporary clients: synthetic files, and read-only copies
of this client's real wow.exe, d3d9.dll and dlls.txt when present
(NORTHLIGHT_LIVE_CLIENT, else the configured client). Dry run writes nothing; apply order,
failure injection at every step, semantic restore (Mod Manager re-sorts, user
removals); wow.exe is never read or written. No Wine or game."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
import hashlib,importlib.util,io,json,os,shutil,tempfile,unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('migrate',fp.src('migrate_mac_proxy.py'))
mig=importlib.util.module_from_spec(spec);spec.loader.exec_module(mig)
m=mig.m
LIVE=Path(os.environ.get('NORTHLIGHT_LIVE_CLIENT') or fp.client_root(required=False) or '/nonexistent-client')
PROXY=b'MZ Northlight renderer 0.3.148; PROXY module=%ls root=%ls'
DXVK=b'MZ DXVK: \0\0\0v1.10.3-20230507-async (macOS)\0'
DLLS=b'mods/winerosetta.dll\nmods/libSiliconPatch.dll\n'
EXE=b'MZ any wow.exe build'
sha=lambda b:hashlib.sha256(b).hexdigest()

def tree(root):
    return {p.relative_to(root).as_posix():p.read_bytes() for p in root.rglob('*') if p.is_file()}

class Migration(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();base=Path(self.tmp.name)
        self.client=base/'game';(self.client/'mods').mkdir(parents=True);self.backups=base/'backups'
        self.dll=base/'build.dll';self.dll.write_bytes(PROXY)
        files={'wow.exe':EXE,'d3d9.dll':DXVK,'dlls.txt':DLLS,'mods/winerosetta.dll':b'wr',
               'libDllLdr.dll':b'ldr','DivxDecoder.dll':b'divx patched','DivxDecoder.dll.bak':b'divx original'}
        for n,d in files.items():(self.client/n).write_bytes(d)
        self.dxvk=patch.object(mig,'WOWSILICON_DXVK',sha(DXVK));self.dxvk.start()
        self.busy=patch.object(mig,'running',lambda:[]);self.busy.start()
        self.versions=base/'versions.json';self.ver=patch.object(mig,'VERSIONS',self.versions);self.ver.start()
        # Every file read and write goes through mig.read and mig.write: none may touch wow.exe.
        self.exe_access=[];read,write=mig.read,mig.write
        def spy(real):
            def call(path,*data):
                if path.name.lower()=='wow.exe':self.exe_access.append(real.__name__)
                return real(path,*data)
            return call
        self.spy=patch.multiple(mig,read=spy(read),write=spy(write));self.spy.start()
    def tearDown(self):
        for p in [self.spy,self.ver,self.busy,self.dxvk]:p.stop()
        self.assertEqual(self.exe_access,[]);self.assertEqual((self.client/'wow.exe').read_bytes(),EXE)
        self.tmp.cleanup()
    def run_main(self,*args):
        out=io.StringIO()
        with redirect_stdout(out):mig.main(['--client',str(self.client),'--dll',str(self.dll),'--backup-root',str(self.backups),*args])
        return out.getvalue()
    def migrated(self):
        return {**self.before,'mods/d3d9.dll':PROXY,'renderer-backends/dxvk/dxvk_d3d9.dll':DXVK,
                'northlight-renderer.ini':mig.CONFIG,'dlls.txt':DLLS+b'mods/d3d9.dll\n'}
    def test_dry_run_writes_nothing(self):
        before=tree(self.client);out=self.run_main()
        self.assertIn('Dry run',out);self.assertEqual(tree(self.client),before);self.assertFalse(self.backups.exists())
    def test_apply_order_and_restore(self):
        self.before=tree(self.client);self.run_main('--apply')
        self.assertEqual(tree(self.client),self.migrated())   # d3d9.dll (WoWSilicon DXVK) and wow.exe untouched
        record_dir=next(self.backups.glob('*'));record=json.loads((record_dir/'transaction.json').read_text())
        self.assertEqual([f['path'] for f in record['files']],[mig.BACKEND,'northlight-renderer.ini',mig.PROXY,'dlls.txt'])
        self.assertIn('Already migrated',self.run_main())
        self.assertEqual(json.loads(self.run_main('--status'))['proxy_preloaded'],True)
        self.assertIn('Dry run',self.run_main('--restore'));self.assertEqual(tree(self.client),self.migrated())
        self.run_main('--restore','--apply')
        self.assertEqual(tree(self.client),self.before)
        self.assertFalse((self.client/'renderer-backends').exists())
    def test_restore_after_mod_manager_resort(self):
        self.before=tree(self.client);self.run_main('--apply')
        # ModService.writeDllsFile: sorted set, one new mod.
        (self.client/'dlls.txt').write_bytes(b'mods/d3d9.dll\nmods/libSiliconPatch.dll\nmods/newmod.dll\nmods/winerosetta.dll\n')
        self.run_main('--restore','--apply')
        self.assertEqual((self.client/'dlls.txt').read_bytes(),b'mods/libSiliconPatch.dll\nmods/newmod.dll\nmods/winerosetta.dll\n')
        self.assertFalse((self.client/'mods/d3d9.dll').exists())
    def test_restore_tolerates_user_removal_and_foreign_mod(self):
        self.before=tree(self.client);self.run_main('--apply')
        (self.client/'dlls.txt').write_bytes(DLLS);(self.client/'mods/d3d9.dll').write_bytes(b'someone else d3d9 mod')
        (self.client/'northlight-renderer.ini').write_bytes(b'[Renderer]\r\nBackend=native\r\n')
        out=self.run_main('--restore','--apply')
        self.assertEqual((self.client/'mods/d3d9.dll').read_bytes(),b'someone else d3d9 mod')   # never deletes a foreign file
        self.assertEqual((self.client/'northlight-renderer.ini').read_bytes(),b'[Renderer]\r\nBackend=native\r\n')
        self.assertFalse((self.client/mig.BACKEND).exists());self.assertIn('left as it is',out)
        (self.client/'mods/d3d9.dll').unlink();self.assertEqual(tree(self.client),{**self.before,'northlight-renderer.ini':b'[Renderer]\r\nBackend=native\r\n'})
    def test_restore_brings_back_previous_ini(self):
        (self.client/'northlight-renderer.ini').write_bytes(b'[Renderer]\r\nBackend=dxvk\r\n');self.before=tree(self.client)
        self.run_main('--apply');self.run_main('--restore','--apply')
        self.assertEqual(tree(self.client),self.before)
    def test_failure_at_every_step_rolls_back(self):
        self.before=tree(self.client);real=mig.write
        for failing in range(1,5):
            with self.subTest(failing=failing):
                count=[0]
                def fail(path,data):
                    count[0]+=1
                    if count[0]==failing:raise OSError('simulated disk failure')
                    return real(path,data)
                with patch.object(mig,'write',fail):
                    with self.assertRaises(OSError):self.run_main('--apply')
                self.assertEqual(tree(self.client),self.before)
                shutil.rmtree(self.backups)
    def test_failure_after_last_step_rolls_back(self):
        self.before=tree(self.client);real=m.atomic_json
        def fail(path,value):
            if value['status']=='installed':raise OSError('simulated failure after the last step')
            return real(path,value)
        with patch.object(m,'atomic_json',fail):
            with self.assertRaises(OSError):self.run_main('--apply')
        self.assertEqual(tree(self.client),self.before)
    def test_preload_refusal_explains(self):
        (self.client/'libDllLdr.dll').unlink()
        with self.assertRaises(ValueError) as refused:self.run_main('--apply')
        self.assertIn('preload is not active',str(refused.exception));self.assertIn('nothing changed',str(refused.exception))
    def test_refusals_before_any_write(self):
        cases=[('d3d9.dll',b'MZ Northlight renderer 0.3.148 proxy'),('d3d9.dll',b'KosmicKrisp d3d9'),('mods/d3d9.dll',b'foreign mod')]
        for name,data in cases:
            with self.subTest(name=name,data=data):
                old=(self.client/name).read_bytes() if (self.client/name).exists() else None;before=tree(self.client)
                (self.client/name).write_bytes(data);expected=tree(self.client)
                with self.assertRaises(ValueError):self.run_main('--apply')
                self.assertEqual(tree(self.client),expected);self.assertFalse(self.backups.exists())
                if old is None:(self.client/name).unlink()
                else:(self.client/name).write_bytes(old)
                self.assertEqual(tree(self.client),before)
        self.dll.write_bytes(b'MZ Northlight renderer 0.3.147; without the proxy loader')
        with self.assertRaises(ValueError):self.run_main('--apply')
    def old_record(self,data,client=None,status='installed',files=None):
        # A migration record from before a rename: its mods/d3d9.dll carries no name that is matched today.
        d=self.backups/'20260101-000000-old';d.mkdir(parents=True,exist_ok=True)
        (d/'transaction.json').write_text(json.dumps({'kind':mig.KIND,'version':'0.3.165','client':client or str(self.client.resolve()),'status':status,
            'files':files or [{'path':mig.PROXY,'before':None,'after':sha(data)}]}))
        return d
    def test_restore_touches_only_managed_paths(self):
        d=self.old_record(None,files=[{'path':'wow.exe','before':sha(b'older bytes'),'after':sha(EXE)}])
        (d/'before').mkdir();(d/'before/wow.exe').write_bytes(b'older bytes');self.before=tree(self.client)
        self.assertIn('wow.exe: not written by this tool; left as it is',self.run_main('--restore','--apply'))
        self.assertEqual(tree(self.client),self.before)
    def test_old_recorded_proxy_is_accepted_and_replaced(self):
        old=b'MZ renamed-away build 0.3.165';(self.client/'mods/d3d9.dll').write_bytes(old);self.old_record(old)
        self.before=tree(self.client);self.run_main('--apply')
        self.assertEqual(tree(self.client),{**self.migrated(),'mods/d3d9.dll':PROXY})
        record_dir=sorted(self.backups.glob('*'))[-1];files=json.loads((record_dir/'transaction.json').read_text())['files']
        self.assertEqual([f['before'] for f in files if f['path']==mig.PROXY],[sha(old)])
        self.assertEqual((record_dir/'before'/mig.PROXY).read_bytes(),old)
    def test_old_signature_proxy_is_accepted_without_record(self):
        old=b'MZ renamed-away build; PROXY module=%ls root=%ls';(self.client/'mods/d3d9.dll').write_bytes(old)
        self.before=tree(self.client);self.run_main('--apply')
        self.assertEqual((self.client/'mods/d3d9.dll').read_bytes(),PROXY)
    def test_foreign_mods_d3d9_still_refused(self):
        old=b'MZ renamed-away build 0.3.165'
        # No record, a record of another client, a restored record, or a record of other bytes: all foreign.
        for data,record in [(old,None),(old,dict(client='/elsewhere')),(old,dict(status='restored')),(b'another mod',dict())]:
            with self.subTest(data=data,record=record):
                (self.client/'mods/d3d9.dll').write_bytes(data)
                if record is not None:self.old_record(old,**record)
                before=tree(self.client)
                with self.assertRaises(ValueError) as refused:self.run_main('--apply')
                self.assertIn('another Mod Manager mod',str(refused.exception))
                self.assertEqual(tree(self.client),before);shutil.rmtree(self.backups,ignore_errors=True)
    def test_other_dxvk_build_accepted(self):
        other=b'MZ DXVK: \0v2.4.1\0 a newer WoWSilicon';(self.client/'d3d9.dll').write_bytes(other);self.before=tree(self.client)
        self.assertIn('other build, DXVK v2.4.1',self.run_main());self.run_main('--apply')
        self.assertEqual((self.client/mig.BACKEND).read_bytes(),other);self.assertEqual((self.client/'d3d9.dll').read_bytes(),other)
    def test_preload_must_be_live(self):
        for name,data in [('libDllLdr.dll',None),('DivxDecoder.dll',b'divx original'),('DivxDecoder.dll.bak',None),('dlls.txt',b'mods/libSiliconPatch.dll\n')]:
            with self.subTest(name=name):
                old=(self.client/name).read_bytes()
                if data is None:(self.client/name).unlink()
                else:(self.client/name).write_bytes(data)
                expected=tree(self.client)
                with self.assertRaises(ValueError):self.run_main('--apply')
                self.assertEqual(tree(self.client),expected);self.assertFalse(self.backups.exists())
                (self.client/name).write_bytes(old)
    def test_launcher_warnings(self):
        game=str(self.client/'wow.exe')
        self.assertIn('not readable',mig.launcher(self.client)[0])
        self.versions.write_text(json.dumps({'versions':{'wrath':{'game_path':'/elsewhere/wow.exe'}}}))
        self.assertIn('not configured for this client',mig.launcher(self.client)[0])
        self.versions.write_text(json.dumps({'versions':{'wrath':{'game_path':game,'settings':{'graphicsSettings':{'backend':'mtld3d'}}}}}))
        self.assertIn('mtld3d',mig.launcher(self.client)[0]);before=tree(self.client)
        with self.assertRaises(ValueError) as refused:self.run_main('--apply')   # d3d9=b: refused before any write
        self.assertIn('MTLD3D',str(refused.exception));self.assertEqual(tree(self.client),before);self.assertFalse(self.backups.exists())
        self.versions.write_text(json.dumps({'versions':{'wrath':{'game_path':game,'settings':{'graphicsSettings':{'backend':'d9vk'}}}}}))
        self.assertEqual(mig.launcher(self.client),[])
        self.assertEqual(json.loads(self.run_main('--status'))['preload'],{'libDllLdr':True,'DivxDecoder_patched':True,'winerosetta_listed':True})
    def test_refuses_while_game_runs(self):
        with patch.object(mig,'running',lambda:['wow.exe']):
            with self.assertRaises(ValueError):self.run_main('--apply')
        self.assertFalse(self.backups.exists())
    def test_windows_restore_refuses_migration_record(self):
        self.run_main('--apply');record_dir=next(self.backups.glob('*'))
        with self.assertRaises(ValueError):m.restore(self.client,record_dir)
    def test_dlls_entry_helpers(self):
        self.assertEqual(mig.with_entry('a.dll'),'a.dll\nmods/d3d9.dll\n');self.assertEqual(mig.with_entry(''),'mods/d3d9.dll\n')
        self.assertEqual(mig.with_entry('MODS\\D3D9.DLL\r\n'),'MODS\\D3D9.DLL\r\n')
        self.assertEqual(mig.without_entry('x\r\n Mods/D3D9.dll \r\ny'),'x\r\ny');self.assertEqual(mig.without_entry('x\n'),'x\n')

@unittest.skipUnless(all((LIVE/n).is_file() for n in ['wow.exe','d3d9.dll','dlls.txt']),'live client files not present')
class RealFilesCopy(unittest.TestCase):
    """Copies of the real files; the live client itself is only read."""
    def test_real_copy_round_trip(self):
        names=[n for n in ['wow.exe','d3d9.dll','dlls.txt','libDllLdr.dll','DivxDecoder.dll','DivxDecoder.dll.bak'] if (LIVE/n).is_file()]
        live={n:sha((LIVE/n).read_bytes()) for n in names}
        with tempfile.TemporaryDirectory() as t:
            client=Path(t)/'game';client.mkdir()
            for n in names:shutil.copy2(LIVE/n,client/n)
            dll=fp.dll()
            if b'PROXY module=%ls root=%ls' not in dll.read_bytes():dll=Path(t)/'proxy.dll';dll.write_bytes(PROXY)
            args=['--client',str(client),'--dll',str(dll),'--backup-root',str(Path(t)/'backups')]
            before=tree(client)
            with patch.object(mig,'running',lambda:[]),redirect_stdout(io.StringIO()):
                mig.main(args);self.assertEqual(tree(client),before)
                mig.main(args+['--apply'])
                self.assertEqual(tree(client)['wow.exe'],before['wow.exe'])
                self.assertEqual(tree(client)['d3d9.dll'],before['d3d9.dll'])
                self.assertEqual(tree(client)['renderer-backends/dxvk/dxvk_d3d9.dll'],before['d3d9.dll'])
                mig.main(args+['--restore','--apply'])
            self.assertEqual(tree(client),before)
        self.assertEqual({n:sha((LIVE/n).read_bytes()) for n in names},live)   # live untouched

if __name__=='__main__':unittest.main()
