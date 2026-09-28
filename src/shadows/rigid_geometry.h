#pragma once
// Rigid-model geometry shared by the persistent casters (persistent_casters.h) and the
// rigid memory (rigid_memory.h), moved from persistent_casters.h in 0.3.172: the client's
// one-influence palette program, a palette bone's world matrix, the static-cache placement
// match and its index, and the centre-in-view test. Portable (no D3D calls).
#include "actor_deformation.h"
#include "world_gi.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>
namespace NorthlightRigidGeometry {
using Vec3=NorthlightGI::Vec3;
// The client's one-influence palette program (vs_2_0/vs_3_0 Diffuse* variants
// with BLENDINDICES and no BLENDWEIGHT): t.x = c0.x(3) * v1.x; a0.x = t.x;
// r0.xyz = dp4(c31..c33[a0.x], v0); r0.w = c0.y(1). Exact template: both mul
// operand orders, t = r0 or r1, the w move anywhere. Reads BLENDINDICES lane x
// only, weight 1 (referencedSlots() with lanes 1).
inline bool oneBoneTemplate(const NorthlightActorDeformation::Program& p){
    if((p.major!=2&&p.major!=3)||p.positionRegister!=0||p.textureCoordinates||p.operations.size()!=6||p.inputs.size()!=2||p.paletteBase!=31)return false;
    bool position=false,index=false;for(const auto& i:p.inputs){if(i.index)return false;position=position||(i.reg==0&&i.usage==0);index=index||(i.reg==1&&i.usage==2);}if(!position||!index)return false;
    bool def=false;for(const auto& d:p.definitions)if(d.reg==0){if(d.value[0]!=3||d.value[1]!=1)return false;def=true;}if(!def)return false;
    unsigned at=0,moves=0,t=0;
    for(const auto& o:p.operations){const auto& a=o.source[0];const auto& b=o.source[1];
        if(o.code==1&&o.destination==0x80080000u&&a.token==0xa0550000u&&!a.address&&!b.token&&!o.source[2].token){++moves;continue;} /* r0.w = c0.y */
        if(b.address||o.source[2].token||o.source[2].address)return false;
        switch(at++){
        case 0:t=o.destination&2047u;if(o.code!=5||(o.destination!=0x80010000u&&o.destination!=0x80010001u)||a.address||!((a.token==0xa0000000u&&b.token==0x90000001u)||(a.token==0x90000001u&&b.token==0xa0000000u)))return false;break;
        case 1:if(o.code!=46||o.destination!=0xb0010000u||a.token!=(0x80000000u|t)||a.address||b.token)return false;break;
        default:{const unsigned row=at-3;if(o.code!=9||o.destination!=(0x80000000u|(1u<<(16+row)))||a.token!=0xa0e4201fu+row||a.address!=0xb0000000u||b.token!=0x90e40000u)return false;}}}
    return at==5&&moves==1;
}
// World matrix of one palette bone: rows = its three view-space rows (c31+3b..,
// translation in w), inverseView = the capture's view->world. Output: the world
// images of the three model axes (9), then of the model origin (3). Camera
// motion changes the rows but not this result.
inline bool worldBone(const float* rows,const float* inverseView,float* out){
    for(unsigned a=0;a<3;++a)for(unsigned w=0;w<3;++w)out[a*3+w]=rows[a]*inverseView[w]+rows[4+a]*inverseView[4+w]+rows[8+a]*inverseView[8+w];
    for(unsigned w=0;w<3;++w)out[9+w]=rows[3]*inverseView[w]+rows[7]*inverseView[4+w]+rows[11]*inverseView[8+w]+inverseView[12+w];
    for(unsigned k=0;k<12;++k)if(!std::isfinite(out[k]))return false;
    return true;
}
// Rigid prop drawn by the static cache: a placement whose origin is the root and
// whose model axes (row-major matrix columns) are the root's world axes. A
// rigid M2 at rest has bone matrix = placement, so a real doodad matches to
// float precision; a server-spawned object at another spot never does.
inline bool staticPlacement(const float* root,const float* axes,const float* translation,const float* matrix,float tolerance=.05f,float axisTolerance=.01f){
    float d=0,scale=1;for(unsigned k=0;k<3;++k){const float t=root[k]-translation[k];d+=t*t;}if(!(d<=tolerance*tolerance))return false;
    for(unsigned k=0;k<9;++k)scale=std::max(scale,std::fabs(matrix[k]));
    for(unsigned a=0;a<3;++a)for(unsigned w=0;w<3;++w)if(!(std::fabs(axes[a*3+w]-matrix[w*3+a])<=axisTolerance*scale))return false;
    return true;
}
// Static-cache placement origins bucketed in 16 yd cells, built a bounded number
// of placements per frame for one scene identity (a large city scene never
// stalls a frame); find() visits the placements within 3x3 cells of a root.
class PlacementIndex {
    struct Item {float x=0,y=0,z=0;std::uint32_t index=0;};
    std::unordered_map<std::uint64_t,std::vector<Item>> cells_;std::size_t items_=0;
    static std::uint64_t key(long x,long y){return (std::uint64_t(std::uint32_t(x))<<32)|std::uint32_t(y);}
    static long cell(float v){return long(std::floor(v/16.f));}
public:
    const void* scene=nullptr;std::uint64_t revision=0;std::size_t next=0;bool complete=false;
    void reset(const void* s,std::uint64_t r){cells_.clear();items_=0;scene=s;revision=r;next=0;complete=false;}
    void add(float x,float y,float z,std::uint32_t index){if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return;cells_[key(cell(x),cell(y))].push_back({x,y,z,index});++items_;}
    std::size_t size()const{return items_;}
    template<class Match> bool find(const float* root,float tolerance,Match match)const{
        if(!std::isfinite(root[0])||!std::isfinite(root[1]))return false;const long cx=cell(root[0]),cy=cell(root[1]);
        for(long dx=-1;dx<=1;++dx)for(long dy=-1;dy<=1;++dy){auto it=cells_.find(key(cx+dx,cy+dy));if(it==cells_.end())continue;
            for(const auto& i:it->second)if(std::fabs(i.x-root[0])<=tolerance&&std::fabs(i.y-root[1])<=tolerance&&std::fabs(i.z-root[2])<=tolerance&&match(i.index))return true;}
        return false;
    }
};
// In-view test: the box centre inside the view frustum (the whole screen) and
// within `range` (a point: low == high).
inline bool centreInView(const float* inverseView,const float* projection,Vec3 low,Vec3 high,float range){
    if(!inverseView||!projection)return false;
    for(unsigned i=0;i<16;++i)if(!std::isfinite(inverseView[i]))return false;
    for(unsigned i=0;i<3;++i)if(!std::isfinite(projection[i])||std::fabs(projection[i])<1e-4f)return false;
    const float* eye=inverseView+12;const float c[3]={(low.x+high.x)*.5f-eye[0],(low.y+high.y)*.5f-eye[1],(low.z+high.z)*.5f-eye[2]};
    if(!(c[0]*c[0]+c[1]*c[1]+c[2]*c[2]<=range*range))return false;
    const float x=c[0]*inverseView[0]+c[1]*inverseView[1]+c[2]*inverseView[2],y=c[0]*inverseView[4]+c[1]*inverseView[5]+c[2]*inverseView[6];
    const float f=(c[0]*inverseView[8]+c[1]*inverseView[9]+c[2]*inverseView[10])*(projection[2]<0?-1.f:1.f);
    return f>.1f&&std::fabs(x*projection[0])<=f&&std::fabs(y*projection[1])<=f;
}
}
