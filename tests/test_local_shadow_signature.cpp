#include "world_mesh_plan.h"
#include "world_math.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <numeric>
using namespace NorthlightGI;
using namespace NorthlightWorldMesh;
namespace S=NorthlightLocalShadowSignature;
static void triangle(WorldScene& s,Vec3 p,uint32_t material=0){
    uint32_t first=uint32_t(s.vertices.size());
    s.vertices.push_back({p,{0,0,1},.1f,.2f});
    s.vertices.push_back({p+Vec3(2,0,0),{0,0,1},.7f,.2f});
    s.vertices.push_back({p+Vec3(0,2,0),{0,0,1},.1f,.8f});
    s.triangles.push_back({first,first+1,first+2,material});
}
static WorldMeshUploadPlan plan(const WorldScene& s){WorldMeshUploadPlan p;std::string error;assert(buildUploadPlan(s,p,error));assert(p.localShadowRecords);return p;}
static S::Digest sig(const WorldMeshUploadPlan& p,const float* m,const S::FixedChunks& fixed={}){return S::signature(*p.localShadowRecords,fixed,m);}
static void matrix(float* m,float x=0){std::fill(m,m+16,0);m[0]=m[5]=.01f;m[10]=.001f;m[12]=-x*.01f;m[14]=.5f;m[15]=1;}
int main(){
    WorldScene base;base.materials.resize(2);base.materials[0].alphaCutoff=0;auto& alpha=base.materials[1];alpha.width=alpha.height=2;alpha.alphaCutoff=.5f;alpha.rgba={0,1,2,255,3,4,5,0,6,7,8,128,9,10,11,255};
    triangle(base,{0,0,0});triangle(base,{200,0,0},1);triangle(base,{-200,0,0});
    auto initial=plan(base);float near[16],right[16];matrix(near);matrix(right,200);
    const auto nearOriginal=sig(initial,near),rightOriginal=sig(initial,right);
    assert(nearOriginal.triangles==1&&rightOriginal.triangles==1);

    // A changed model outside the left cascade does not invalidate that map.
    auto changed=base;changed.vertices[3].position.z+=7;auto moved=plan(changed);
    assert(sig(moved,near)==nearOriginal&&sig(moved,right)!=rightOriginal);
    float farCascade[16];matrix(farCascade);farCascade[0]=farCascade[5]=1.f/300;
    assert(sig(initial,farCascade).triangles==3);
    assert(sig(moved,farCascade)!=sig(initial,farCascade)); // Same center, far invalidates while near reuses.
    changed=base;triangle(changed,{400,0,0});auto more=plan(changed);
    assert(sig(more,near)==nearOriginal&&sig(more,right)==rightOriginal);
    changed=base;changed.triangles.erase(changed.triangles.begin());assert(sig(plan(changed),near)!=nearOriginal);

    // Same submitted coverage after source/material/index and triangle reorder.
    changed=base;std::reverse(changed.materials.begin(),changed.materials.end());
    for(auto& t:changed.triangles)t.material=1-t.material;
    std::reverse(changed.vertices.begin(),changed.vertices.end());
    for(auto& t:changed.triangles)for(auto* i:{&t.v0,&t.v1,&t.v2})*i=uint32_t(changed.vertices.size()-1-*i);
    std::reverse(changed.triangles.begin(),changed.triangles.end());auto reordered=plan(changed);
    assert(sig(reordered,near)==nearOriginal&&sig(reordered,right)==rightOriginal);

    changed=base;changed.materials[1].rgba[0]^=255;assert(sig(plan(changed),right)==rightOriginal); // RGB never used by shadow PS.
    changed.materials[1].rgba[3]^=255;assert(sig(plan(changed),right)!=rightOriginal);
    changed=base;changed.materials[1].alphaCutoff=.51f;assert(sig(plan(changed),right)!=rightOriginal);
    changed=base;changed.materials[1].addressU=3;assert(sig(plan(changed),right)!=rightOriginal);
    changed=base;changed.vertices[3].u+=.125f;assert(sig(plan(changed),right)!=rightOriginal);
    changed=base;changed.vertices[0].normal={1,0,0};assert(sig(plan(changed),near)==nearOriginal);
    changed=base;changed.materials[0].width=changed.materials[0].height=1;changed.materials[0].rgba={12,34,56,0};assert(sig(plan(changed),near)==nearOriginal);

    // Upstream geometry remains relevant even fully before the near plane.
    changed=base;triangle(changed,{0,0,-2000});assert(sig(plan(changed),near)!=nearOriginal);
    changed=base;triangle(changed,{0,0,2000});assert(sig(plan(changed),near)==nearOriginal); // Far-plane exclusion unchanged.
    changed=base;triangle(changed,{99,0,0});assert(sig(plan(changed),near)!=nearOriginal); // XY crossing edge.
    changed=base;triangle(changed,{100.00001f,0,0});assert(sig(plan(changed),near)!=nearOriginal); // Float error margin.

    WorldScene terrain;terrain.materials.resize(1);terrain.materials[0].terrain=true;
    triangle(terrain,{10,10,0});auto land=plan(terrain);assert(sig(land,near).triangles==0);
    const auto& b=land.batches.front();assert(b.chunkX>=0&&b.chunkY>=0);
    S::FixedChunks fixed{{b.chunkX,b.chunkY}};assert(sig(land,near,fixed).triangles==1);
    assert(sig(land,right,fixed).triangles==0);

    // Cached-frame memo scans once per publication, then stays idle even while
    // caller camera coordinates move (the cached matrix itself is unchanged).
    S::Memo memo;auto value=memo.get(initial.localShadowRecords.get(),1,{},near);
    for(unsigned i=0;i<10000;++i)assert(memo.get(initial.localShadowRecords.get(),1,{},near)==value);
    assert(memo.evaluations()==1);
    assert(memo.get(reordered.localShadowRecords.get(),2,{},near)==value&&memo.evaluations()==2);
    assert(memo.get(moved.localShadowRecords.get(),3,{},right)!=rightOriginal&&memo.evaluations()==3);
    S::Memo terrainMemo;assert(terrainMemo.get(land.localShadowRecords.get(),1,{},near).triangles==0);
    assert(terrainMemo.get(land.localShadowRecords.get(),2,fixed,near).triangles==1);

    // Published plans remain unchanged when preparation is denied.
    auto preserved=initial;std::string error;
    assert(!buildUploadPlan(base,preserved,error,[](uint64_t){return false;}));
    assert(preserved.localShadowRecords==initial.localShadowRecords);

    // Large world coordinates and 2-degree light use the exact same conservative
    // clip helper as drawing. No camera-dependent near-plane shortcut is used.
    WorldScene distant;distant.materials.resize(1);triangle(distant,{17000,-17000,1400});
    auto far=plan(distant);float lowLight[16];NorthlightWorldMath::shadowMatrix({17000,-17000,1400},{1,0,.03492077f},60,lowLight);
    assert(sig(far,lowLight).triangles==1);
    lowLight[3]=1;assert(sig(initial,lowLight).triangles==3); // Uncertain/perspective => no rejection.

    // Bounded metadata fallback retains usable geometry, rather than dropping
    // casters when a pathological asset has too many spatial records.
    WorldScene sparse;sparse.materials.resize(1);
    for(unsigned i=0;i<65537;++i)triangle(sparse,{float(i)*128,0,0});
    WorldMeshUploadPlan oversized;assert(buildUploadPlan(sparse,oversized,error));
    assert(!oversized.localShadowRecords&&oversized.triangleCount==65537);
    assert(S::needsRefresh(false,{},1,true,{},2)); // Unknown old geometry -> known empty must clear.
    assert(S::needsRefresh(true,{},1,false,{},2));
    assert(!S::needsRefresh(false,{},1,false,{},1));
    assert(!S::needsRefresh(true,nearOriginal,1,true,nearOriginal,2));
    assert(S::needsRefresh(true,nearOriginal,1,true,{},2));
    puts("PASS local shadow content: selective cascades, index/material reorder, geometry/UV/alpha/address edits, fixed terrain, upstream/far edges, 10000 warm memo hits, allocation failure, bounded fallback");
}
