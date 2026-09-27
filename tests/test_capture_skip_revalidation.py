#!/usr/bin/env python3
# northlight-test: requires=cxx
"""0.3.141 capture skip: the model snapshot cache's revalidation slice ((serial&15)==(frame&15))
must reach every entry when captures run only every N frames. Production sets a version provider
(every untracked hit revalidates), so this guards the provider-less fallback: WorldRenderer::endFrame
advances the snapshot frame (clearFrame) on capture frames only; the counterfactual (every frame)
leaves entries unrevalidated indefinitely at N=2. Fake device only; no real D3D/Wine/game."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import ast,subprocess,tempfile,re
HERE=Path(__file__).resolve().parent
def literal(file,key):return next(ast.literal_eval(n.value) for n in ast.parse(file.read_text()).body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==key for t in n.targets))
stub=literal(HERE/'test_terrain_snapshot.py','stub');prefix=literal(HERE/'test_draw_snapshot.py','harness').split('int main()')[0]
source=fp.src('world_renderer.h').read_text()
assert re.search(r'if\(captureMode!=CaptureSkipped\)replaySnapshots\.clearFrame\(\);',source),'endFrame must advance the snapshot frame on capture frames only'
assert source.count('replaySnapshots.clearFrame()')==1
body=r'''
#include <map>
static std::map<void*,std::uint64_t> tokens;
static std::uint64_t identity(void* p,bool){auto it=tokens.find(p);return it==tokens.end()?0:it->second;}
/* Frames 0..; capture on f%N==0. perFrame=true is the counterfactual (clearFrame every frame).
   Returns the largest number of capture frames any of 16 entries needed to see an in-place rewrite
   (1000 = never within 64 capture frames). */
static unsigned run(unsigned N,bool perFrame,unsigned phase){
 Device d;for(unsigned s=0;s<2;++s)d.vb[s].desc.Usage=D3DUSAGE_WRITEONLY;
 tokens.clear();tokens[&d.vb[0]]=1;tokens[&d.vb[1]]=2;tokens[&d.ib]=3;
 constexpr unsigned vertices=64,n=3*32;d.vb[0].bytes.resize(d.offset[0]+vertices*d.stride[0]);d.vb[1].bytes.resize(d.offset[1]+vertices*d.stride[1]);
 for(unsigned s=0;s<2;++s)for(unsigned i=0;i<d.vb[s].bytes.size();++i)d.vb[s].bytes[i]=std::uint8_t(i*31+s);
 d.ib.desc.Usage=0;d.ib.desc.Format=D3DFMT_INDEX16;d.ib.bytes.resize(n*2);for(unsigned i=0;i<n;++i){std::uint16_t x=std::uint16_t(i%vertices);std::memcpy(d.ib.bytes.data()+i*2,&x,2);}
 Frame frame;frame.setIdentityProvider(&identity);Mesh mesh;Diagnostics why;
 std::shared_ptr<const Mesh> original[16],current[16];unsigned seen[16];
 auto readAll=[&](std::shared_ptr<const Mesh>* out){for(unsigned k=0;k<16;++k){Draw draw{D3DPT_TRIANGLELIST,0,0,vertices,0,32-k,true};assert(frame.read(&d,&d.decl,draw,mesh,&why,false,&out[k]));assert(out[k]);}};
 unsigned captures=0,f=0;for(;f<phase;++f)if(perFrame)frame.clearFrame();
 frame.clearFrame();readAll(original);
 d.vb[0].bytes[d.offset[0]+5]^=0x5a; /* vertex 0: referenced by every draw */
 for(auto& s:seen)s=1000;
 for(++f;captures<64;++f){const bool capture=f%N==0;
  if(perFrame||capture)frame.clearFrame();
  if(!capture)continue;++captures;readAll(current);
  for(unsigned k=0;k<16;++k)if(seen[k]==1000&&current[k]!=original[k])seen[k]=captures;}
 unsigned worst=0;for(auto s:seen)worst=std::max(worst,s);return worst;
}
int main(){
 for(unsigned N:{1u,2u,3u,4u,16u})for(unsigned phase=0;phase<16;++phase){const unsigned worst=run(N,false,phase);assert(worst<=16);}
 unsigned stuck=0;for(unsigned phase=0;phase<16;++phase)stuck+=run(2,true,phase)==1000;
 assert(stuck==16); /* per-frame clearFrame with captures every 2nd frame: some entries never revalidate */
 std::printf("capture-frame revalidation: N=1,2,3,4,16 every entry within 16 capture frames; per-frame counterfactual N=2 leaves entries stale in %u/16 phases\n",stuck);
}
'''
with tempfile.TemporaryDirectory(prefix='northlight-skip-revalidation-') as tmp:
 t=Path(tmp);(t/'d3d9.h').write_text(stub);(t/'draw_snapshot.h').write_text(fp.src('draw_snapshot.h').read_text());(t/'test.cpp').write_text(prefix+body)
 for label,flags in [('native',['-O2']),('sanitizer',['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-I',str(t),*fp.test_include_flags(),str(t/'test.cpp'),'-o',str(t/'test')],check=True)
  print(label,subprocess.check_output([str(t/'test')],text=True).strip())
