// Offline fake-D3D + software raster proof for 0.3.136 static cache dirty
// rects: R32F colour = float depth, D24 LESSEQUAL depth, shared (scrambled)
// depth buffer. A scissored rect redraw of the complete ordered plan must be
// bit-identical to a full clear+redraw; a naive append is shown order-dependent.
#define STATIC_SHADOW_GPU_TEST
#include "test_static_shadow_fake_d3d.h"
#include "static_shadow_gpu.h"
#include "static_cache_slices.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include <chrono>
using namespace StaticShadow;
static float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
static constexpr long Size=256,Margin=2,Tile=16;static constexpr size_t MaxRects=8;
struct Target {std::vector<float> color;std::vector<uint32_t> depth;Target():color(Size*Size,1.f),depth(Size*Size,0xffffffu){}};
static float edge(float ax,float ay,float bx,float by,float px,float py){return (bx-ax)*(py-ay)-(by-ay)*(px-ax);}
// Top-left-free sample rule is irrelevant here: both paths share it exactly.
static void raster(const IDirect3DDevice9& d,Target& t,const float* m){
 for(const auto& r:d.drawnRecords){
  const auto* vb=r.vertices;const auto* ib=r.indices;assert(vb&&ib);
  for(unsigned tri=0;tri<r.primitives;++tri){float sx[3],sy[3],sz[3];
   for(unsigned k=0;k<3;++k){uint16_t index;std::memcpy(&index,ib->bytes.data()+(r.firstIndex+tri*3+k)*2,2);Vertex v;std::memcpy(&v,vb->bytes.data()+index*sizeof(Vertex),sizeof v);
    const float* row=r.transform.data();const float w[3]={row[0]*v.x+row[1]*v.y+row[2]*v.z+row[3],row[4]*v.x+row[5]*v.y+row[6]*v.z+row[7],row[8]*v.x+row[9]*v.y+row[10]*v.z+row[11]};
    const float cx=w[0]*m[0]+w[1]*m[4]+w[2]*m[8]+m[12],cy=w[0]*m[1]+w[1]*m[5]+w[2]*m[9]+m[13],cz=w[0]*m[2]+w[1]*m[6]+w[2]*m[10]+m[14];
    sx[k]=(cx+1)*.5f*Size;sy[k]=(1-cy)*.5f*Size;sz[k]=cz;}
   const float area=edge(sx[0],sy[0],sx[1],sy[1],sx[2],sy[2]);if(area==0)continue;
   for(long y=0;y<Size;++y)for(long x=0;x<Size;++x){
    if(r.scissorTest&&(x<r.scissor.left||x>=r.scissor.right||y<r.scissor.top||y>=r.scissor.bottom))continue;
    const float px=x+.5f,py=y+.5f;float w0=edge(sx[1],sy[1],sx[2],sy[2],px,py)/area,w1=edge(sx[2],sy[2],sx[0],sy[0],px,py)/area,w2=edge(sx[0],sy[0],sx[1],sy[1],px,py)/area;
    if(w0<0||w1<0||w2<0)continue;
    const float z=std::max(0.f,w0*sz[0]+w1*sz[1]+w2*sz[2]);if(z>1)continue;
    const uint32_t q=uint32_t(std::lround(double(z)*16777215.0));auto& dst=t.depth[y*Size+x];
    if(q<=dst){dst=q;t.color[y*Size+x]=z;}
   }
  }
 }
}
static std::shared_ptr<Model> model(std::string key,float z){
 auto m=std::make_shared<Model>();m->key=key;m->contentRevision=1;m->index16=true;
 NorthlightGI::WorldMaterial mat;mat.alphaCutoff=0;mat.width=mat.height=1;mat.rgba={255,255,255,255};m->materials={mat};m->materialKeys={7};
 m->vertices={{0,0,z,0,0},{.2f,0,z,1,0},{0,.2f,z,0,1},{.2f,.2f,z,1,1}};m->indices={0,1,2,1,3,2};m->batches.push_back({0,6,0,{0,0,z},{.2f,.2f,z}});return m;
}
static void add(Snapshot& s,const std::shared_ptr<Model>& m,uint64_t uid,Vec3 t){Placement p;p.uid=uid;p.category=1;p.modelKey=m->key;p.translation=t;s.placements.push_back(p);s.models[m->key]=m;}
static bool same(const Target& a,const Target& b){return std::memcmp(a.color.data(),b.color.data(),a.color.size()*4)==0;}
int main(){
 GpuCache::Limits limits;limits.uploadMs=1000;limits.uploadBytes=128u<<20;
 // Tie witness: two keys in one D24 quantum, different float colour.
 const float tieLow=.5f,tieHigh=std::nextafter(.5f,1.f);
 assert(std::lround(double(tieLow)*16777215.0)==std::lround(double(tieHigh)*16777215.0));
 std::mt19937 rng(136);std::uniform_real_distribution<float> pos(-.9f,.7f),depth(.1f,.9f);
 unsigned partial=0,naiveDiffers=0,removals=0;size_t maxRects=0;unsigned long long partialTexels=0,skipped=0,drawn=0,fullDraws=0;
 IDirect3DDevice9 d;d.captureTransforms=true;GpuCache warm(limits);
 auto s=std::make_shared<Snapshot>();s->map="t";s->mapGeneration=1;
 std::vector<std::shared_ptr<Model>> shapes;for(unsigned i=0;i<24;++i)shapes.push_back(model("m"+std::to_string(100+i),i<2?(i?tieHigh:tieLow):depth(rng)));
 // Seed: every second model, with coplanar pair placed identically.
 for(unsigned i=1;i<shapes.size();i+=2)for(unsigned k=0;k<4;++k)add(*s,shapes[i],i*100+k,{i<2?.1f:pos(rng),i<2?.1f:pos(rng),0});
 assert(warm.update(&d,s,1));GpuCache::ContentRecord record;
 Target cache;d.drawnRecords.clear();assert(warm.draw(&d,identity));raster(d,cache,identity);warm.record(identity,record);assert(record.valid);
 for(unsigned iteration=0;iteration<60;++iteration){
  auto next=std::make_shared<Snapshot>(*s);next->preparedIdentity=nullptr;std::vector<Placement> covered;
  const unsigned kind=iteration%5,pick=unsigned(rng()%shapes.size());
  if(kind==0||iteration==0){const unsigned i=iteration==0?0:pick;for(unsigned k=0;k<3;++k)add(*next,shapes[i],1000*iteration+i*10+k,{i<2?.1f:pos(rng),i<2?.1f:pos(rng),0});} // readiness/additions
  Placement removed;bool removal=false;
  if(kind==1&&!next->placements.empty()){const size_t i=rng()%next->placements.size();removed=next->placements[i];removal=true;next->placements.erase(next->placements.begin()+i);} // removal
  if(kind==2&&!next->placements.empty()){auto& p=next->placements[rng()%next->placements.size()];p.translation.x=pos(rng);} // move
  if(kind==3){auto& key=shapes[pick]->key;if(next->models.count(key)){auto r=std::make_shared<Model>(*next->models.at(key));++r->contentRevision;for(auto& v:r->vertices)v.z=depth(rng);r->batches[0].low.z=r->batches[0].high.z=r->vertices[0].z;next->models[key]=r;}} // revision
  if(kind==4&&!next->placements.empty())covered={next->placements[rng()%next->placements.size()]}; // ownership
  warm.setCoveredPlacements(covered);assert(warm.update(&d,next,iteration+2));
  // Reference: complete clear + redraw of the new content.
  Target reference;d.drawnRecords.clear();assert(warm.draw(&d,identity));raster(d,reference,identity);fullDraws+=d.drawnRecords.size();
  std::vector<CasterBounds> boxes;size_t models=0;assert(warm.changedBounds(identity,record,boxes,models));
  std::vector<NorthlightShadowBounds::TexelRect> footprints,rects;for(const auto& b:boxes){NorthlightShadowBounds::TexelRect f;assert(b.bounded&&NorthlightShadowBounds::texelFootprint(b.low,b.high,identity,Size,Margin,f));footprints.push_back(f);}
  NorthlightShadowBounds::dirtyRects(footprints,Size,Tile,MaxRects,rects);maxRects=std::max(maxRects,rects.size());
  for(size_t i=0;i<rects.size();++i)for(size_t j=i+1;j<rects.size();++j)assert(!NorthlightShadowBounds::intersects(rects[i],rects[j]));
  // The removed placement's OLD footprint must be inside the rects.
  if(removal){const auto& m=*s->models.at(removed.modelKey);Vec3 lo,hi;transformBounds(removed,m.batches[0].low,m.batches[0].high,lo,hi);NorthlightShadowBounds::TexelRect f;assert(NorthlightShadowBounds::texelFootprint(lo,hi,identity,Size,0,f));footprints.push_back(f);++removals;}
  for(const auto& f:footprints){long long covered=0;for(const auto& r:rects){const long l=std::max(f.left,r.left),rr=std::min(f.right,r.right),tp=std::max(f.top,r.top),b=std::min(f.bottom,r.bottom);if(l<rr&&tp<b)covered+=(long long)(rr-l)*(b-tp);}assert(covered==f.area());}
  // Naive append with a persistent depth buffer: old content, then only the
  // newly added model. At a D24 tie the later draw wins, so the key-sorted
  // full redraw and the append disagree: incremental add is not exact.
  if(iteration==0){Target ordered;
   {IDirect3DDevice9 oldD;oldD.captureTransforms=true;GpuCache oldCache(limits);assert(oldCache.update(&oldD,s,1)&&oldCache.draw(&oldD,identity));raster(oldD,ordered,identity);oldCache.reset();}
   auto only=std::make_shared<Snapshot>();only->map="t";only->mapGeneration=1;for(const auto& p:next->placements)if(p.modelKey==shapes[0]->key){only->placements.push_back(p);only->models[p.modelKey]=next->models.at(p.modelKey);}
   IDirect3DDevice9 appendD;appendD.captureTransforms=true;GpuCache appended(limits);assert(appended.update(&appendD,only,1)&&appended.draw(&appendD,identity));
   raster(appendD,ordered,identity);naiveDiffers+=!same(ordered,reference);appended.reset();
  }
  // Partial: other slots own the shared depth buffer outside the rect.
  for(auto& value:cache.depth)value=uint32_t(rng()&0xffffffu);
  for(const auto& dirty:rects)for(long y=dirty.top;y<dirty.bottom;++y)for(long x=dirty.left;x<dirty.right;++x){cache.color[y*Size+x]=1.f;cache.depth[y*Size+x]=0xffffffu;}
  d.drawnRecords.clear();d.scissorTest=0;assert(warm.draw(&d,identity,&rects,Size,Margin));assert(d.scissorTest==1);for(const auto& r:d.drawnRecords)assert(r.scissorTest);
  skipped+=warm.stats().rectSkipped;drawn+=d.drawnRecords.size();raster(d,cache,identity);
  if(!same(cache,reference)){std::cerr<<"mismatch iteration "<<iteration<<" kind "<<kind<<"\n";return 1;}
  ++partial;for(const auto& r:rects)partialTexels+=r.area();warm.record(identity,record);assert(record.valid);s=next;
  // Committed-content lookup used by the dedup diagnostic.
  for(const auto& slice:record.slices)assert(record.contains(slice.key,slice.revision)&&!record.contains(slice.key,slice.revision+1));assert(!record.contains("absent",1));
 }
 // 0.3.151 StaticCacheSlices: the renderer's cycle decisions (world_renderer.h) over the same
 // content stream. Bands are drawn one per frame into the scrambled shared depth; mid-cycle
 // changes restart with diff(recorded key) + every drawn band; content back at the key redraws
 // the drawn bands. Every completed cycle is bit-identical to a full redraw of the final content,
 // including a model added and removed again mid-cycle (the ghost case).
 unsigned sliceCycles=0,sliceBands=0,sliceRestarts=0,sliceGhosts=0,sliceReverts=0;
 for(unsigned slices:{2u,3u,4u}){NorthlightStaticSlices::Cycle cycle;uint64_t keySignature=warm.signature(identity);
  auto mutate=[&](const std::shared_ptr<Snapshot>& from,unsigned kind,Placement* added)->std::shared_ptr<Snapshot>{
   auto next=std::make_shared<Snapshot>(*from);next->preparedIdentity=nullptr;const unsigned pick=unsigned(rng()%shapes.size());
   if(kind==0){for(unsigned k=0;k<2;++k)add(*next,shapes[pick],7000000+rng()%1000000,{pos(rng),pos(rng),0});if(added)*added=next->placements.back();}
   else if(kind==1&&!next->placements.empty())next->placements.erase(next->placements.begin()+long(rng()%next->placements.size()));
   else if(kind==2&&!next->placements.empty())next->placements[rng()%next->placements.size()].translation.y=pos(rng);
   return next;};
  auto drawRects=[&](const std::vector<NorthlightShadowBounds::TexelRect>& rects){
   for(auto& value:cache.depth)value=uint32_t(rng()&0xffffffu);
   for(const auto& r:rects)for(long y=r.top;y<r.bottom;++y)for(long x=r.left;x<r.right;++x){cache.color[y*Size+x]=1.f;cache.depth[y*Size+x]=0xffffffu;}
   d.drawnRecords.clear();assert(warm.draw(&d,identity,&rects,Size,Margin));raster(d,cache,identity);};
  auto complete=[&](){Target reference;d.drawnRecords.clear();assert(warm.draw(&d,identity));raster(d,reference,identity);
   if(!same(cache,reference)){std::cerr<<"sliced mismatch slices "<<slices<<" cycle "<<sliceCycles<<"\n";std::exit(1);}
   warm.record(identity,record);assert(record.valid);keySignature=warm.signature(identity);cycle.reset();++sliceCycles;};
  // One renderer frame for this slot: continue, (re)start, or redraw the drawn bands.
  auto frame=[&](){const uint64_t now=warm.signature(identity);
   if(cycle.current(now)&&now!=keySignature){drawRects(cycle.band());++sliceBands;if(cycle.last())complete();else cycle.drew();return;}
   if(now!=keySignature){std::vector<CasterBounds> boxes;size_t models=0;assert(warm.changedBounds(identity,record,boxes,models));
    std::vector<NorthlightShadowBounds::TexelRect> fp,rects;for(const auto& b:boxes){NorthlightShadowBounds::TexelRect f;assert(NorthlightShadowBounds::texelFootprint(b.low,b.high,identity,Size,Margin,f));fp.push_back(f);}
    NorthlightShadowBounds::dirtyRects(fp,Size,Tile,MaxRects,rects);
    sliceRestarts+=cycle.active;cycle.start(rects,slices,"static-models-partial",now,Size,Tile,MaxRects);
    drawRects(cycle.band());++sliceBands;if(cycle.last())complete();else cycle.drew();return;}
   if(cycle.active){std::vector<NorthlightShadowBounds::TexelRect> rects;NorthlightShadowBounds::dirtyRects(cycle.drawn,Size,Tile,MaxRects,rects);++sliceReverts;if(!rects.empty())drawRects(rects);complete();}};
  for(unsigned iteration=0;iteration<24;++iteration){
   Placement added;auto next=mutate(s,iteration%3,&added);warm.setCoveredPlacements({});assert(warm.update(&d,next,5000+iteration*10));s=next;
   const unsigned scenario=iteration%4;unsigned step=0;
   while(true){frame();++step;if(!cycle.active)break;
    if(step==1&&scenario==1){next=mutate(s,iteration%3==0?1:0,nullptr);assert(warm.update(&d,next,5000+iteration*10+step));s=next;} // restart mid-cycle
    if(step==1&&scenario==2&&iteration%3==0){next=std::make_shared<Snapshot>(*s);next->preparedIdentity=nullptr; // add, draw a band, remove again
     assert(next->placements.back().uid==added.uid);next->placements.resize(next->placements.size()-2);
     if(iteration>=12)next->placements[rng()%next->placements.size()].translation.x=pos(rng); // and restart with another change
     assert(warm.update(&d,next,5000+iteration*10+step));s=next;++sliceGhosts;}
    if(step==2&&scenario==3){for(unsigned k=0;k<4&&cycle.active;++k){next=mutate(s,k%3,nullptr);assert(warm.update(&d,next,5000+iteration*10+step+k));s=next;frame();}} // restart cap
    assert(step<32);}
  }
 }
 assert(sliceCycles>=60&&sliceBands>sliceCycles&&sliceRestarts>0&&sliceGhosts>0);
 // Invalidation: reset epoch, instancing mode and matrix changes refuse rects.
 {std::vector<CasterBounds> boxes;size_t models=0;float moved[16];std::memcpy(moved,identity,sizeof moved);moved[12]=.01f;
  assert(!warm.changedBounds(moved,record,boxes,models));auto copy=record;copy.instancing=!copy.instancing;assert(!warm.changedBounds(identity,copy,boxes,models));
  warm.reset();assert(warm.update(&d,s,999));assert(!warm.changedBounds(identity,record,boxes,models));
  GpuCache::ContentRecord none;assert(!warm.changedBounds(identity,none,boxes,models));}
 // Metadata-capped plans have no slices: always a full redraw.
 {auto l=limits;l.planMetadataBytes=0;GpuCache capped(l);IDirect3DDevice9 c;assert(capped.update(&c,s,1));GpuCache::ContentRecord r;capped.record(identity,r);assert(!r.valid);capped.reset();}
 // Footprint helper: invalid input fails open, clamping and margins hold.
 {NorthlightShadowBounds::TexelRect f;assert(!NorthlightShadowBounds::texelFootprint({0,0,0},{INFINITY,0,0},identity,Size,Margin,f));
  assert(NorthlightShadowBounds::texelFootprint({-5,-5,0},{5,5,0},identity,Size,Margin,f)&&f.left==0&&f.top==0&&f.right==Size&&f.bottom==Size);
  assert(NorthlightShadowBounds::texelFootprint({0,0,0},{0,0,0},identity,Size,Margin,f)&&f.left<=Size/2-Margin&&f.right>=Size/2+Margin&&f.area()<=(2*Margin+3)*(2*Margin+3));
  assert(NorthlightShadowBounds::texelFootprint({5,5,0},{6,6,0},identity,Size,Margin,f)&&f.empty());}
 // Scale benchmark (fake device, no raster): 1280 texel cascade, 400 models x
 // 10 placements of ~6 texel casters, half clustered; one model becomes ready.
 for(bool clustered:{true,false}){IDirect3DDevice9 bd;GpuCache big(limits);auto scene=std::make_shared<Snapshot>();scene->map="b";scene->mapGeneration=1;std::uniform_real_distribution<float> u(-.98f,.98f),near(-.05f,.05f);
  auto tiny=[&](std::string key){auto m=model(key,depth(rng));for(auto& v:m->vertices){v.x*=.05f;v.y*=.05f;}m->batches[0].high={.01f,.01f,m->batches[0].high.z};return m;};
  for(unsigned i=0;i<400;++i){auto m=tiny("k"+std::to_string(1000+i));const float cx=u(rng),cy=u(rng);for(unsigned k=0;k<10;++k)add(*scene,m,i*100+k,{i%2?cx+near(rng):u(rng),i%2?cy+near(rng):u(rng),0});}
  assert(big.update(&bd,scene,1));big.signature(identity);GpuCache::ContentRecord rec;big.record(identity,rec);auto t0=std::chrono::steady_clock::now();big.record(identity,rec);const double recordMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
  auto next=std::make_shared<Snapshot>(*scene);next->preparedIdentity=nullptr;auto m=tiny("k0500x");const float cx=u(rng),cy=u(rng);for(unsigned k=0;k<10;++k)add(*next,m,999000+k,{clustered?cx+near(rng):u(rng),clustered?cy+near(rng):u(rng),0});
  assert(big.update(&bd,next,2));bd.drawnRecords.clear();assert(big.draw(&bd,identity));const size_t fullCalls=big.stats().drawCalls;
  t0=std::chrono::steady_clock::now();std::vector<CasterBounds> boxes;size_t models=0;assert(big.changedBounds(identity,rec,boxes,models)&&models==1);
  std::vector<NorthlightShadowBounds::TexelRect> fp,rs;for(const auto& b:boxes){NorthlightShadowBounds::TexelRect f;assert(NorthlightShadowBounds::texelFootprint(b.low,b.high,identity,1280,Margin,f));fp.push_back(f);}
  NorthlightShadowBounds::dirtyRects(fp,1280,64,8,rs);const double diffMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
  long long area=0;for(const auto& r:rs)area+=r.area();bd.drawnRecords.clear();assert(big.draw(&bd,identity,&rs,1280,Margin));const size_t multiCalls=big.stats().drawCalls;
  NorthlightShadowBounds::TexelRect all;for(const auto& r:rs)all=NorthlightShadowBounds::unite(all,r);std::vector<NorthlightShadowBounds::TexelRect> one={all};
  std::vector<size_t> costs;big.drawCalls(identity,{nullptr,&rs,&one},1280,Margin,costs);assert(costs[0]==fullCalls&&costs[1]==multiCalls); // estimate is exact
  bd.drawnRecords.clear();assert(big.draw(&bd,identity,&one,1280,Margin)&&big.stats().drawCalls==costs[2]);
  const size_t chosen=std::min({costs[0],costs[1],costs[2]});
  std::cout<<(clustered?"clustered":"scattered")<<" readiness at scale: "<<rs.size()<<" rects "<<100.0*double(area)/(1280.0*1280.0)<<"% texels, static draw calls multi="<<costs[1]<<" bounding="<<costs[2]<<" full="<<costs[0]<<" chosen="<<chosen<<", record "<<recordMs<<"ms diff+rects "<<diffMs<<"ms\n";
  big.reset();}
 warm.reset();assert(FakeResource::alive==0);
 assert(naiveDiffers==1&&removals>0);
 std::cout<<"static cache slices: "<<sliceCycles<<" sliced cycles ("<<sliceBands<<" bands, "<<sliceRestarts<<" restarts, "<<sliceGhosts<<" add-then-remove, "<<sliceReverts<<" reverts) bit-identical to full redraw\n";
 std::cout<<"static cache dirty rects: "<<partial<<" scissored redraws bit-identical to full redraw (scrambled shared depth), avg rect "<<100.0*double(partialTexels)/(double(partial)*Size*Size)<<"% texels (max "<<maxRects<<" disjoint rects), draws "<<drawn<<" vs full "<<fullDraws<<" (skipped commands "<<skipped<<"); naive append order-dependent at D24 tie: yes; invalidation/reset/metadata-cap/footprint checks passed\n";
}
