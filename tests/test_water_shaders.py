#!/usr/bin/env python3
# northlight-test: requires=cxx,client,stormlib
"""Offline bytecode/dataflow regression: every original instruction is preserved.
0.3.161: the runtime C++ patch of all 36 client originals is byte-identical to Python
patch() and lands on the generated patched hash and word count, i.e. on the exact
words 0.3.160 embedded. No game, graphics device, or Wine process is started by this test.
"""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
import json,re,struct,tempfile,unittest
from pathlib import Path
from extract_water_shaders import HERE,Archive,variants,fnv1a64,patch,parse,register,reg,dst,src,accepted_originals
from test_water_shader_patch import run_patcher

def binary(words):return struct.pack('<%dI'%len(words),*words)
def sample(vertex=True):
    return binary([0xfffe0300 if vertex else 0xffff0300,0x0200001f,0x80000000 if vertex else 0x80000005,dst(6 if vertex else 1,0),0x02000001,dst(6 if vertex else 8,0),src(1,0),65535])
class Patching(unittest.TestCase):
    def test_original_corpus(self):
        report=json.loads((HERE/'water-shader-identities.json').read_text());archives={};count=0;loopCount=0
        try:
            for record in report['accepted']:
                source=record['sources'][0];path=source['archive']
                if path not in archives:archives[path]=Archive(fp.client_root()/path).__enter__()
                original=next(code for index,_,code in variants(archives[path].read(source['path'])) if index==source['variant'])
                self.assertEqual(fnv1a64(original),int(record['hash'],16))
                result,info=patch(original);ow,ops,_=parse(original);rw,rops,_=parse(result)
                self.assertEqual(info['loops'],record['loops']);loopCount+=info['loops']
                # RG32 mask alpha lives in G, while A retains native alpha-test input.
                self.assertEqual((rw[-5]>>16)&255,0xe4 if info['vertex'] else 0xff)
                # Remove the one injected declaration, restore only redirected
                # output register tokens, and compare the entire byte stream.
                extra=info['model']==3 or not info['vertex']
                if extra:
                    candidates=[(p,a) for p,op,a in rops if op==31 and register(a[1])==((6 if info['vertex'] else (1 if info['model']==3 else 3)),info['varying'])]
                    self.assertEqual(len(candidates),1);p,_=candidates[0];del rw[p:p+3]
                del rw[-7:-1]
                self.assertEqual(len(rw),len(ow))
                for i,(a,b) in enumerate(zip(ow,rw)):
                    if a!=b:
                        self.assertEqual(register(b),(0,info['temp']))
                        self.assertEqual(b,reg(a,0,info['temp']))
                # The full original alpha expression, kills, loop constants and
                # relative-addressing operands remain byte-identical above.
                count+=1
        finally:
            for archive in archives.values():archive.__exit__(None,None,None)
        self.assertEqual(count,36);self.assertGreaterEqual(loopCount,12)
    def test_runtime_patch_identity(self):
        table={m[0]:(int(m[1],16),int(m[2]),m[3]=='true') for m in re.findall(r'\{UINT64_C\(0x([0-9a-f]{16})\), UINT64_C\(0x([0-9a-f]{16})\), (\d+), (true|false)\}',fp.src('water_shader_identities.h').read_text())}
        records,originals=zip(*accepted_originals())
        with tempfile.TemporaryDirectory(prefix='water-identity-') as tmp:
            results=run_patcher(list(originals),tmp)
        self.assertEqual(len(table),36);self.assertEqual(sorted(table),sorted(r['hash'] for r in records))
        for record,original,result in zip(records,originals,results):
            patchedHash,words,vertex=table[record['hash']]
            self.assertIsNotNone(result,record['hash'])
            self.assertEqual(result,patch(original)[0])
            self.assertEqual((fnv1a64(result),len(result)//4,vertex),(patchedHash,words,record['vertex']))
            self.assertEqual('%016x'%patchedHash,record['patched_fnv1a64'])
    def test_unknown_control_and_partial_output_rejected(self):
        for words in ([0xfffe0300,0x0200001f,0x80000000,dst(6,0),0x01000028,src(14,0),0x02000001,dst(6,0),src(1,0),65535],
                      [0xfffe0300,0x0200001f,0x80000000,dst(6,0),0x02000001,dst(6,0,7),src(1,0),65535]):
            with self.assertRaises(ValueError):patch(binary(words))
    def test_loop_output_cannot_bypass_final_position(self):
        code=binary([0xfffe0300,0x0200001f,0x80000000,dst(6,0),0x0200001b,src(15,0),src(7,0),0x02000001,dst(6,0),src(1,0),0x0000001d,65535])
        with self.assertRaises(ValueError):patch(code)
    def test_mrt_and_depth_side_effect_rejected(self):
        for kind,index in [(8,1),(9,0)]:
            w=list(struct.unpack('<%dI'%(len(sample(False))//4),sample(False)))
            w[-1:-1]=[0x02000001,dst(kind,index),src(1,0)]
            with self.assertRaises(ValueError):patch(binary(w))
    def test_magma_never_classified(self):
        report=json.loads((HERE/'water-shader-identities.json').read_text())
        self.assertTrue(all('magma' not in s['path'].lower() and '_editor' not in s['path'].lower() for r in report['accepted'] for s in r['sources']))
    def test_water_depth_interpolation(self):
        # Perspective-correct varying clip.w reconstructs surface view depth;
        # linearly interpolating view depth in screen coordinates would fail.
        bary=[.2,.3,.5];zs=[2.,5.,20.]
        correct=1/sum(b/z for b,z in zip(bary,zs))
        varying=sum(b*z/z for b,z in zip(bary,zs))/sum(b/z for b,z in zip(bary,zs))
        self.assertAlmostEqual(varying,correct);self.assertGreater(abs(varying-sum(b*z for b,z in zip(bary,zs))),5)
if __name__=='__main__':unittest.main()
