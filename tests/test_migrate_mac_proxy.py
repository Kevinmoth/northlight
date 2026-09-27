#!/usr/bin/env python3
# northlight-test: requires=dll
"""macOS client migration to the dlls.txt-preloaded mods/d3d9.dll proxy
(migrate_mac_proxy.py) on temporary clients: synthetic files, and read-only copies
of this client's real wow.exe, d3d9.dll, frd9.dll and dlls.txt when present
(NORTHLIGHT_LIVE_CLIENT, else the configured client). Dry run writes nothing; apply order,
failure injection at every step, semantic restore (Mod Manager re-sorts, user
removals); any wow.exe is accepted, the only exe write is reverting our old 2-byte
frd9 patch, found by signature. No Wine or game."""
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
sha=lambda b:hashlib.sha256(b).hexdigest()

def tree(root):
    return {p.relative_to(root).as_posix():p.read_bytes() for p in root.rglob('*') if p.is_file()}

class Migration(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();base=Path(self.tmp.name)
        self.client=base/'game';(self.client/'mods').mkdir(parents=True);self.backups=base/'backups'
        self.dll=base/'build.dll';self.dll.write_bytes(PROXY)
        self.original=b'head\0d3d9.dll\0\0\0\0-tail';self.patched=b'head\0frd9.dll\0\0\0\0-tail'
        files={'wow.exe':self.patched,'d3d9.dll':DXVK,'frd9.dll':b'MZ Northlight renderer 0.3.147;','dlls.txt':DLLS,'mods/winerosetta.dll':b'wr',
               'libDllLdr.dll':b'ldr','DivxDecoder.dll':b'divx patched','DivxDecoder.dll.bak':b'divx original'}
        for n,d in files.items():(self.client/n).write_bytes(d)
        self.config=patch.multiple(mig,OFFSET=5,ORIGINAL=sha(self.original),PATCHED=sha(self.patched));self.config.start()
        self.dxvk=patch.object(mig,'WOWSILICON_DXVK',sha(DXVK));self.dxvk.start()
        self.busy=patch.object(mig,'running',lambda:[]);self.busy.start()
        self.reference=base/'reference/wow.exe';self.ref=patch.object(mig,'REFERENCE_EXE',self.reference);self.ref.start()
        self.versions=base/'versions.json';self.ver=patch.object(mig,'VERSIONS',self.versions);self.ver.start()
        # Every file write goes through mig.write: record wow.exe writes; each may only be
        # the revert of our old patch (2 bytes "fr" -> "d3" at OFFSET).
        self.exe_writes=[];real=mig.write
        def spy(path,data):
            if path.name=='wow.exe':
                old=path.read_bytes()
                if not getattr(self,'corrupt_exe_recovery',False):
                    assert len(old)==len(data) and [i for i in range(len(data)) if old[i]!=data[i]]==[5,6] and data[5:14]==b'd3d9.dll\0',(old,data)
                self.exe_writes.append(data)
            return real(path,data)
        self.spy=patch.object(mig,'write',spy);self.spy.start()
    def tearDown(self):
        for p in [self.spy,self.ver,self.ref,self.busy,self.dxvk,self.config]:p.stop()
        if not getattr(self,'corrupt_exe_recovery',False):
            self.assertNotIn(sha(self.patched),[sha(d) for d in self.exe_writes])   # no other code path writes the patched exe
        self.tmp.cleanup()
    def run_main(self,*args):
        out=io.StringIO()
        with redirect_stdout(out):mig.main(['--client',str(self.client),'--dll',str(self.dll),'--backup-root',str(self.backups),*args])
        return out.getvalue()
    def migrated(self):
        return {**self.before,'wow.exe':self.original,'mods/d3d9.dll':PROXY,'renderer-backends/dxvk/dxvk_d3d9.dll':DXVK,
                'northlight-renderer.ini':mig.CONFIG,'dlls.txt':DLLS+b'mods/d3d9.dll\n'}
    def test_dry_run_writes_nothing(self):
        before=tree(self.client);out=self.run_main()
        self.assertIn('Dry run',out);self.assertEqual(tree(self.client),before);self.assertFalse(self.backups.exists())
    def test_apply_order_and_restore(self):
        self.before=tree(self.client);self.run_main('--apply')
        self.assertEqual(tree(self.client),self.migrated())   # d3d9.dll (WoWSilicon DXVK) and frd9.dll untouched
        record_dir=next(self.backups.glob('*'));record=json.loads((record_dir/'transaction.json').read_text())
        self.assertEqual([f['path'] for f in record['files']],[mig.BACKEND,'northlight-renderer.ini',mig.PROXY,'dlls.txt','wow.exe'])
        self.assertEqual((record_dir/'before/wow.exe').read_bytes(),self.patched)   # manual recovery copy only
        self.assertIn('Already migrated',self.run_main())
        self.assertEqual(json.loads(self.run_main('--status'))['proxy_preloaded'],True)
        self.assertIn('Dry run',self.run_main('--restore'));self.assertEqual(tree(self.client),self.migrated())
        self.run_main('--restore','--apply')
        self.assertEqual(tree(self.client),{**self.before,'wow.exe':self.original})   # stops at the ORIGINAL exe
        self.assertFalse((self.client/'renderer-backends').exists())
        self.assertEqual(self.exe_writes,[self.original])
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
        (self.client/'mods/d3d9.dll').unlink();self.assertEqual(tree(self.client),{**self.before,'wow.exe':self.original,'northlight-renderer.ini':b'[Renderer]\r\nBackend=native\r\n'})
    def test_restore_brings_back_previous_ini(self):
        (self.client/'northlight-renderer.ini').write_bytes(b'[Renderer]\r\nBackend=dxvk\r\n');self.before=tree(self.client)
        self.run_main('--apply');self.run_main('--restore','--apply')
        self.assertEqual(tree(self.client),{**self.before,'wow.exe':self.original})
    def test_failure_at_every_step_rolls_back(self):
        self.before=tree(self.client);real=mig.write
        for failing in range(1,6):
            with self.subTest(failing=failing):
                count=[0]
                def fail(path,data):
                    count[0]+=1
                    if count[0]==failing:raise OSError('simulated disk failure')
                    return real(path,data)
                with patch.object(mig,'write',fail):
                    with self.assertRaises(OSError):self.run_main('--apply')
                self.assertEqual(tree(self.client),self.before)   # exe step is last: still patched, nothing else left
                shutil.rmtree(self.backups)
    def test_failure_after_exe_keeps_original_exe(self):
        self.before=tree(self.client);real=m.atomic_json
        def fail(path,value):
            if value['status']=='installed':raise OSError('simulated failure after the last step')
            return real(path,value)
        with patch.object(m,'atomic_json',fail):
            with self.assertRaises(OSError):self.run_main('--apply')
        self.assertEqual(tree(self.client),{**self.before,'wow.exe':self.original})
    def test_exe_failing_verification_is_recovered(self):
        self.before=tree(self.client);self.corrupt_exe_recovery=True;real=mig.write
        def corrupt(path,data):return real(path,b'torn write' if path.name=='wow.exe' and data==self.original else data)
        with patch.object(mig,'write',corrupt):
            with self.assertRaises(ValueError):self.run_main('--apply')
        self.assertEqual(tree(self.client),self.before)   # verified backup (the previous exe) put back; nothing else left
    def test_preload_refusal_explains(self):
        (self.client/'libDllLdr.dll').unlink()
        with self.assertRaises(ValueError) as refused:self.run_main('--apply')
        self.assertIn('preload is not active',str(refused.exception));self.assertIn('nothing changed',str(refused.exception))
    def test_already_original_exe_is_not_written(self):
        (self.client/'wow.exe').write_bytes(self.original);self.before=tree(self.client)
        self.run_main('--apply');self.assertEqual(self.exe_writes,[])
        self.assertEqual(tree(self.client),self.migrated())
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
        self.dll.write_bytes(b'MZ Northlight renderer 0.3.147; frd9 without the proxy loader')
        with self.assertRaises(ValueError):self.run_main('--apply')
    def old_record(self,data,client=None,status='installed'):
        # A migration record from before a rename: its mods/d3d9.dll carries no name that is matched today.
        d=self.backups/'20260101-000000-old';d.mkdir(parents=True,exist_ok=True)
        (d/'transaction.json').write_text(json.dumps({'kind':mig.KIND,'version':'0.3.165','client':client or str(self.client.resolve()),'status':status,
            'files':[{'path':mig.PROXY,'before':None,'after':sha(data)}]}))
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
    def test_reference_exe_is_informational(self):
        self.reference.parent.mkdir();self.reference.write_bytes(b'a different reference exe');ref=self.reference.stat()
        self.assertIn('differs after migration (informational)',self.run_main())
        self.run_main('--apply');self.assertEqual((self.client/'wow.exe').read_bytes(),self.original)   # never refused
        self.assertEqual((self.reference.read_bytes(),self.reference.stat().st_mtime_ns),(b'a different reference exe',ref.st_mtime_ns))   # only read
        self.reference.write_bytes(self.original);self.assertIn('matches after migration',json.loads(self.run_main('--status'))['reference_exe'])
    def test_unknown_exe_without_signature_untouched(self):
        for exe in [b'unknown build',b'LAA\0d3d9.dll\0\0\0\0 other bytes']:
            with self.subTest(exe=exe):
                (self.client/'wow.exe').write_bytes(exe);self.before=tree(self.client)
                self.assertIn('other build (accepted)',self.run_main());self.run_main('--apply')
                record_dir=sorted(self.backups.glob('*'))[-1]
                self.assertNotIn('wow.exe',[f['path'] for f in json.loads((record_dir/'transaction.json').read_text())['files']])
                self.assertEqual(tree(self.client),{**self.migrated(),'wow.exe':exe});self.assertEqual(self.exe_writes,[])
                self.run_main('--restore','--apply');self.assertEqual(tree(self.client),self.before)
    def test_unknown_exe_with_signature_gets_two_byte_revert(self):
        exe=b'LAA\0\0frd9.dll\0\0\0\0 other build';(self.client/'wow.exe').write_bytes(exe);self.before=tree(self.client)
        self.run_main('--apply')
        self.assertEqual(self.exe_writes,[b'LAA\0\0d3d9.dll\0\0\0\0 other build'])
        self.run_main('--restore','--apply');self.assertEqual(tree(self.client),{**self.before,'wow.exe':self.exe_writes[0]})
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
        self.assertEqual((self.client/'wow.exe').read_bytes(),self.patched)
    def test_windows_restore_refuses_migration_record(self):
        self.run_main('--apply');record_dir=next(self.backups.glob('*'))
        with self.assertRaises(ValueError):m.restore(self.client,record_dir)
        self.assertEqual((self.client/'wow.exe').read_bytes(),self.original)
    def test_revert_by_signature_only(self):
        self.assertIsNone(mig.revert_exe(self.original));self.assertEqual(mig.revert_exe(self.patched),self.original)
        self.assertIsNone(mig.revert_exe(b'unknown'));self.assertIsNone(mig.revert_exe(b''))
        other=b'OTHR\0frd9.dll\0\0\0\0 a different build'   # unknown hash, our signature: 2-byte revert
        self.assertEqual(mig.revert_exe(other),b'OTHR\0d3d9.dll\0\0\0\0 a different build')
        for near in [b'OTHRXfrd9.dll\0\0\0\0rest',b'OTHR\0frd9.dll\0X\0\0rest',b'OTHR\0frd9.dllX\0\0\0rest']:
            self.assertIsNone(mig.revert_exe(near))   # not the same .rdata string: untouched
        self.assertIn('other build (accepted)',mig.exe_note(b'unknown'))
    def test_dlls_entry_helpers(self):
        self.assertEqual(mig.with_entry('a.dll'),'a.dll\nmods/d3d9.dll\n');self.assertEqual(mig.with_entry(''),'mods/d3d9.dll\n')
        self.assertEqual(mig.with_entry('MODS\\D3D9.DLL\r\n'),'MODS\\D3D9.DLL\r\n')
        self.assertEqual(mig.without_entry('x\r\n Mods/D3D9.dll \r\ny'),'x\r\ny');self.assertEqual(mig.without_entry('x\n'),'x\n')

@unittest.skipUnless(all((LIVE/n).is_file() for n in ['wow.exe','d3d9.dll','dlls.txt']),'live client files not present')
class RealFilesCopy(unittest.TestCase):
    """Copies of the real files; the live client itself is only read."""
    def test_real_copy_round_trip(self):
        names=[n for n in ['wow.exe','d3d9.dll','frd9.dll','dlls.txt','libDllLdr.dll','DivxDecoder.dll','DivxDecoder.dll.bak'] if (LIVE/n).is_file()]
        live={n:sha((LIVE/n).read_bytes()) for n in names}
        reference=mig.REFERENCE_EXE;ref_state=(sha(reference.read_bytes()),reference.stat().st_mtime_ns) if reference and reference.is_file() else None
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
                self.assertEqual(sha((client/'wow.exe').read_bytes()),mig.ORIGINAL)
                if ref_state:self.assertEqual((client/'wow.exe').read_bytes(),reference.read_bytes())   # approved reference, read-only
                self.assertEqual(tree(client)['d3d9.dll'],before['d3d9.dll'])
                self.assertEqual(tree(client)['renderer-backends/dxvk/dxvk_d3d9.dll'],before['d3d9.dll'])
                mig.main(args+['--restore','--apply'])
            restored=tree(client);self.assertEqual(sha(restored.pop('wow.exe')),mig.ORIGINAL)
            self.assertEqual(restored,{k:v for k,v in before.items() if k!='wow.exe'})
        self.assertEqual({n:sha((LIVE/n).read_bytes()) for n in names},live)   # live untouched
        if ref_state:self.assertEqual((sha(reference.read_bytes()),reference.stat().st_mtime_ns),ref_state)   # other client untouched

if __name__=='__main__':unittest.main()
