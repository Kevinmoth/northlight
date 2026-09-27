// Offline CPU/fake-D3D regression, with production placement transforms.
#define STATIC_SHADOW_GPU_TEST
#include "test_static_shadow_fake_d3d.h"
#include "static_shadow_gpu.h"
#include "test_shadow_bounds_reference.h"
#include "world_math.h"
#include <cassert>
#include <iostream>
using namespace StaticShadow;
static float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
static std::shared_ptr<Model> model(std::string key,unsigned batches=1){
 auto m=std::make_shared<Model>();m->key=key;m->contentRevision=1;m->index16=true;
 NorthlightGI::WorldMaterial mat;mat.alphaCutoff=.37f;mat.addressU=3;mat.addressV=2;mat.width=mat.height=1;mat.rgba={255,255,255,127};m->materials={mat};m->materialKeys={1};
 for(unsigned b=0;b<batches;++b){m->vertices.insert(m->vertices.end(),{{0,0,.5f,0,0},{.2f,0,.5f,1,0},{0,.2f,.5f,0,1}});m->indices.insert(m->indices.end(),{3*b,3*b+1,3*b+2});m->batches.push_back({3*b,3,0,{0,0,.5f},{.2f,.2f,.5f}});}return m;
}
static void add(Snapshot& s,std::shared_ptr<Model> m,uint64_t uid,Vec3 translation){Placement p;p.uid=uid;p.category=1;p.modelKey=m->key;p.translation=translation;s.placements.push_back(p);s.models[m->key]=m;}
static std::shared_ptr<Snapshot> scene(){auto s=std::make_shared<Snapshot>();s->map="test";s->mapGeneration=1;return s;}

static std::shared_ptr<Snapshot> populated(unsigned models=80,unsigned placements=25){
 auto s=scene();for(unsigned m=0;m<models;++m){auto shape=model("model-"+std::to_string(m),4);for(unsigned i=0;i<placements;++i)add(*s,shape,m*1000+i,{float(i%5)*.08f,float(i/5)*.08f,float(m)*.0001f});}return s;
}
static void equalRecords(const IDirect3DDevice9& a,const IDirect3DDevice9& b){assert(a.drawnRecords.size()==b.drawnRecords.size());for(size_t i=0;i<a.drawnRecords.size();++i){const auto& x=a.drawnRecords[i];const auto& y=b.drawnRecords[i];assert(x.transform==y.transform&&x.firstIndex==y.firstIndex&&x.pixel==y.pixel);}}
int main(){
 GpuCache::Limits limits;limits.uploadMs=1000;limits.uploadBytes=128u<<20;
 size_t records=0;uint64_t incrementalTests=0,fullTests=0;
 for(bool metadata:{true,false}){
  auto l=limits;if(!metadata)l.planMetadataBytes=0;
  IDirect3DDevice9 d;d.captureTransforms=true;GpuCache warm(l);auto s=populated();assert(warm.update(&d,s,1));warm.signature(identity);
  std::vector<Placement> covered;
  for(unsigned iteration=0;iteration<18;++iteration){
   auto next=std::make_shared<Snapshot>(*s);size_t group=iteration%6;const std::string key="model-"+std::to_string(group);
   if(iteration%6==0){next->placements[group*25].translation.x=.7f;}
   if(iteration%6==1){next->placements[group*25].translation.x=40;}
   if(iteration%6==2){auto replacement=std::make_shared<Model>(*next->models.at(key));++replacement->contentRevision;replacement->batches.front().high.x=.4f;next->models[key]=replacement;}
   if(iteration%6==3){covered={next->placements[group*25]};}
   if(iteration%6==4){covered.clear();auto p=next->placements[group*25];p.uid+=100000;p.translation.x=.15f;next->placements.push_back(p);}
   if(iteration%6==5){next->placements.erase(std::remove_if(next->placements.begin(),next->placements.end(),[&](const Placement& p){return p.modelKey=="model-79";}),next->placements.end());next->models.erase("model-79");}
   const auto before=warm.stats().placementTests;warm.setCoveredPlacements(covered);assert(warm.update(&d,next,iteration+2));auto signature=warm.signature(identity);
   IDirect3DDevice9 reference;reference.captureTransforms=true;GpuCache cold(limits);cold.setCoveredPlacements(covered);assert(cold.update(&reference,next,1));assert(signature==cold.signature(identity));
   d.drawnRecords.clear();d.drawnTransforms.clear();assert(warm.draw(&d,identity)&&cold.draw(&reference,identity));equalRecords(d,reference);records+=d.drawnRecords.size();
   assert(warm.stats().instances==cold.stats().instances&&warm.stats().drawCalls==cold.stats().drawCalls);
   if(metadata){incrementalTests+=warm.stats().placementTests-before;fullTests+=cold.stats().placementTests;}
   cold.reset();s=next;
  }
  if(metadata)assert(warm.stats().modelPlanReuses>0&&incrementalTests*10<fullTests);else assert(warm.stats().modelPlanFallbacks>0&&!warm.stats().modelPlanReuses);
  // Several models can change/retire before one signature call; never copy an
  // old resource slice after the first invalidation hid subsequent changes.
  auto empty=scene();assert(warm.update(&d,empty,10000,false));assert(warm.signature(identity)!=0);assert(warm.draw(&d,identity)&&warm.stats().instances==0);
  warm.reset();assert(FakeResource::alive==0);
 }
 // Matrix-slot replacement and reset must invalidate all old model slices.
 {IDirect3DDevice9 d;d.captureTransforms=true;GpuCache gpu(limits);auto s=populated(2,4);gpu.update(&d,s,1);for(unsigned i=0;i<20;++i){float m[16];std::memcpy(m,identity,sizeof m);m[12]=i*.01f;gpu.signature(m);assert(gpu.draw(&d,m));}gpu.reset();gpu.update(&d,s,2);assert(gpu.draw(&d,identity)&&gpu.stats().instances==32);gpu.reset();assert(FakeResource::alive==0);}
 // Native selection-only benchmark. GPU uploads/drawing are excluded; every
 // changed model is still immediately present in the resulting draw plan.
 auto benchmark=[&](bool incremental){auto l=limits;if(!incremental)l.planMetadataBytes=0;IDirect3DDevice9 d;GpuCache gpu(l);auto s=populated(80,25);gpu.update(&d,s,1);gpu.signature(identity);double milliseconds=0;
  for(unsigned i=0;i<200;++i){auto next=std::make_shared<Snapshot>(*s);next->placements.front().translation.x=float(i%2)*.03f;gpu.update(&d,next,i+2);auto begin=std::chrono::steady_clock::now();gpu.signature(identity);milliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();s=next;}
  gpu.reset();assert(FakeResource::alive==0);return milliseconds;
 };
 const double fullMs=benchmark(false),incrementalMs=benchmark(true);
 std::cout<<"native selection-only 200 single-model updates, 80 models x25 placements x4 batches: full="<<fullMs<<"ms incremental="<<incrementalMs<<"ms ratio="<<fullMs/incrementalMs<<"\n";
 std::cout<<"incremental static plans: "<<records<<" exact records, changed-model placement tests "<<incrementalTests<<" vs full "<<fullTests<<", revision/move/add/remove/coverage/reset/metadata-cap parity passed\n";
}
