#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Offline 200-draw working-set comparison; same code with 2/16 MiB budgets."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import ast,json,subprocess,tempfile
HERE=Path(__file__).resolve().parent
def literal(file,key):return next(ast.literal_eval(n.value) for n in ast.parse(file.read_text()).body if isinstance(n,ast.Assign) and any(getattr(t,'id','')==key for t in n.targets))
stub=literal(HERE/'test_terrain_snapshot.py','stub')
prefix=literal(HERE/'test_draw_snapshot.py','harness').split('int main()')[0]
body=r'''
#include <chrono>
int main(){Device d;constexpr unsigned draws=200,indices=12000,vertices=4096;
 for(unsigned s=0;s<2;++s)d.vb[s].bytes.resize(d.offset[s]+vertices*d.stride[s]);
 d.ib.desc.Format=D3DFMT_INDEX16;d.ib.bytes.resize(size_t(draws)*indices*2);
 for(unsigned j=0;j<draws;++j)for(unsigned i=0;i<indices;++i){uint16_t v=(i+j*17)%vertices;memcpy(d.ib.bytes.data()+(size_t(j)*indices+i)*2,&v,2);}
 Frame frame;Mesh m;Diagnostics why;double elapsed=0;uint64_t checksum=0;unsigned hits=0,misses=0;
 for(unsigned f=0;f<10;++f){frame.clearFrame();auto start=std::chrono::steady_clock::now();
  for(unsigned j=0;j<draws;++j){Draw draw{D3DPT_TRIANGLELIST,0,0,vertices,j*indices,indices/3,true};assert(frame.read(&d,&d.decl,draw,m,&why,true));assert(m.vertexCount==vertices);checksum+=m.indices[j];}
  if(f>0){elapsed+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();hits+=frame.indexCacheHits();misses+=frame.indexCacheMisses();}}
 printf("{\"msPerFrame\":%.4f,\"warmHits\":%u,\"warmMisses\":%u,\"residentBytes\":%zu,\"checksum\":%llu}\n",elapsed/9,hits,misses,frame.indexCacheBytes(),(unsigned long long)checksum);
}
'''
results={}
with tempfile.TemporaryDirectory(prefix='northlight-index-working-set-') as temp:
 p=Path(temp);(p/'d3d9.h').write_text(stub);(p/'test.cpp').write_text(prefix+body)
 for budget in [2,16]:
  source=fp.src('draw_snapshot.h').read_text();assert source.count('IndexCacheLimit=16u*1024u*1024u')==1
  header=source.replace('IndexCacheLimit=16u*1024u*1024u',f'IndexCacheLimit={budget}u*1024u*1024u')
  (p/'draw_snapshot.h').write_text(header)
  subprocess.run(['clang++','-std=c++17','-O2','-I'+str(p),*fp.test_include_flags(),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  results[str(budget)]=json.loads(subprocess.check_output([str(p/'test')],text=True))
assert results['16']['warmMisses']==0 and results['2']['warmMisses']>0
assert results['16']['checksum']==results['2']['checksum']
print(json.dumps(results,indent=2))
