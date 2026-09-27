#pragma once
#include "world_mesh_plan.h"
#include <cmath>
#include <cstdint>
#include <string>

namespace NorthlightWorldStreaming {
// Start the next bounded build while the current 96-unit coverage still has
// 64 units of travel left. Coverage and GI solve radii are not enlarged.
inline constexpr float GeometryRefreshDistance=32.f;
inline constexpr float GeometryCoverageDistance=96.f;
inline bool within(NorthlightGI::Vec3 center,NorthlightGI::Vec3 camera,double radius){
    const double x=double(center.x)-camera.x,y=double(center.y)-camera.y,z=double(center.z)-camera.z;
    return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&x*x+y*y+z*z<=radius*radius;
}
inline bool needsGeometry(const std::string& map,NorthlightGI::Vec3 center,
                          const std::string& currentMap,NorthlightGI::Vec3 camera){
    return map!=currentMap||!within(center,camera,GeometryRefreshDistance);
}
struct Retry {
    uint32_t failedAt=0;bool waiting=false;
    bool ready(uint32_t now)const{return !waiting||uint32_t(now-failedAt)>=1000u;}
    void fail(uint32_t now){failedAt=now;waiting=true;}
    void clear(){waiting=false;}
};
inline bool applicable(const std::string& map,NorthlightGI::Vec3 center,
                       const std::string& currentMap,NorthlightGI::Vec3 camera) {
    return map==currentMap&&within(center,camera,GeometryCoverageDistance);
}
// Source and plan are immutable and retained together by the caller. Validate
// their identity and all upload sizes BEFORE making any resource allocation.
// Per-index validity is established by buildUploadPlan on the worker.
inline const char* validate(const NorthlightWorldMesh::WorldMeshUploadPlan& p,const NorthlightGI::WorldScene& s) {
    if(p.source!=&s)return "plan/source identity mismatch";
    if(!p.vertexCount||!p.triangleCount||p.batches.empty()||p.materials.empty())return "empty upload plan";
    if(p.vertexCount!=s.vertices.size()||p.triangleCount!=s.triangles.size()||p.materials.size()!=s.materials.size())return "plan/source count mismatch";
    if(p.vertexBytes!=uint64_t(s.vertices.size())*sizeof(NorthlightGI::WorldVertex)||p.vertexBytes>UINT32_MAX||
       p.indexBytes!=uint64_t(p.indices.size())*sizeof(uint32_t)||p.indexBytes>UINT32_MAX||
       uint64_t(p.triangleCount)*3!=p.indices.size())return "invalid buffer sizes";
    uint64_t expected=0;
    for(const auto& b:p.batches){
        if(!b.count||b.material>=p.materials.size()||b.start!=expected)return "invalid batch range";
        expected+=uint64_t(b.count)*3;if(expected>p.indices.size())return "batch exceeds index buffer";
    }
    if(expected!=p.indices.size())return "incomplete batch coverage";
    for(const auto& m:p.materials)
        if(!m.width||!m.height||m.width>4096||m.height>4096||uint64_t(m.width)*m.height*4!=m.bgra.size())return "invalid texture size";
    return nullptr;
}
enum class BeginResult { Ready, InvalidPlan, VertexFailure, IndexFailure };
// Allocation transaction shared by runtime and fake-resource tests. The rollback
// callback releases only new pending resources; the last committed mesh is never
// handed to this function and therefore cannot be destroyed by a failed attempt.
template<class Vertex,class Index,class Rollback>
BeginResult begin(const NorthlightWorldMesh::WorldMeshUploadPlan& p,const NorthlightGI::WorldScene& s,
                  Vertex vertex,Index index,Rollback rollback,const char*& reason) {
    reason=validate(p,s);
    if(reason){rollback();return BeginResult::InvalidPlan;}
    if(!vertex(uint32_t(p.vertexBytes))){rollback();return BeginResult::VertexFailure;}
    if(!index(uint32_t(p.indexBytes))){rollback();return BeginResult::IndexFailure;}
    return BeginResult::Ready;
}
} // namespace NorthlightWorldStreaming
