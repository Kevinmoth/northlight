#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native/fake-D3D correctness tests for 0.3.132. Never launches game/Wine."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import client_fixtures  # the real client programs, from the tester's client
from pathlib import Path
import ast,json,subprocess,tempfile
HERE=Path(__file__).resolve().parent
OUT=fp.output_dir()
def literal(name,key):return next(ast.literal_eval(n.value) for n in ast.parse(fp.tracked(name).read_text()).body if isinstance(n,ast.Assign) and any(getattr(t,'id','')==key for t in n.targets))
stub=literal('test_terrain_snapshot.py','stub')
prefix=literal('test_draw_snapshot.py','harness').split('int main()')[0]
snapshot=prefix+r'''
static uint64_t token=1,revision=1;
static uint64_t identity(void* p,bool){return uint64_t(reinterpret_cast<uintptr_t>(p))+token*100;}
static uint64_t version(void*,bool){return revision;}
int main(){
 Device d;Frame frame;frame.setIdentityProvider(identity);frame.setVersionProvider(version);
 // Five ~15 MiB immutable snapshots force eviction. An explicitly touched
 // older snapshot must survive, the true oldest must miss, and outside shared
 // owners must remain byte-exact even after eviction and resource writes.
 const unsigned count=120000;d.stride[0]=128;d.stride[1]=8;
 for(unsigned s=0;s<2;++s)d.vb[s].bytes.resize(d.offset[s]+count*d.stride[s]);
 d.ib.desc.Format=D3DFMT_INDEX32;d.ib.bytes.resize(count*4);
 for(unsigned i=0;i<count;++i)std::memcpy(d.ib.bytes.data()+i*4,&i,4);
 Draw draw{D3DPT_TRIANGLELIST,0,0,count,0,count/3,true};Mesh scratch;Diagnostics why;
 std::shared_ptr<const Mesh> keep[5],next;
 for(token=1;token<=3;++token){frame.clearFrame();assert(frame.read(&d,&d.decl,draw,scratch,&why,true,&keep[token-1]));}
 token=1;frame.clearFrame();assert(frame.read(&d,&d.decl,draw,scratch,&why,true,&next)&&next==keep[0]);
 token=4;frame.clearFrame();frame.sampleMaintenance(true);assert(frame.read(&d,&d.decl,draw,scratch,&why,true,&keep[3]));assert(frame.maintenance().snapshotEvictions>0);
 token=1;frame.clearFrame();assert(frame.read(&d,&d.decl,draw,scratch,&why,true,&next)&&next==keep[0]);
 token=2;frame.clearFrame();frame.sampleMaintenance(true);assert(frame.read(&d,&d.decl,draw,scratch,&why,true,&next)&&next!=keep[1]);
 assert(frame.maintenance().missEvicted==1&&frame.snapshotCacheMisses()==1);
 assert(next->indices==keep[1]->indices&&next->streams[0].bytes==keep[1]->streams[0].bytes);
 auto before=next;++revision;d.vb[0].bytes[d.offset[0]+4]^=91;frame.clearFrame();frame.sampleMaintenance(true);
 assert(frame.read(&d,&d.decl,draw,scratch,&why,true,&next)&&next!=before&&frame.maintenance().missRevision==1);
 assert(next->streams[0].bytes!=before->streams[0].bytes&&before->streams[0].bytes==keep[1]->streams[0].bytes);
 assert(frame.snapshotCacheBytes()<=Frame::SnapshotCacheLimit);
 frame.clearIndexCache();assert(!frame.snapshotCacheEntries()&&!frame.indexCacheEntries());
 // Index plans: count pressure, exact least-recently-used victim, freed-slot
 // reuse, compaction-run lifetime and repeated reset/refill.
 Device small;small.indices({7,9,8});Frame plans;Draw part{D3DPT_TRIANGLELIST,0,0,12,0,1,true};
 for(unsigned cycle=0;cycle<3;++cycle){
  for(unsigned i=0;i<1024;++i){plans.clearFrame();part.vertices=12+i;assert(plans.read(&small,&small.decl,part,scratch,&why,true));}
  assert(plans.indexCacheEntries()==1024);
  plans.clearFrame();part.vertices=12;assert(plans.read(&small,&small.decl,part,scratch,&why,true)&&plans.indexCacheHits()==1);
  plans.clearFrame();part.vertices=1036;assert(plans.read(&small,&small.decl,part,scratch,&why,true)&&plans.maintenance().indexEvictions==1);
  plans.clearFrame();part.vertices=12;assert(plans.read(&small,&small.decl,part,scratch,&why,true)&&plans.indexCacheHits()==1);
  plans.clearFrame();part.vertices=13;assert(plans.read(&small,&small.decl,part,scratch,&why,true)&&plans.indexCacheMisses()==1);
  assert(scratch.vertexCount==3&&scratch.indices==std::vector<uint32_t>({0,2,1}));
  plans.clearIndexCache();assert(!plans.indexCacheEntries());
 }
 puts("PASS snapshot/index LRU victims, capacity, revisions, immutable held owners, history, reset and compaction lifetime");
}
'''
actor=literal('test_actor_deformation.py','harness').replace('#include "actor_deformation.h"','#include "actor_deformation.h"\n#include "sampled_vertex_cache.h"\n#include "replay_shadow_policy.h"')
needle='  // Rotate camera and corresponding bone-to-view matrices: world pose is invariant.'
extra=r'''
  {SampledVertexCache cache;auto owned=std::make_shared<NorthlightDrawSnapshot::Mesh>(mesh);float camera[]={1000,-20,0};
   std::vector<NorthlightReplayShadowPolicy::Candidate> a,b;
   for(unsigned i=0;i<2500;++i){float varied[1024];std::memcpy(varied,constants,sizeof varied);varied[34*4+3]+=float(i%79)*.25f;camera[1]=float(i%23)-20;
    float expected=-1,actual=-2;assert(sampledDistanceSquared(p,*owned,decl,3,varied,inverse,camera,expected));
    assert(cache.distance(p,*owned,owned,decl,decl,3,varied,inverse,camera,actual));assert(std::memcmp(&expected,&actual,4)==0);
    a.push_back({i,1000+(i%17)*17,expected,true,true});b.push_back({i,1000+(i%17)*17,actual,true,true});}
   assert(cache.hits==2499&&cache.misses==1);
   NorthlightReplayShadowPolicy::choose(a,512000);NorthlightReplayShadowPolicy::choose(b,512000);
   for(size_t i=0;i<a.size();++i)assert(a[i].index==b[i].index&&a[i].keep==b[i].keep);
   // Same declaration pointer with changed metadata must reject cached inputs.
   auto changed=std::make_shared<NorthlightDrawSnapshot::Mesh>(*owned);float x=9;std::memcpy(changed->streams[0].bytes.data(),&x,4);
   float e=0,v=0;assert(sampledDistanceSquared(p,*changed,decl,3,constants,inverse,camera,e));assert(cache.distance(p,*changed,changed,decl,decl,3,constants,inverse,camera,v)&&e==v);
   auto saved=decl[0];decl[0].Offset=12;assert(!cache.distance(p,*owned,owned,decl,decl,3,constants,inverse,camera,v));decl[0]=saved;
   // Program identity reuse with a changed input mapping must not reuse old values.
   auto original=p.inputs;p.inputs[0].usage=15;assert(!cache.distance(p,*owned,owned,decl,decl,3,constants,inverse,camera,v));p.inputs=original;
   // Mutable/unowned geometry always goes through the immediate decoder.
   assert(cache.distance(p,mesh,{},decl,decl,3,constants,inverse,camera,v));
   assert(sampledDistanceSquared(p,mesh,decl,3,constants,inverse,camera,e)&&e==v);
   cache.clear();assert(!cache.hits&&!cache.misses);
  }
'''
assert needle in actor;actor=actor.replace(needle,extra+needle)
math=(HERE/'test_world_math.cpp').read_text()
needle='    std::puts("PASS cached static shadow map texel offset, depth delta and direction quantization");'
extra=r'''
    for(float radius:{48.f,192.f}){
        auto anchor=NorthlightWorldMath::shadowFrame(center,sun,radius);
        for(long dx:{-129L,-128L,-3L,0L,127L,128L,129L})for(long dy:{-128L,0L,128L}){
            auto current=anchor;current.cx+=dx*anchor.texel;current.cy+=dy*anchor.texel;current.cz+=12.5f;
            auto keep=NorthlightWorldMath::shadowCachePlacement(current,anchor,true,true,128);
            if(std::labs(dx)<=128){assert(!keep.reason&&keep.frame.cx==anchor.cx&&keep.frame.cy==anchor.cy&&keep.frame.cz==anchor.cz);
                assert(keep.offX==dx&&keep.offY==dy&&keep.dz==12.5f/NorthlightWorldMath::ShadowDepthSpan);
                // Repeated content-only refresh keeps the original frame and the
                // exact same union offset/depth correction, including at edges.
                for(unsigned i=0;i<10;++i){auto again=NorthlightWorldMath::shadowCachePlacement(current,keep.frame,true,true,128);
                    assert(!again.reason&&again.offX==keep.offX&&again.offY==keep.offY&&again.dz==keep.dz);}
            }else{assert(std::strcmp(keep.reason,"travel")==0&&keep.frame.cx==current.cx&&!keep.offX&&!keep.offY&&keep.dz==0);}
            auto invalid=NorthlightWorldMath::shadowCachePlacement(current,anchor,false,true,128);
            assert(std::strcmp(invalid.reason,"invalid")==0&&!invalid.offX&&!invalid.offY&&invalid.dz==0);
            auto direction=NorthlightWorldMath::shadowCachePlacement(current,anchor,true,false,128);assert(std::strcmp(direction.reason,"direction")==0&&direction.frame.cx==current.cx);
        }
        auto fractional=anchor;fractional.cx+=anchor.texel*.25f;
        assert(std::strcmp(NorthlightWorldMath::shadowCachePlacement(fractional,anchor,true,true,128).reason,"frame")==0);
    }
    std::puts("PASS content refresh anchor retention, near/far edges, travel, direction, invalidation and fractional-grid fallback");
'''
assert needle in math;math=math.replace(needle,extra+needle)
report=[]
with tempfile.TemporaryDirectory(prefix='northlight-cache-shadow-')as tmp:
 p=Path(tmp);(p/'d3d9.h').write_text(stub)
 for name,source in [('snapshot',snapshot),('distance',actor),('anchor',math)]:
  code=p/(name+'.cpp');code.write_text(source)
  for label,flags in [('O2',['-O2']),('ASan+UBSan',['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
   exe=p/name;command=['clang++','-std=c++17',*flags,'-I'+str(p),*fp.test_include_flags(),str(code)]
   if name=='anchor':command+=[str(fp.src('world_gi.cpp'))]
   subprocess.run(command+['-o',str(exe)],check=True)
   args=[str(exe)]
   if name=='distance' and fp.shader_corpus() and client_fixtures.shaders_available():args+=[str(fp.shader_corpus()),str(client_fixtures.four_bone_vs3())]
   elif name=='distance':print('SKIP sub-case: distance corpus (NORTHLIGHT_SHADER_CORPUS not set)')
   result=subprocess.check_output(args,text=True);print(name,label,result,flush=True);report.append({'test':name,'build':label,'output':result})
OUT.mkdir(exist_ok=True);(OUT/'cache-shadow-validation.json').write_text(json.dumps(report,indent=2)+'\n')
