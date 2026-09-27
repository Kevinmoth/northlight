#include "world_streaming.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <memory>
using namespace NorthlightWorldStreaming;
struct Resource {
    static int live;int id;
    explicit Resource(int value):id(value){++live;}
    ~Resource(){--live;}
};
int Resource::live=0;
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
    std::printf("{\"allocation_rollback_cases\":%u,\"invalid_plans_rejected_before_allocation\":%u,\"retry_wraparound\":true,\"stale_publication_frames_rejected\":1000,\"old_resource_survives_failures\":true}\n",rollbackCases,rejected);
}
