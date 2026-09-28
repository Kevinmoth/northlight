#pragma once
// 0.3.172 near capture reserve. The capture budget (CaptureBudgetMiB) runs out in the game's
// draw order: facing a crowd, the player's own mount and rider can come after it is spent and
// lose their shadow. A skinned draw about to be turned away for bytes is tested here; a near
// one may read from a separate reserve (draw_snapshot.h Frame::setNearReserve) after the main
// budget is gone. Near = its palette root within SelfRadius of the last selected self (only
// while the self is established: ActorShadowRadius > 0), or within RayMiss of the view ray at
// a distance of pivotDistance +- RayWindow (clamped to RayMin..RayMax): the player stands on
// that ray at every radius, including 0, and a stale pivot distance after a zoom stays inside
// the window. An unknown or non-finite root is never near. Portable: no D3D calls.
#include <cmath>
#include <cstddef>
namespace NorthlightNearReserve {
constexpr std::size_t ReserveBytes=4u<<20;
constexpr float SelfRadius=6.f,RayMiss=3.5f,RayWindow=12.f,RayMin=.5f,RayMax=80.f;
struct Anchor {bool self=false,ray=false;float selfAt[3]={},eye[3]={},forward[3]={},low=0,high=0;};
// selfAt: the established self (null: none); eye and forward (any length): the main camera.
inline Anchor anchor(const float* selfAt,const float* eye,const float* forward,float pivotDistance){
    Anchor a;auto finite=[](const float* v){return v&&std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]);};
    if(finite(selfAt)){a.self=true;for(unsigned k=0;k<3;++k)a.selfAt[k]=selfAt[k];}
    if(finite(eye)&&finite(forward)&&std::isfinite(pivotDistance)){
        const float length=std::sqrt(forward[0]*forward[0]+forward[1]*forward[1]+forward[2]*forward[2]);
        if(length>1e-6f){for(unsigned k=0;k<3;++k){a.eye[k]=eye[k];a.forward[k]=forward[k]/length;}
            a.low=std::fmax(RayMin,pivotDistance-RayWindow);a.high=std::fmin(RayMax,pivotDistance+RayWindow);a.ray=a.low<=a.high;}}
    return a;
}
enum Hit : unsigned {None=0,Self=1,Ray=2};
// Which anchors hold `root` (0: not near).
inline unsigned test(const Anchor& a,const float* root){
    if(!root||!std::isfinite(root[0])||!std::isfinite(root[1])||!std::isfinite(root[2]))return None;
    unsigned hit=None;
    if(a.self){float q=0;for(unsigned k=0;k<3;++k){const float d=root[k]-a.selfAt[k];q+=d*d;}if(q<=SelfRadius*SelfRadius)hit|=Self;}
    if(a.ray){float v[3],t=0,q=0;for(unsigned k=0;k<3;++k){v[k]=root[k]-a.eye[k];t+=v[k]*a.forward[k];q+=v[k]*v[k];}
        if(t>=a.low&&t<=a.high&&q-t*t<=RayMiss*RayMiss)hit|=Ray;}
    return hit;
}
}
