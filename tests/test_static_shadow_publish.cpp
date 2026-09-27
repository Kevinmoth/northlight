// 0.3.138 publication/ownership invalidation: randomized snapshot streams
// (moving windows, inserts/removals anywhere, reorders, duplicate identities,
// moves, revisions, owner-set changes, idle plans) against the 0.3.137 path.
// grouped-only: byte-identical invalidation state of every plan slot and stats.
// exact diff (+idle release): every prepared plan field-identical to 0.3.137's
// and to a cold full build (fewer spurious invalidations, same plans).
#define STATIC_SHADOW_GPU_TEST
#include "test_static_shadow_fake_d3d.h"
#include "static_shadow_gpu.h"
#include "world_math.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace StaticShadow;
static uint32_t rng=0x138138;
static uint32_t nx(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static float rf(float a,float b){return a+(b-a)*float(nx()>>8)/16777216.f;}
static unsigned pick(unsigned n){return n?nx()%n:0;}
static std::shared_ptr<Model> model(const std::string& key,uint64_t revision){auto m=std::make_shared<Model>();m->key=key;m->contentRevision=revision;m->index16=true;
 NorthlightGI::WorldMaterial mat;mat.alphaCutoff=.37f;mat.width=mat.height=1;mat.rgba={255,255,255,127};m->materials={mat};m->materialKeys={1};
 const unsigned n=1+pick(4);const float s=rf(.5f,12);for(unsigned b=0;b<n;++b){m->vertices.insert(m->vertices.end(),{{0,0,.5f,0,0},{.2f,0,.5f,1,0},{0,.2f,.5f,0,1}});m->indices.insert(m->indices.end(),{3*b,3*b+1,3*b+2});m->batches.push_back({3*b,3,0,{rf(-s,0),rf(-s,0),rf(-2,0)},{rf(0,s),rf(0,s),rf(1,3*s)}});}return m;}
static std::shared_ptr<Snapshot> finish(const std::vector<Placement>& ps,const std::map<std::string,std::shared_ptr<Model>>& models,bool prepared,uint64_t geometry){
 auto s=std::make_shared<Snapshot>();s->map="Kalimdor";s->mapGeneration=2;for(const auto& p:ps){s->placements.push_back(p);s->models[p.modelKey]=models.at(p.modelKey);}
 if(prepared){for(size_t i=0;i<s->placements.size();++i)s->placementGroups[s->placements[i].modelKey].push_back(i);s->preparedIdentity=s.get();s->geometryRevision=geometry;}return s;}
int main(int argc,char**argv){
 const unsigned trials=argc>1?unsigned(atoi(argv[1])):120;unsigned long long states=0,plans=0,coldPlans=0;uint64_t diffs=0,fallbacks=0,testsNew=0,testsRef=0,idle=0;double msNew=0,msRef=0;
 for(unsigned trial=0;trial<trials;++trial){
  const bool strict=trial%2==0; /* strict: grouped only, same invalidation state */
  GpuCache::Limits l;l.uploadMs=1e9;l.uploadBytes=pick(3)?(1u<<30):unsigned(300+pick(3000));l.retainFrames=1+pick(30);if(!pick(5))l.planMetadataBytes=size_t(400+pick(3000));
  GpuCache fast(l),ref(l);ref.referencePublish();if(strict)fast.exactPublishOnly(false,true,false);else fast.exactPublishOnly(true,true,true,5+pick(60));
  IDirect3DDevice9 d1,d2;
  std::map<std::string,std::shared_ptr<Model>> models;const unsigned modelCount=6+pick(40);
  for(unsigned m=0;m<modelCount;++m){std::string key="world/kalimdor/durotar/passivedoodads/"+std::string(pick(2)?"barricade/":"fence/")+std::to_string(m*7919%1000)+".m2";models[key]=model(key,1);}
  const Vec3 c{rf(-9000,9000),rf(-9000,9000),rf(0,200)};
  std::vector<Placement> world;uint64_t uid=1;
  auto fresh=[&](){Placement p;p.uid=uid++;p.category=1+pick(3);auto it=models.begin();std::advance(it,pick(unsigned(models.size())));p.modelKey=it->first;p.translation=c+Vec3(rf(-400,400),rf(-400,400),rf(-20,40));
   if(pick(4)){const float a=rf(0,6.283f);const float r[9]={std::cos(a),-std::sin(a),0,std::sin(a),std::cos(a),0,0,0,1};std::memcpy(p.matrix,r,36);}else for(float& x:p.matrix)x=rf(-2,2);return p;};
  for(unsigned i=0;i<200+pick(900);++i)world.push_back(fresh());
  std::sort(world.begin(),world.end(),[](const Placement& a,const Placement& b){return int(a.translation.x/64)!=int(b.translation.x/64)?int(a.translation.x/64)<int(b.translation.x/64):a.uid<b.uid;});
  float x0=c.x-400,width=rf(300,700);auto window=[&](){std::vector<Placement> ps;for(const auto& p:world)if(p.translation.x>=x0&&p.translation.x<x0+width)ps.push_back(p);return ps;};
  std::vector<Placement> current=window();uint64_t geometry=1;auto s=finish(current,models,true,geometry++);
  std::vector<std::array<float,16>> matrices;
  auto addMatrix=[&](){std::array<float,16> m;const float e=rf(5,80)*.01745329252f,z=rf(-3.14f,3.14f);NorthlightWorldMath::shadowMatrixFrom(NorthlightWorldMath::shadowFrame(c+Vec3(rf(-150,150),rf(-150,150),0),NorthlightWorldMath::quantizeDirection({std::cos(e)*std::cos(z),std::cos(e)*std::sin(z),std::sin(e)}),pick(2)?48.f:192.f),(pick(2)?48.f:192.f)*1.25f,m.data());if(!pick(50))m[pick(16)]=NAN;matrices.push_back(m);};
  for(int i=0;i<3;++i)addMatrix();
  std::vector<Placement> covered;
  for(unsigned frame=1;frame<=90;++frame){
   const unsigned op=pick(12);
   if(op<=3){ /* snapshot change */
    std::vector<Placement> n;
    if(op==0){x0+=rf(-60,60);n=window();}
    else{n=current;for(unsigned e=1+pick(6);e--;){const unsigned w=pick(7);
     if(w==0&&!n.empty())n.erase(n.begin()+pick(unsigned(n.size())));
     else if(w==1)n.insert(n.begin()+pick(unsigned(n.size()+1)),fresh());
     else if(w==2&&n.size()>1)std::swap(n[pick(unsigned(n.size()))],n[pick(unsigned(n.size()))]);
     else if(w==3&&!n.empty())n[pick(unsigned(n.size()))].translation.y+=rf(-30,30);
     else if(w==4&&!n.empty())n.push_back(n[pick(unsigned(n.size()))]); /* duplicate identity */
     else if(w==5){auto it=models.begin();std::advance(it,pick(unsigned(models.size())));it->second=model(it->first,it->second->contentRevision+1);}
     else if(!n.empty()){const Placement p=n[pick(unsigned(n.size()))];n.erase(n.begin()+pick(unsigned(n.size())));n.push_back(p);}}}
    current=n;s=finish(current,models,pick(4)!=0,geometry++);
   }else if(op==4){covered.clear();for(const auto& p:current)if(!pick(4)){covered.push_back(p);if(!pick(10))covered.back().translation.z+=1;}fast.setCoveredPlacements(covered);ref.setCoveredPlacements(covered);}
   else if(op==5){addMatrix();if(matrices.size()>11)matrices.erase(matrices.begin()+pick(unsigned(matrices.size())));}
   else if(op==6&&s->preparedIdentity==s.get())s=finish(current,models,true,s->geometryRevision); /* same geometry */
   const bool uploads=pick(4)!=0;
   auto t=std::chrono::steady_clock::now();fast.update(&d1,s,frame,uploads);msNew+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
   t=std::chrono::steady_clock::now();ref.update(&d2,s,frame,uploads);msRef+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
   assert(fast.stats().readyModels==ref.stats().readyModels);
   if(strict){assert(fast.planStateDigest()==ref.planStateDigest());++states;}
   // Prepare a varying subset (some matrices go idle), same calls on both.
   for(size_t i=0;i<matrices.size();++i)if(i<2||pick(3)==0){const float* m=matrices[i].data();const auto x=fast.planDigest(m,strict),y=ref.planDigest(m,strict);assert(x==y);++plans;
    if(!strict&&!pick(4)&&uploads&&l.uploadBytes==(1u<<30)){GpuCache cold(l);IDirect3DDevice9 d3;cold.setCoveredPlacements(covered);cold.update(&d3,s,frame);
     if(cold.stats().readyModels==fast.stats().readyModels&&cold.stats().pendingModels==0&&fast.stats().pendingModels==0){assert(cold.planDigest(m,false)==x);++coldPlans;}cold.reset();}}
   if(strict){assert(fast.planStateDigest()==ref.planStateDigest());++states;}
  }
  diffs+=fast.stats().publishDiffs;fallbacks+=fast.stats().publishDiffFallbacks;testsNew+=fast.stats().ownerTests;testsRef+=ref.stats().ownerTests;idle+=fast.stats().planIdleReleases;
  fast.reset();ref.reset();
 }
 assert(FakeResource::alive==0);
 std::cout<<"static publish: "<<trials<<" randomized streams, "<<states<<" byte-identical invalidation states (grouped), "<<plans<<" field-identical plans vs 0.3.137, "<<coldPlans<<" equal to a cold build; exact diffs="<<diffs<<" fallbacks="<<fallbacks<<" idleReleases="<<idle<<"; owner coarse tests new="<<testsNew<<" 0.3.137="<<testsRef<<"; update ms new="<<msNew<<" 0.3.137="<<msRef<<"\n";
}
