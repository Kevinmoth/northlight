#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Inject allocation failure during snapshot remap replacement; no D3D runtime."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import ast,subprocess,tempfile
HERE=Path(__file__).resolve().parent
def literal(file,name):
 return next(ast.literal_eval(n.value) for n in ast.parse(file.read_text()).body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets))
stub=literal(HERE/'test_terrain_snapshot.py','stub')
prefix=literal(HERE/'test_draw_snapshot.py','harness').split('int main()')[0]
body=r'''
#include <new>
#include <cstdlib>
static long failAfter=-1;
void* operator new(std::size_t size){if(failAfter==0)throw std::bad_alloc();if(failAfter>0)--failAfter;void* p=std::malloc(size?size:1);if(!p)throw std::bad_alloc();return p;}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(){unsigned failures=0;
 for(long point=0;point<24;++point){Device device;Frame frame;Mesh mesh;Draw draw{D3DPT_TRIANGLELIST,0,0,10,0,3,true};
  device.indices({0,0,1,1,0,1,0,1,0});assert(frame.read(&device,&device.decl,draw,mesh));
  device.indices({0,1,2,3,4,5,6,7,9});failAfter=point;
  try{assert(frame.read(&device,&device.decl,draw,mesh));}catch(const std::bad_alloc&){++failures;}
  failAfter=-1;frame.clearFrame();assert(frame.read(&device,&device.decl,draw,mesh));
  const unsigned original[]={0,1,2,3,4,5,6,7,9};assert(mesh.vertexCount==9&&mesh.indices.size()==9);
  for(unsigned i=0;i<9;++i)for(unsigned stream=0;stream<2;++stream)for(unsigned byte=0;byte<(stream?4u:16u);++byte)
   assert(mesh.streams[stream].bytes[mesh.indices[i]*device.stride[stream]+byte]==device.vb[stream].bytes[device.offset[stream]+original[i]*device.stride[stream]+byte]);
  assert(device.ib.refs==1&&device.ib.locks==device.ib.unlocks);for(auto& vb:device.vb)assert(vb.refs==1&&vb.locks==vb.unlocks);
 }
 assert(failures>=4);std::printf("allocation recovery passed: %u injected failures, 24 checkpoints\n",failures);
}
'''
with tempfile.TemporaryDirectory(prefix='northlight-snapshot-allocation-') as temp:
 p=Path(temp);(p/'d3d9.h').write_text(stub);(p/'test.cpp').write_text(prefix+body)
 for flags in (['-O2'],['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']):
  subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-I',str(p),*fp.test_include_flags(),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
