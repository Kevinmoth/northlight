#!/usr/bin/env python3
# northlight-test: requires=cxx
"""0.3.161 runtime water mask patch (src/water/water_shader_patch.h) == the Python oracle
patch() in renderer/water_shader_patch.py, byte for byte, accept and reject alike: the
synthetic rejection cases of test_water_shaders.py plus a seeded corpus of generated
programs. Also guards that no source carries game bytes again. Native clang++; no client,
StormLib, game, graphics device or Wine."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
import random,re,struct,subprocess,tempfile,unittest
from pathlib import Path
from water_shader_patch import patch,dst,src

HERE=Path(__file__).resolve().parent
def binary(words):return struct.pack('<%dI'%len(words),*words)

def run_patcher(programs,workdir):
    """C++ patch() over byte strings; returns patched bytes or None (rejected) for each."""
    exe=Path(workdir)/'water-shader-patch'
    if not exe.exists():
        subprocess.run(['clang++','-std=c++17','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',
                        *fp.test_include_flags(),str(HERE/'test_water_shader_patch.cpp'),'-o',str(exe)],check=True)
    stream=b''.join(struct.pack('<I',len(p)//4)+p for p in programs)
    (Path(workdir)/'in.bin').write_bytes(stream)
    print(subprocess.check_output([str(exe),str(Path(workdir)/'in.bin'),str(Path(workdir)/'out.bin')],text=True),end='')
    data=(Path(workdir)/'out.bin').read_bytes();results=[];offset=0
    while offset<len(data):
        n,=struct.unpack_from('<I',data,offset);offset+=4
        results.append(data[offset:offset+4*n] if n else None);offset+=4*n
    assert len(results)==len(programs)
    return results

def oracle(code):
    try:return patch(code)[0]
    except Exception:   # ValueError, and IndexError on malformed DCLs: both reject
        return None

def generated(rng):
    """One random program: mostly well-formed SM2/SM3 with DCL/DEF/comments, arithmetic
    (incl. matrix ops and relative addressing), loops and a position/oC0 write; sometimes
    broken in one of the ways patch() must reject."""
    vertex=rng.random()<.6;major=rng.choice((2,3));w=[(0xfffe0000 if vertex else 0xffff0000)|(major<<8)]
    if rng.random()<.02:w[0]=rng.choice((0xfffe0101,0xffff0104,0xfffe0400))
    def ins(op,*tokens,flags=0):w.extend([op|(len(tokens)<<24)|flags,*tokens])
    pos=(4,0) if vertex and major==2 else (8,0) if not vertex else (6,rng.randrange(4))
    temps=32 if major==3 else 12
    if rng.random()<.3:w.extend([0xfffe|(2<<16),0x42415443,rng.getrandbits(32)])
    if vertex and major==3 and rng.random()<.95:ins(31,0x80000000|(rng.random()<.1),dst(*pos))
    for _ in range(rng.randrange(4)):
        usage=rng.choice((5,5,3,10,0))|(rng.randrange(8 if rng.random()<.15 else 4)<<16)
        kind=6 if vertex and major==3 and rng.random()<.5 else 1 if major==3 or vertex else 3
        ins(31,0x80000000|usage,dst(kind,rng.randrange(8)))
    if rng.random()<.5:ins(81,dst(2,rng.randrange(8)),*[rng.getrandbits(32) for _ in range(4)])
    def operand(first):
        if first:
            kind,index=rng.choice([pos,pos,(0,rng.randrange(temps)),(0,rng.randrange(temps))]+([(8,1),(9,0)] if not vertex and rng.random()<.05 else []))
            return [dst(kind,index,rng.choice((15,15,15,7,8,1)))]
        kind=rng.choice((0,0,1,2));t=[src(kind,rng.randrange(temps if kind==0 else 8),rng.choice((0xe4,0xff,0x00,0x55)))]
        if rng.random()<.08:t=[t[0]|0x2000,src(rng.choice((3,15,15,0)),0)]
        return t
    ops=[1,2,4,5,8,19,20,21,22,23,24,6,7,35,66,65,15]
    depth=0
    for _ in range(rng.randrange(1,10)):
        r=rng.random()
        if r<.06 and depth<2:ins(27,src(15,0),src(7,rng.randrange(4)));depth+=1;continue
        if r<.12 and depth:ins(29);depth-=1;continue
        if r<.13:ins(rng.choice((40,41,42,43,95)),src(14,0));continue
        op=rng.choice(ops);n={1:2,2:3,4:4,5:3,8:3,19:2,20:3,21:3,22:3,23:3,24:3,6:2,7:2,35:2,66:3,65:1,15:2}[op]
        tokens=[]
        for k in range(n):tokens+=operand(k==0)
        if rng.random()<.03:tokens.append(src(0,0))
        ins(op,*tokens,flags=0x10000000 if rng.random()<.02 else 0)
    if rng.random()<.1:
        for _ in range(depth):ins(29)
        depth=0
    if rng.random()<.9:ins(1,dst(*pos,rng.choice((15,15,15,3))),src(0,rng.randrange(temps)))
    if depth and rng.random()<.7:
        for _ in range(depth):ins(29)
    if rng.random()<.2:ins(0)
    w.append(0xffff)
    if rng.random()<.03:w=w[:-rng.randrange(1,4)]
    return binary(w)

class RuntimePatch(unittest.TestCase):
    def test_synthetic_rejections_and_samples(self):
        # tests/test_water_shaders.py: unknown control, partial output, loop output, MRT/depth.
        def sample(vertex=True):return [0xfffe0300 if vertex else 0xffff0300,0x0200001f,0x80000000 if vertex else 0x80000005,dst(6 if vertex else 1,0),0x02000001,dst(6 if vertex else 8,0),src(1,0),65535]
        rejects=[[0xfffe0300,0x0200001f,0x80000000,dst(6,0),0x01000028,src(14,0),0x02000001,dst(6,0),src(1,0),65535],
                 [0xfffe0300,0x0200001f,0x80000000,dst(6,0),0x02000001,dst(6,0,7),src(1,0),65535],
                 [0xfffe0300,0x0200001f,0x80000000,dst(6,0),0x0200001b,src(15,0),src(7,0),0x02000001,dst(6,0),src(1,0),0x0000001d,65535]]
        for kind,index in [(8,1),(9,0)]:w=sample(False);w[-1:-1]=[0x02000001,dst(kind,index),src(1,0)];rejects.append(w)
        accepts=[sample(True),sample(False)]
        with tempfile.TemporaryDirectory(prefix='water-patch-') as tmp:
            results=run_patcher([binary(w) for w in rejects+accepts],tmp)
        for code,result in zip(rejects,results):
            self.assertIsNone(oracle(binary(code)));self.assertIsNone(result)
        for code,result in zip(accepts,results[len(rejects):]):
            self.assertIsNotNone(result);self.assertEqual(result,oracle(binary(code)))
    def test_generated_corpus_matches_python(self):
        rng=random.Random(161);programs=[generated(rng) for _ in range(4000)]
        with tempfile.TemporaryDirectory(prefix='water-patch-') as tmp:
            results=run_patcher(programs,tmp)
        accepted=0
        for i,(code,result) in enumerate(zip(programs,results)):
            expected=oracle(code)
            self.assertEqual(result,expected,'program %d: %s'%(i,code.hex()))
            accepted+=expected is not None
        print('generated: %d programs, %d accepted, %d rejected'%(len(programs),accepted,len(programs)-accepted))
        self.assertGreater(accepted,400);self.assertGreater(len(programs)-accepted,400)
    def test_no_game_bytes_in_sources(self):
        # 0.3.161: the DLL carries hashes only. The embedded arrays must not come back.
        banned=re.compile(r'kWaterOriginal_|NorthlightShadowBlobReference|kWaterShaderVariants')
        hits=[str(p.relative_to(fp.REPO)) for d in fp.SRC_DIRS for p in sorted(d.iterdir()) if p.suffix in ('.h','.inl','.cpp','.inc') and banned.search(p.read_text(errors='replace'))]
        self.assertEqual(hits,[])
        self.assertFalse((fp.GAMEDATA/'water_shader_signatures.h').exists()or(fp.GAMEDATA/'shadow_blob_reference.h').exists())
if __name__=='__main__':unittest.main()
