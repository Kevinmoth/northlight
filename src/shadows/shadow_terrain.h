#pragma once
// Worker-side, asset-derived distant shadow casters. Does not extend the GI
// BVH, change celestial directions, or depend on the camera's viewing angle.
#include "world_mesh_plan.h"
#include <algorithm>
#include <cmath>

namespace NorthlightShadowTerrain {
using NorthlightGI::Vec3;
constexpr float Radius=928.f;
// Far cached cascade: radius 240 including its margin, depth half-span 640.
// Its enclosing sphere is <725u. Add 80u maximum camera orbit distance and
// 96u publication/upload applicability margin: 928u covers every direction.
// 0.3.169: the render thread holds a snapshot past 96u until its replacement
// applies (hard limit 160u, world_streaming.h). Between 123u and 160u, low-sun
// terrain casters at the extreme far-cascade corner can be missing for up to
// one build; the near cascade stays inside the local box.
constexpr double Origin=17066.666666666666,Chunk=100.0/3.0;
inline std::pair<int,int> chunk(Vec3 a,Vec3 b,Vec3 c){
    const double x=std::floor((Origin-(double(a.y)+b.y+c.y)/3)/Chunk);
    const double y=std::floor((Origin-(double(a.x)+b.x+c.x)/3)/Chunk);
    if(!std::isfinite(x)||!std::isfinite(y)||x<0||y<0||x>=1024||y>=1024)return {-1,-1};
    const double lx=Origin-(y+1)*Chunk,hx=Origin-y*Chunk,ly=Origin-(x+1)*Chunk,hy=Origin-x*Chunk;
    for(auto p:{a,b,c})if(p.x<lx-.03||p.x>hx+.03||p.y<ly-.03||p.y>hy+.03)return {-1,-1};
    return {int(x),int(y)};
}
inline bool outside(Vec3 lo,Vec3 hi,Vec3 a,Vec3 b){
    return hi.x<a.x||lo.x>b.x||hi.y<a.y||lo.y>b.y||hi.z<a.z||lo.z>b.z;
}
// Snapshot positions/indices have already passed the terrain capture validator.
// Original indices remain intact for point lights; this is an appended stream.
template<class Snapshot> void appendLiveDirectional(const Snapshot& snapshot,
        const std::set<std::pair<int,int>>& fixed,uint32_t offset,std::vector<uint32_t>& out){
    bool anyFixed=false,anyLive=false;
    for(const auto& c:snapshot.bounds.chunks){if(fixed.count({c.x,c.y}))anyFixed=true;else anyLive=true;}
    if(!anyFixed){for(auto i:snapshot.indices)out.push_back(i+offset);return;}
    if(!anyLive)return;
    auto position=[&](uint32_t i){const auto& p=snapshot.positions[i];return Vec3(p.x,p.y,p.z);};
    for(size_t i=0;i<snapshot.indices.size();i+=3){
        const auto a=snapshot.indices[i],b=snapshot.indices[i+1],c=snapshot.indices[i+2];
        if(fixed.count(chunk(position(a),position(b),position(c))))continue;
        out.insert(out.end(),{a+offset,b+offset,c+offset});
    }
}
inline bool build(const NorthlightGI::WorldScene& local,const NorthlightGI::WorldScene& terrain,
                  Vec3 center,NorthlightWorldMesh::WorldMeshUploadPlan& output,std::string& error,
                  const NorthlightGI::AllocationAdmission& admit = {},float radius=Radius){
    try {
    const Vec3 low=center-Vec3(288,288,320),high=center+Vec3(288,288,320);
    auto allowed=[&](uint64_t bytes){if(!admit||admit(bytes))return true;error="Geometry allocation deferred";return false;};
    auto merged=std::make_shared<NorthlightGI::WorldScene>();
    // Reserve final upper bounds once; do not copy then immediately reallocate.
    if(!allowed((local.vertices.size()+terrain.vertices.size())*sizeof(NorthlightGI::WorldVertex)))return false;
    merged->vertices.reserve(local.vertices.size()+terrain.vertices.size());
    if(!allowed((local.triangles.size()+terrain.triangles.size())*sizeof(NorthlightGI::WorldTriangle)))return false;
    merged->triangles.reserve(local.triangles.size()+terrain.triangles.size());
    merged->vertices.insert(merged->vertices.end(),local.vertices.begin(),local.vertices.end());
    merged->triangles.insert(merged->triangles.end(),local.triangles.begin(),local.triangles.end());
    if(!allowed((local.materials.size()+1)*sizeof(NorthlightGI::WorldMaterial)))return false;
    merged->materials.reserve(local.materials.size()+1);
    for(const auto& m:local.materials){if(!allowed(m.rgba.size()))return false;merged->materials.push_back(m);}
    if(!allowed(local.completePlacements.size()*sizeof(NorthlightGI::WorldPlacementCoverage)))return false;
    merged->completePlacements=local.completePlacements;
    std::set<std::pair<int,int>> fixed;
    if(!allowed(terrain.vertices.size()*sizeof(uint32_t)))return false;
    std::vector<uint32_t> remap(terrain.vertices.size(),UINT32_MAX);
    const uint32_t material=uint32_t(merged->materials.size());
    NorthlightGI::WorldMaterial opaque;opaque.terrain=true;
    merged->materials.push_back(opaque);
    for(const auto& t:terrain.triangles){
        if(t.v0>=terrain.vertices.size()||t.v1>=terrain.vertices.size()||t.v2>=terrain.vertices.size()||
           t.material>=terrain.materials.size()||!terrain.materials[t.material].terrain){error="Invalid shadow terrain";return false;}
        const Vec3 a=terrain.vertices[t.v0].position,b=terrain.vertices[t.v1].position,c=terrain.vertices[t.v2].position;
        const Vec3 lo(std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z}));
        const Vec3 hi(std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z}));
        // The existing local loader owns all triangles intersecting this box.
        if(!outside(lo,hi,low,high))continue;
        NorthlightGI::WorldTriangle added;added.material=material;
        uint32_t* dest[]={&added.v0,&added.v1,&added.v2};const uint32_t src[]={t.v0,t.v1,t.v2};
        for(unsigned i=0;i<3;++i){
            if(remap[src[i]]==UINT32_MAX){remap[src[i]]=uint32_t(merged->vertices.size());merged->vertices.push_back(terrain.vertices[src[i]]);}
            *dest[i]=remap[src[i]];
        }
        merged->triangles.push_back(added);
        const auto key=chunk(a,b,c);
        if(key.first<0||key.second<0)continue;
        const Vec3 cl(float(Origin-(key.second+1)*Chunk),float(Origin-(key.first+1)*Chunk),low.z);
        const Vec3 ch(float(Origin-key.second*Chunk),float(Origin-key.first*Chunk),high.z);
        // Only full distant chunks are immutable. Border chunks retain the
        // previous live/cached policy, with no claims on unprovided geometry.
        if(outside(cl,ch,low,high)&&cl.x>=center.x-radius&&ch.x<=center.x+radius&&
           cl.y>=center.y-radius&&ch.y<=center.y+radius)fixed.insert(key);
    }
    NorthlightWorldMesh::WorldMeshUploadPlan plan;
    if(!NorthlightWorldMesh::buildUploadPlan(*merged,plan,error,admit))return false;
    plan.ownedSource=merged;plan.fixedTerrainChunks=std::move(fixed);
    output=std::move(plan);return true;
    }catch(...){error="Cannot allocate shadow terrain mesh";return false;}
}
} // namespace NorthlightShadowTerrain
