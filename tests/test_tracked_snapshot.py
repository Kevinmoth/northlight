#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native/fake-D3D regression; no game or GPU execution."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import ast,subprocess,tempfile
HERE=Path(__file__).resolve().parent
def literal(file,key):return next(ast.literal_eval(n.value) for n in ast.parse(file.read_text()).body if isinstance(n,ast.Assign) and any(getattr(t,'id','')==key for t in n.targets))
stub=literal(HERE/'test_terrain_snapshot.py','stub')
prefix=literal(HERE/'test_draw_snapshot.py','harness').split('int main()')[0]
body=r'''
#include <map>
static std::map<void*,uint64_t> revisions;
static uint64_t identity(void* p,bool){return uint64_t(reinterpret_cast<uintptr_t>(p));}
static uint64_t version(void* p,bool){return revisions[p];}
static unsigned locks(Device& d){return d.vb[0].locks+d.vb[1].locks+d.ib.locks;}
int main(){
 Device d;d.indices({7,9,8});revisions[&d.vb[0]]=1;revisions[&d.vb[1]]=2;revisions[&d.ib]=3;
 Frame frame;frame.setIdentityProvider(identity);frame.setVersionProvider(version);
 Mesh scratch;Diagnostics why;std::shared_ptr<const Mesh> first,next;
 Draw draw{D3DPT_TRIANGLELIST,0,0,12,0,1,true};
 assert(frame.read(&d,&d.decl,draw,scratch,&why,false,&first)&&first&&first->dynamic);
 auto charge=frame.bytesRead();unsigned before=locks(d);
 for(int i=0;i<32;++i){frame.clearFrame();assert(frame.read(&d,&d.decl,draw,scratch,&why,false,&next));assert(next==first&&locks(d)==before&&frame.trackedHits()==1&&frame.avoidedReadBytes()==charge&&frame.bytesRead()==charge);}
 // Vertex edit: no cached version can survive a write, old snapshots remain immutable.
 auto bytes=first->streams[1].bytes;d.vb[1].bytes[d.offset[1]+7*d.stride[1]]^=0x55;revisions[&d.vb[1]]=4;
 frame.clearFrame();assert(frame.read(&d,&d.decl,draw,scratch,&why,false,&next));assert(next!=first&&next->streams[1].bytes!=bytes&&first->streams[1].bytes==bytes);
 Mesh direct;Frame reference;assert(reference.read(&d,&d.decl,draw,direct,&why));assert(direct.indices==next->indices);for(int s=0;s<4;++s)assert(direct.streams[s].bytes==next->streams[s].bytes);
 // Index write changes compaction/winding immediately.
 d.indices({8,9,8});revisions[&d.ib]=5;frame.clearFrame();assert(frame.read(&d,&d.decl,draw,scratch,&why,false,&next));assert(next->vertexCount==2&&next->indices==std::vector<uint32_t>({0,1,0}));
 // Unknown or currently locked stream must use old capture even with tracked peers.
 revisions[&d.vb[0]]=0;frame.clearFrame();before=locks(d);assert(frame.read(&d,&d.decl,draw,scratch,&why,false,&next));assert(!next&&locks(d)>before&&frame.trackedHits()==0);
 // Reset/identity generation change; compare unchanged geometry, not just metadata.
 revisions[&d.vb[0]]=6;revisions[&d.ib]=7;frame.clearFrame();assert(frame.read(&d,&d.decl,draw,scratch,&why,false,&next)&&next);
 // Identical hit admission: caching never expands the accepted caster budget.
 Frame limited(charge,0);limited.setIdentityProvider(identity);limited.setVersionProvider(version);
 assert(limited.read(&d,&d.decl,draw,scratch,&why,false,&next));assert(!limited.read(&d,&d.decl,draw,scratch,&why,false,&next)&&why.error==Error::Budget);
 limited.clearFrame();assert(limited.read(&d,&d.decl,draw,scratch,&why,false,&next)&&limited.trackedHits()==1);
 // Untracked static resources retain byte verification on EVERY hit when
 // write tracking is installed; unknown writes cannot become stale GPU entries.
 revisions.clear();d.vb[0].desc.Usage=d.vb[1].desc.Usage=d.ib.desc.Usage=0;
 frame.clearFrame();assert(frame.read(&d,&d.decl,draw,scratch,&why,false,&first)&&first);
 d.vb[0].bytes[d.offset[0]+8*d.stride[0]+1]^=0x77;
 frame.clearFrame();assert(frame.read(&d,&d.decl,draw,scratch,&why,false,&next)&&next!=first&&frame.snapshotRevalidationMismatches()==1);
 puts("PASS tracked dynamic generations: zero hit locks, VB/IB rewrites, immutable snapshots, unknown/locked fallback, equal admission");
}
'''
with tempfile.TemporaryDirectory(prefix='northlight-tracked-snapshot-') as tmp:
 p=Path(tmp);(p/'d3d9.h').write_text(stub);(p/'test.cpp').write_text(prefix+body)
 for flags in [[],['-fsanitize=address,undefined','-fno-omit-frame-pointer']]:
  subprocess.run(['clang++','-std=c++17','-O2',*flags,'-I'+str(p),*fp.test_include_flags(),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
