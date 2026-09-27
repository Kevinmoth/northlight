#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native policy, shared-owner lifetime, deformation-distance and capture-budget tests."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import client_fixtures  # the real client programs, from the tester's client
from pathlib import Path
import ast,hashlib,json,subprocess,tempfile
HERE=Path(__file__).resolve().parent
OUT=fp.output_dir()
def literal(path,key):
    return next(ast.literal_eval(n.value) for n in ast.parse(path.read_text()).body if isinstance(n,ast.Assign) and any(getattr(t,'id','')==key for t in n.targets))
stub=literal(HERE/'test_terrain_snapshot.py','stub')
policy=r'''
#include "replay_shadow_policy.h"
#include <cassert>
#include <memory>
#include <limits>
#include <cstdio>
using namespace NorthlightReplayShadowPolicy;
struct Packet{bool shadowSelected=true;int value=0;const int* borrowed=nullptr;};
int main(){
 assert(!small(false,1,50)&&small(true,49,50)&&!small(true,50,50)&&!small(true,1,0));
 for(unsigned n=0;n<3000;++n){unsigned expected=n<25?0:n<50?1:n<100?2:n<500?3:n<2000?4:5;assert(bucket(n)==expected);}
 std::vector<Candidate> v={{0,10,100,true},{1,10,1,true},{2,10,25,true}};
 auto r=choose(v,20);assert(r.kept==2&&r.dropped==1&&r.keptBytes==20&&r.droppedBytes==10);
 assert(v[0].index==1&&v[0].keep&&v[1].index==2&&v[1].keep&&!v[2].keep);
 v={{0,10,4,true},{1,10,4,true},{2,10,4,true}};choose(v,20);assert(v[0].index==0&&v[1].index==1&&!v[2].keep);
 v={{0,100,1,true},{1,10,2,true}};r=choose(v,10);assert(!v[0].keep&&v[1].keep&&r.keptBytes==10);
 v={{0,10,1,true},{1,10,999,false},{2,10,std::numeric_limits<float>::quiet_NaN(),true}};
 r=choose(v,20);assert(r.unknown==2&&v[0].index==1&&v[1].index==2&&!v[2].keep);
 v={{0,10,100,true},{1,10,1,true}};r=choose(v,0);assert(v[0].index==0&&v[1].index==1&&r.kept==2&&r.dropped==0);
 // Every excluded owner stays alive until all borrowers have finished replaying.
 for(unsigned mask=0;mask<256;++mask){
  std::vector<std::unique_ptr<Packet>> packets,held;
  for(unsigned i=0;i<8;++i){auto p=std::make_unique<Packet>();p->value=int(i+123);p->shadowSelected=(mask>>i)&1;packets.push_back(std::move(p));}
  for(unsigned i=0;i<8;++i)packets[i]->borrowed=&packets[0]->value;
  retainSelected(packets,held);int previous=-1;unsigned count=0;
  for(auto& p:packets){assert(p->shadowSelected&&*p->borrowed==123&&p->value>previous);previous=p->value;++count;}
  assert(count+held.size()==8);for(auto& p:held)assert(!p->shadowSelected&&*p->borrowed==123);
  packets.clear();held.clear();
 }
 std::puts("policy: thresholds, stable near-first budget, unknown distances, oversize, disabled identity, 256 owner-retention patterns passed");
}
'''
actor=literal(HERE/'test_actor_deformation.py','harness')
needle='  // Rotate camera and corresponding bone-to-view matrices: world pose is invariant.'
extra=r'''
  {float camera[]={1000,-20,0},distance=-1;
   assert(sampledDistanceSquared(p,mesh,decl,3,constants,inverse,camera,distance));
   assert(distance==102*102+13*13+4*4);
   mesh.indexed=true;assert(!sampledDistanceSquared(p,mesh,decl,3,constants,inverse,camera,distance));
   mesh.indices={0};assert(sampledDistanceSquared(p,mesh,decl,3,constants,inverse,camera,distance));
   mesh.indices={1};assert(!sampledDistanceSquared(p,mesh,decl,3,constants,inverse,camera,distance));
   mesh.indices={0};auto bytes=mesh.streams[0].bytes;mesh.streams[0].bytes.resize(3);
   assert(!sampledDistanceSquared(p,mesh,decl,3,constants,inverse,camera,distance));mesh.streams[0].bytes=bytes;
   assert(!sampledDistanceSquared(uv,mesh,decl,3,constants,inverse,camera,distance));
   camera[0]=NAN;assert(!sampledDistanceSquared(p,mesh,decl,3,constants,inverse,camera,distance));
   mesh.indexed=false;mesh.indices.clear();
  }
'''
assert needle in actor;actor=actor.replace(needle,extra+needle)
needle='  mesh.streams[0].bytes[16]=255;'
extra=r'''
  {float camera[]={1000,-20,0},distance=-1;
   assert(sampledDistanceSquared(p,mesh,decl,3,rotatedConstants,inverse,camera,distance));
   assert(distance==102*102+13*13+4*4);
   // Index zero can be outside the actual draw: use the first referenced vertex.
   mesh.vertexCount=2;mesh.streams[0].bytes.resize(40);std::memcpy(mesh.streams[0].bytes.data()+20,mesh.streams[0].bytes.data(),20);
   float far=50;std::memcpy(mesh.streams[0].bytes.data(),&far,4);mesh.indexed=true;mesh.indices={1};
   assert(sampledDistanceSquared(p,mesh,decl,3,rotatedConstants,inverse,camera,distance));assert(distance==102*102+13*13+4*4);
   mesh.vertexCount=1;mesh.streams[0].bytes.resize(20);mesh.indexed=false;mesh.indices.clear();
  }
'''
assert needle in actor;actor=actor.replace(needle,extra+needle)
budget=literal(HERE/'test_draw_snapshot.py','harness')
needle='int main(){Device d;Frame frame;Mesh a,b;Diagnostics why;Draw draw{D3DPT_TRIANGLELIST,-3,7,3,1,1,true};'
extra=r'''
 {Frame configured;assert(configured.configureBudget(240,120));Mesh m;
  assert(configured.read(&d,&d.decl,draw,m,&why));
  assert(!configured.configureBudget(1000,500));
  assert(!configured.read(&d,&d.decl,draw,m,&why)&&why.error==Error::Budget);
  assert(configured.read(&d,&d.decl,draw,m,&why,true));
  configured.clearFrame();assert(configured.configureBudget(0,0));assert(configured.captureExhausted(true));
  assert(configured.configureBudget(240,120));assert(!configured.captureExhausted(true));
 }
'''
assert needle in budget;budget=budget.replace(needle,needle+extra)
reports=[]
with tempfile.TemporaryDirectory(prefix='northlight-shadow-experiment-') as tmp:
    p=Path(tmp);(p/'d3d9.h').write_text(stub)
    for name,source in [('policy',policy),('distance',actor),('capture',budget)]:
        src=p/(name+'.cpp');src.write_text(source)
        for label,flags in [('O2',['-O2']),('ASan+UBSan',['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
            exe=p/name
            subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-I',str(p),*fp.test_include_flags(),str(src),'-o',str(exe)],check=True)
            args=[str(exe)]
            if name=='distance' and fp.shader_corpus() and client_fixtures.shaders_available():args+=[str(fp.shader_corpus()),str(client_fixtures.four_bone_vs3())]
            elif name=='distance':print('SKIP sub-case: distance corpus (NORTHLIGHT_SHADER_CORPUS not set)')
            result=subprocess.check_output(args,text=True);reports.append({'case':name,'build':label,'stdout':result});print(name,label,result,flush=True)
source_names=['replay_shadow_policy.h','world_shadow_experiment.inl','actor_deformation.h','draw_snapshot.h','world_renderer.h','renderer.cpp','test_shadow_experiment.py']
report={'status':'pass','tests':reports,'source_sha256':{n:hashlib.sha256(fp.tracked(n).read_bytes()).hexdigest() for n in source_names},'game_launched':False,'wine_launched':False,'limitations':['Distance is a sampled current vertex, not exact object distance or a culling proof.','Native tests do not validate Windows/macOS GPU image or FPS.','Near-first selection covers only already-captured geometry; hard capture rejects remain draw-ordered.']}
OUT.mkdir(exist_ok=True);(OUT/'shadow-experiment-validation.json').write_text(json.dumps(report,indent=2)+'\n')
