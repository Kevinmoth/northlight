#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Tracked terrain lifetime/write revisions, native fake D3D; no game."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import ast,json,subprocess,tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
nodes=ast.parse((HERE/'test_terrain_snapshot.py').read_text()).body
values={n.targets[0].id:ast.literal_eval(n.value) for n in nodes if isinstance(n,ast.Assign) and isinstance(n.targets[0],ast.Name) and n.targets[0].id in ('stub','harness')}
source=values['harness'].split('int main(){')[0]+r'''
#include <chrono>
static uint64_t identities[2]={11,12},revisions[2]={21,22};
static uint64_t identity(void*,bool index){return identities[index];}
static uint64_t version(void*,bool index){return revisions[index];}
int main(){
 Device d;constexpr double origin=17066.666666666666,step=100./3.;
 float x=float(origin-821*step),y=float(origin-548*step);
 d.put(4,{x,y,3});d.put(5,{x+30,y,4});d.put(6,{x,y+30,5});
 FrameCache cache;cache.setIdentityProvider(identity);cache.setVersionProvider(version);
 std::shared_ptr<const MeshSnapshot> a,b;Diagnostics info;
 auto read=[&](FrameCache& c){return c.readMeshShared(&d,D3DPT_TRIANGLELIST,-3,7,3,1,1,d.view,b,&info);};
 assert(read(cache)&&!info.trackedCacheHit);a=b;const auto bytes=cache.bytesRead();assert(bytes>0);
 unsigned vl=d.vb.locks,il=d.ib.locks;cache.clearFrame();
 assert(read(cache)&&info.trackedCacheHit&&a==b&&cache.bytesRead()==0&&cache.avoidedReadBytes()==bytes);
 assert(d.vb.locks==vl&&d.ib.locks==il);
 // Write/DISCARD/NOOVERWRITE revisions invalidate within the SAME frame.
 d.put(4,{x,y,30});++revisions[0];assert(read(cache)&&!info.trackedCacheHit&&b->positions[0].z==30&&a->positions[0].z==3);
 std::uint16_t swapped[]={123,8,7,9};std::memcpy(d.ib.bytes.data(),swapped,sizeof swapped);++revisions[1];
 assert(read(cache)&&!info.trackedCacheHit&&b->positions[0].z==4);
 // Reused pointers with a different lifetime token can never hit old content.
 ++identities[0];vl=d.vb.locks;assert(read(cache)&&!info.trackedCacheHit&&d.vb.locks>vl);
 ++identities[1];il=d.ib.locks;assert(read(cache)&&!info.trackedCacheHit&&d.ib.locks>il);
 // A pending/failed lock or untracked buffer returns revision 0. Full byte
 // fallback catches mutations even when the other buffer remains tracked.
 revisions[0]=0;d.put(5,{x+30,y,40});assert(read(cache)&&!info.trackedCacheHit&&b->positions[0].z==40);
 vl=d.vb.locks;assert(read(cache)&&!info.trackedCacheHit&&d.vb.locks>vl);revisions[0]=29;
 assert(read(cache)&&!info.trackedCacheHit);assert(read(cache)&&info.trackedCacheHit);
 // Missing identity also forbids tracked reuse, including unknown COM buffers.
 identities[1]=0;il=d.ib.locks;assert(read(cache)&&!info.trackedCacheHit&&d.ib.locks>il);identities[1]=90;
 assert(read(cache)&&!info.trackedCacheHit);cache.clearPersistent();assert(cache.trackedHits()==0);assert(read(cache)&&!info.trackedCacheHit);
 // Admission remains identical although fast hits physically read no bytes.
 Limits limits;limits.maxReadBytesPerFrame=bytes;FrameCache bounded(limits);bounded.setIdentityProvider(identity);bounded.setVersionProvider(version);
 assert(read(bounded));bounded.clearFrame();assert(read(bounded)&&info.trackedCacheHit&&bounded.bytesRead()==0);
 assert(!read(bounded)&&info.reason==RejectReason::ByteBudget);
 // Providers are replaceable only by invalidating their proof domain.
 cache.setVersionProvider(nullptr);vl=d.vb.locks;assert(read(cache)&&!info.trackedCacheHit&&d.vb.locks>vl);
 cache.setVersionProvider(version);assert(read(cache));vl=d.vb.locks;il=d.ib.locks;
 const auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<10000;++i){cache.clearFrame();assert(read(cache)&&info.trackedCacheHit);}
 double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/10000;
 assert(d.vb.locks==vl&&d.ib.locks==il&&cache.bytesRead()==0);
 assert(d.vb.refs==1&&d.ib.refs==1&&d.declaration.refs==1&&d.vb.locks==d.vb.unlocks&&d.ib.locks==d.ib.unlocks);
 std::printf("{\"pass\":true,\"warm_draw_us\":%.6f,\"warm_locks\":0,\"warm_read_bytes\":0,\"write_identity_reset_and_unknown_fallback\":true}\n",us);
}
'''
results=[]
with tempfile.TemporaryDirectory(prefix='fr-terrain-tracked-') as temp:
 p=Path(temp);(p/'d3d9.h').write_text(values['stub']);(p/'test.cpp').write_text(source)
 for flags in (['-O2'],['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']):
  subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-I',str(p),*fp.test_include_flags(),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  results.append(json.loads(subprocess.check_output([str(p/'test')],text=True)))
report={'tests':results,'native_fake_D3D':True,'game_launched':False}
(fp.output_dir()/'terrain-tracked-cache-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
