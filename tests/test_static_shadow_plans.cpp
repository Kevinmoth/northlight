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
static uint32_t rng=0x873fe54;
static float random(float lo,float hi){rng=rng*1664525u+1013904223u;return lo+(hi-lo)*float(rng>>8)/16777216.f;}
int main(){
 // Each resource publication, replacement and exact-owner change touches only
 // intersecting cascades; signature and draw use one prepared list.
 {IDirect3DDevice9 d;GpuCache::Limits l;l.uploadMs=1000;GpuCache gpu(l);auto s=scene();auto a=model("a"),b=model("b");add(*s,a,1,{0,0,0});gpu.update(&d,s,1);
  float far[16];std::memcpy(far,identity,sizeof far);far[12]=-50;
  auto nearSignature=gpu.signature(identity),farSignature=gpu.signature(far);auto builds=gpu.stats().planBuilds;
  assert(gpu.draw(&d,identity)&&gpu.stats().planBuilds==builds);assert(gpu.signature(identity)==nearSignature&&gpu.stats().planBuilds==builds);
  auto next=std::make_shared<Snapshot>(*s);add(*next,b,2,{50,0,0});gpu.update(&d,next,2,false);
  assert(gpu.signature(identity)==nearSignature&&gpu.signature(far)==farSignature&&gpu.stats().planBuilds==builds);
  gpu.update(&d,next,3);assert(gpu.signature(identity)==nearSignature&&gpu.stats().planBuilds==builds);farSignature=gpu.signature(far);assert(gpu.stats().planBuilds==++builds);
  gpu.setCoveredPlacements({next->placements.back()});assert(gpu.signature(identity)==nearSignature&&gpu.stats().planBuilds==builds);assert(gpu.signature(far)!=farSignature&&gpu.stats().planBuilds==++builds);
  gpu.setCoveredPlacements({});assert(gpu.signature(far)==farSignature&&gpu.stats().planBuilds==++builds);assert(gpu.signature(identity)==nearSignature&&gpu.stats().planBuilds==builds);
  auto newer=std::make_shared<Snapshot>(*next);auto revision=model("b");revision->contentRevision=2;newer->models["b"]=revision;gpu.update(&d,newer,4);
  assert(gpu.signature(identity)==nearSignature&&gpu.stats().planBuilds==builds);assert(gpu.signature(far)!=farSignature&&gpu.stats().planBuilds==++builds);
  // Moving an existing placement removes its old shadow AND adds its new one.
  auto moved=std::make_shared<Snapshot>(*newer);moved->placements.back().translation.x=0;gpu.update(&d,moved,5,false);
  assert(gpu.signature(identity)!=nearSignature);assert(gpu.draw(&d,identity)&&gpu.stats().instances==2);assert(gpu.draw(&d,far)&&gpu.stats().instances==0);
  // Evicted resources cannot leave dangling prepared command pointers.
  auto empty=scene();gpu.update(&d,empty,10000,false);assert(gpu.draw(&d,identity)&&gpu.signature(identity)!=nearSignature);gpu.reset();assert(FakeResource::alive==0);
 }
 // Reject an entire off-light model placement before visiting its 100 parts.
 // Placement metadata boxes are intentionally absent; actual batch bounds win.
 {IDirect3DDevice9 d;GpuCache::Limits l;l.uploadMs=1000;GpuCache gpu(l);auto s=scene();auto m=model("parts",100);for(unsigned i=0;i<500;++i)add(*s,m,i,{i?50.f:0.f,0,0});gpu.update(&d,s,1);
  gpu.signature(identity);assert(gpu.stats().placementTests==500&&gpu.stats().batchTests==100);assert(gpu.draw(&d,identity)&&gpu.stats().instances==100);assert(gpu.stats().planBuilds==1&&d.indexBindings==1&&d.materialConstants==1&&d.textureBindings==1);gpu.reset();assert(FakeResource::alive==0);
 }
 // Analytic bounds must never reject anything the old eight-corner path kept.
 unsigned comparisons=0;
 for(unsigned trial=0;trial<100000;++trial){Vec3 center{random(-17000,17000),random(-17000,17000),random(-3000,3000)};
  float m[16];const float e=(trial%3?random(2,85):2)*.01745329252f,z=random(-3.14f,3.14f);
  NorthlightWorldMath::shadowMatrix(center,{std::cos(e)*std::cos(z),std::cos(e)*std::sin(z),std::sin(e)},trial%2?48:192,m);
  Vec3 p=center+Vec3(random(-1500,1500),random(-1500,1500),random(-900,900)),r{random(0,300),random(0,300),random(0,500)};
  for(bool upstream:{false,true}){bool before=ReferenceShadowBounds::clipReject(p-r,p+r,m,1e-4f,upstream),after=NorthlightShadowBounds::clipReject(p-r,p+r,m,1e-4f,upstream);assert(!after||before);++comparisons;}
 }
 // Reference batch traversal vs prepared commands at low sun, world-coordinate
 // cancellation, rotation, nonuniform/mirrored scaling and sheared placements.
 size_t checked=0;
 for(unsigned trial=0;trial<40;++trial){IDirect3DDevice9 d;d.captureTransforms=true;GpuCache::Limits l;l.uploadMs=1000;GpuCache gpu(l);auto s=scene();auto m=model("random",20);
  Vec3 center{random(-17000,17000),random(-17000,17000),random(-100,500)};
  for(auto& b:m->batches){b.low={random(-30,0),random(-30,0),random(-30,0)};b.high={random(0,30),random(0,30),random(0,80)};}
  for(unsigned i=0;i<100;++i){add(*s,m,i,center+Vec3(random(-600,600),random(-600,600),random(-100,100)));for(float& c:s->placements.back().matrix)c=random(-3,3);}
  float matrix[16];float e=(trial%3==0?2:trial%3==1?5:15)*.01745329252f;
  NorthlightWorldMath::shadowMatrix(center,{std::cos(e),0,std::sin(e)},trial%2?48:192,matrix);
  gpu.update(&d,s,1);gpu.signature(matrix);assert(gpu.draw(&d,matrix));
  std::vector<IDirect3DDevice9::DrawRecord> expected;
  for(const auto& batch:m->batches)for(unsigned path=0;path<2;++path)for(const auto& p:s->placements){Vec3 lo,hi;transformBounds(p,batch.low,batch.high,lo,hi);
   if(ReferenceShadowBounds::directionalClipReject(lo,hi,matrix)||unsigned(NorthlightShadowBounds::depthFullyInside(lo,hi,matrix))!=path)continue;
   IDirect3DDevice9::DrawRecord record{};record.firstIndex=batch.firstIndex;record.pixel=path?1:0;
   for(unsigned r=0;r<3;++r){for(unsigned c=0;c<3;++c)record.transform[r*4+c]=p.matrix[r*3+c];record.transform[r*4+3]=r==0?p.translation.x:r==1?p.translation.y:p.translation.z;}expected.push_back(record);
  }
  size_t pos=0;for(const auto& record:expected){while(pos<d.drawnRecords.size()&&(d.drawnRecords[pos].transform!=record.transform||d.drawnRecords[pos].firstIndex!=record.firstIndex||d.drawnRecords[pos].pixel!=record.pixel))++pos;assert(pos<d.drawnRecords.size());++pos;++checked;}
  // Reused plans still produce exactly the same content/order.
  auto first=d.drawnTransforms;d.drawnTransforms.clear();assert(gpu.draw(&d,matrix)&&d.drawnTransforms==first&&gpu.stats().planBuilds==1);
  gpu.reset();assert(FakeResource::alive==0);
 }
 // Unsupported/invalid matrices must remain fail-open.
 for(unsigned mode=0;mode<4;++mode){float m[16];std::memcpy(m,identity,sizeof m);if(mode==0)m[3]=.1f;if(mode==1)m[14]=NAN;if(mode==2)m[0]=INFINITY;if(mode==3)m[15]=0;assert(!NorthlightShadowBounds::directionalClipReject({5,5,5},{6,6,6},m));}
 std::cout<<"shadow plans: targeted publication/revision/ownership/eviction, 500:1 coarse rejection, "<<comparisons<<" bounds comparisons and "<<checked<<" reference caster records preserved\n";
}
