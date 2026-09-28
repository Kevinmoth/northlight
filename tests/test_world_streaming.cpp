#include "world_streaming.h"
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>
#include <vector>
#include <memory>
using namespace NorthlightWorldStreaming;
struct Resource {
    static int live;int id;
    explicit Resource(int value):id(value){++live;}
    ~Resource(){--live;}
};
int Resource::live=0;

// 0.3.169 protocol model with the production predicates: the render thread adopts and retires
// per frame, the builder refreshes on 32 units of lead-point travel and cancels a build more
// than 96 from the eye (after the BVH and after terrain), and the worker drops a delivery more
// than 96 from the eye after its adoption latency. Third-person eye at distance d behind the
// player; a flick turns the camera 180 degrees in 0.25 s, holds 2 s and turns back.
struct Coverage {float maxDistance=0;unsigned skipped=0,builds=0;std::vector<NorthlightGI::Vec3> centres;};
static Coverage simulate(float speed,float buildSeconds,bool lead,float spinAt=-1,float seconds=14,float pivotDistance=34){
    using V=NorthlightGI::Vec3;const float d=34,dt=1.f/30,latency=.15f,pi=3.14159265f;
    auto pose=[&](float t,V& eye,V& pivot){const float p=std::max(0.f,t-.5f)*speed;float angle=0;
        if(spinAt>=0){const float s=t-spinAt;if(s>=0&&s<.25f)angle=pi*s/.25f;else if(s>=.25f&&s<2.25f)angle=pi;else if(s>=2.25f&&s<2.5f)angle=pi*(1-(s-2.25f)/.25f);}
        const V forward(std::cos(angle),std::sin(angle),0);eye=V(p-d*forward.x,-d*forward.y,0);pivot=V(eye.x+pivotDistance*forward.x,eye.y+pivotDistance*forward.y,0);};
    auto distance=[](V a,V b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));};
    const std::string map="Azeroth";Motion motion;Coverage out;
    V eye,pivot;pose(0,eye,pivot);V builtCenter=eye,published=eye,active=eye;bool haveActive=true,building=false,handing=false,wanted=false;
    V buildCenter,handCenter;float buildStart=0,handAt=0;int checks=0;long lastCell=LONG_MIN;
    for(int frame=0;frame*dt<seconds;++frame){const float t=frame*dt;pose(t,eye,pivot);
        motion.update(map,eye,pivot,uint32_t(1000+std::lround(t*1000)));
        const V target=lead?leadCenter(eye,motion.velocity(),buildSeconds):eye;
        const long cell=long(std::floor(eye.x/8))*100000+long(std::floor(eye.y/8));const bool request=cell!=lastCell;lastCell=cell;
        if(building){
            for(;checks<2&&t-buildStart>=(checks?.85f:.5f)*buildSeconds;++checks)if(!applicable(map,buildCenter,map,eye)){building=false;break;}
            if(building&&t-buildStart>=buildSeconds){building=false;handing=true;handCenter=buildCenter;handAt=t;builtCenter=buildCenter;}
        }
        if(!building&&(request||wanted)&&(wanted||needsGeometry(map,builtCenter,map,target))){building=true;buildCenter=target;buildStart=t;checks=0;wanted=false;++out.builds;out.centres.push_back(target);}
        if(handing&&t-handAt>=latency){handing=false;if(applicable(map,handCenter,map,eye))published=handCenter;else wanted=true;}
        if(adopts(map,published,true,haveActive,map,eye)){active=published;haveActive=true;}
        if(haveActive&&!retained(map,active,map,eye))haveActive=false;
        if(haveActive)out.maxDistance=std::max(out.maxDistance,distance(active,eye));else ++out.skipped;
    }
    return out;
}
int main(){
    NorthlightGI::WorldScene scene;scene.vertices.resize(3);scene.materials.resize(1);
    scene.vertices[1].position.x=1;scene.vertices[2].position.y=1;
    scene.triangles.push_back({0,1,2,0});
    NorthlightWorldMesh::WorldMeshUploadPlan plan;std::string error;
    assert(NorthlightWorldMesh::buildUploadPlan(scene,plan,error));assert(!validate(plan,scene));
    unsigned rollbackCases=0,rejected=0;
    // Allocation failures at either stage, including APIs returning an allocated
    // object alongside failure, must release pending objects and retain old GPU
    // resources. A successful later attempt can commit without lost ownership.
    for(int failedStage=0;failedStage<2;++failedStage){
        auto committed=std::make_unique<Resource>(77);
        std::unique_ptr<Resource> vertex,index;int calls=0,rollbacks=0;const char* reason=nullptr;
        auto allocate=[&](uint32_t bytes,std::unique_ptr<Resource>& target){assert(bytes>0);target=std::make_unique<Resource>(calls+1);return calls++!=failedStage;};
        auto result=begin(plan,scene,[&](uint32_t n){return allocate(n,vertex);},[&](uint32_t n){return allocate(n,index);},[&]{vertex.reset();index.reset();++rollbacks;},reason);
        assert(result==(failedStage==0?BeginResult::VertexFailure:BeginResult::IndexFailure));
        assert(calls==failedStage+1&&rollbacks==1&&Resource::live==1&&committed->id==77);++rollbackCases;
        Retry retry;retry.fail(12345);assert(!retry.ready(13344)&&retry.ready(13345));
        result=begin(plan,scene,[&](uint32_t){vertex=std::make_unique<Resource>(10);return true;},[&](uint32_t){index=std::make_unique<Resource>(11);return true;},[&]{assert(false);},reason);
        assert(result==BeginResult::Ready&&Resource::live==3&&committed->id==77);
        committed.swap(vertex);vertex.reset();assert(committed->id==10&&Resource::live==2);
    }
    assert(Resource::live==0);
    auto reject=[&](const NorthlightWorldMesh::WorldMeshUploadPlan& bad,const NorthlightGI::WorldScene& source){
        int calls=0,rollbacks=0;const char* reason=nullptr;
        auto result=begin(bad,source,[&](uint32_t){++calls;return true;},[&](uint32_t){++calls;return true;},[&]{++rollbacks;},reason);
        assert(result==BeginResult::InvalidPlan&&reason&&calls==0&&rollbacks==1);++rejected;
    };
    auto bad=plan;bad.vertexBytes=0;reject(bad,scene);
    bad=plan;bad.indexBytes=uint64_t(UINT32_MAX)+1;reject(bad,scene);
    bad=plan;bad.vertexCount=0;reject(bad,scene);
    bad=plan;bad.triangleCount=2;reject(bad,scene);
    bad=plan;bad.indices.pop_back();reject(bad,scene);
    bad=plan;bad.batches[0].count=2;reject(bad,scene);
    bad=plan;bad.batches[0].material=1;reject(bad,scene);
    bad=plan;bad.materials[0].bgra.pop_back();reject(bad,scene);
    bad=plan;bad.batches[0].start=1;reject(bad,scene);
    auto anotherScene=scene;reject(plan,anotherScene);
    Retry retry;retry.fail(0xfffffff0u);assert(!retry.ready(983u)&&retry.ready(984u));retry.clear();assert(retry.ready(12));
    using V=NorthlightGI::Vec3;
    assert(applicable("Azeroth",V(),"Azeroth",V(96,0,0)));
    assert(!applicable("Azeroth",V(),"Azeroth",V(96.01f,0,0)));
    assert(!applicable("Azeroth",V(),"Kalimdor",V()));
    assert(!applicable("Azeroth",V(),"Azeroth",V(std::numeric_limits<float>::quiet_NaN(),0,0)));
    // A stale publication cannot be re-adopted on any subsequent stationary frame.
    unsigned adoptions=0;for(int frame=0;frame<1000;++frame)adoptions+=applicable("Azeroth",V(),"Azeroth",V(100,0,0));assert(adoptions==0);
    // 0.3.169 hold and adoption predicates.
    assert(retained("Azeroth",V(),"Azeroth",V(160,0,0))&&!retained("Azeroth",V(),"Azeroth",V(160.01f,0,0)));
    assert(retained("Azeroth",V(),"Azeroth",V(120,0,0))&&!applicable("Azeroth",V(),"Azeroth",V(120,0,0))); /* held, not applicable */
    assert(!retained("Azeroth",V(),"Kalimdor",V())); /* a map change retires at once */
    assert(!retained("Azeroth",V(),"Azeroth",V(std::numeric_limits<float>::quiet_NaN(),0,0)));
    assert(adopts("Azeroth",V(150,0,0),true,true,"Azeroth",V()));   /* a drawable replacement within the hard limit */
    assert(!adopts("Azeroth",V(161,0,0),true,true,"Azeroth",V()));  /* beyond it: never adopted, never held */
    assert(!adopts("Azeroth",V(),true,true,"Kalimdor",V()));
    assert(!adopts("Azeroth",V(),false,true,"Azeroth",V()));        /* an error snapshot never replaces a drawable one */
    assert(adopts("Azeroth",V(),false,false,"Azeroth",V()));        /* ... but replaces nothing or another error snapshot */
    // Frame sequence: A held at 120 while an error snapshot is published, then B adopted at 150.
    {struct S {V center;bool bvh;};S a{V(),true},error{V(100,0,0),false},b{V(140,0,0),true};const S* active=&a;unsigned frames=0,retired=0;
     auto frame=[&](const S* published,V eye){if(published&&published!=active&&adopts("Azeroth",published->center,published->bvh,active&&active->bvh,"Azeroth",eye))active=published;
         if(active&&!retained("Azeroth",active->center,"Azeroth",eye)){active=nullptr;++retired;}++frames;};
     frame(&error,V(120,0,0));assert(active==&a&&!retired);frame(&b,V(150,0,0));assert(active==&b);frame(&b,V(290,0,0));assert(active==&b);
     frame(&b,V(301,0,0));assert(!active&&retired==1);frame(&error,V(301,0,0));assert(!active); /* the error snapshot is 201 away */
     frame(&error,V(160,0,0));assert(active==&error&&frames==6);}
    // Geometry lead: none below 10 u/s, speed x expected build time, capped at 60.
    assert(leadCenter(V(1,2,3),V(9.9f,0,0),1).x==1);
    {const V c=leadCenter(V(),V(0,21.7f,0),1);assert(std::fabs(c.y-21.7f)<1e-4f&&c.x==0);}
    {const V c=leadCenter(V(),V(70,0,0),1.5f);assert(std::fabs(c.x-60)<1e-4f);}
    assert(leadCenter(V(5,0,0),V(std::numeric_limits<float>::quiet_NaN(),0,0),1).x==5&&leadCenter(V(5,0,0),V(70,0,0),0).x==5);
    // Safety: the lead keeps the eye inside the 96-unit coverage of any region whose centre is
    // within the refresh of the lead point; the held snapshot keeps the near cascade
    // (pivot <= eye+80, radius 48) inside the +-288 local box.
    static_assert(GeometryLeadMax+GeometryRefreshDistance<GeometryCoverageDistance,"lead cap");
    static_assert(GeometryRetainDistance+80+48<=288,"near cascade inside the local box");
    // Motion: a third-person flick moves the eye but not the pivot; a teleport or a gap resets.
    {Motion m;m.update("A",V(),V(34,0,0),1000);for(uint32_t t=1033;t<=1250;t+=33)m.update("A",V(34*(1-std::cos(3.14159f*(t-1000)/250.f)),0,0),V(34,0,0),t);
     const V v=m.velocity();assert(std::sqrt(v.x*v.x+v.y*v.y)<1e-3f);
     m=Motion();for(uint32_t t=0;t<=4000;t+=20)m.update("A",V(.0217f*t,0,0),V(.0217f*t+30,0,0),t);assert(std::fabs(m.velocity().x-21.7f)<.1f);
     m.update("A",V(500,0,0),V(530,0,0),4020);assert(m.velocity().x==0);                       /* teleport */
     m.update("A",V(501,0,0),V(531,0,0),4040);m.update("B",V(501,0,0),V(531,0,0),4060);assert(m.velocity().x==0&&m.map=="B"); /* map change */
     m.update("B",V(520,0,0),V(550,0,0),4800);assert(m.velocity().x==0);}                        /* loading gap */
    // Protocol coverage. Flying mount 310 pct (21.7 u/s): every flick phase at every logged build time.
    unsigned phases=0;float flying=0,flyingOff=0;unsigned flyingOffSkipped=0;
    for(float build:{.66f,1.f,1.17f,1.5f})for(int i=0;i<80;++i){const float at=2+i*.05f;++phases;
        const auto on=simulate(21.7f,build,true,at);assert(!on.skipped&&on.maxDistance<GeometryRetainDistance);flying=std::max(flying,on.maxDistance);
        const auto off=simulate(21.7f,build,false,at);flyingOff=std::max(flyingOff,off.maxDistance);flyingOffSkipped+=off.skipped;}
    assert(flyingOffSkipped>0); /* without the lead a 96-unit builder check drops the flick build */
    // GM speed 10 (70 u/s): straight travel for build times up to the logged fast-travel maximum.
    float gm=0;for(float build:{.66f,1.f,1.17f}){const auto on=simulate(70,build,true);assert(!on.skipped);gm=std::max(gm,on.maxDistance);}
    // Walking: the lead is off (below 10 u/s), so build centres are unchanged.
    {const auto on=simulate(7,1,true,-1,60),off=simulate(7,1,false,-1,60);assert(on.builds==off.builds);
     for(size_t i=0;i<on.centres.size();++i)assert(on.centres[i].x==off.centres[i].x&&on.centres[i].y==off.centres[i].y);}
    // No extra builds: over 60 s the build count follows distance travelled (+1 for the lead start).
    unsigned builds[2][3]={};int column=0;for(float speed:{7.f,21.7f,70.f}){for(int lead=0;lead<2;++lead)builds[lead][column]=simulate(speed,1,lead!=0,-1,60).builds;
        assert(builds[1][column]<=builds[0][column]+1);++column;}
    std::printf("{\"allocation_rollback_cases\":%u,\"invalid_plans_rejected_before_allocation\":%u,\"retry_wraparound\":true,\"stale_publication_frames_rejected\":1000,\"old_resource_survives_failures\":true,\"hold_flying_phases\":%u,\"hold_flying_max\":%.1f,\"hold_flying_max_without_lead\":%.1f,\"hold_flying_skipped_without_lead\":%u,\"hold_gm_straight_max\":%.1f,\"builds_60s_7_21_70\":[%u,%u,%u],\"builds_60s_with_lead\":[%u,%u,%u]}\n",rollbackCases,rejected,
        phases,flying,flyingOff,flyingOffSkipped,gm,builds[0][0],builds[0][1],builds[0][2],builds[1][0],builds[1][1],builds[1][2]);
}
