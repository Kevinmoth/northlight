// 0.3.172 near capture reserve (near_reserve.h): the self sphere, the view-ray capsule with a stale
// pivot distance, the ray window limits, unknown and non-finite roots, no self (capsule only).
#include "near_reserve.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
using namespace NorthlightNearReserve;
int main(){
    const float eye[3]={-8886.2f,572.2f,100.4f},raw[3]={.79f*2,.51f*2,-.33f*2}; /* the 13:34:35 behind view; forward of any length */
    const float n=std::sqrt(raw[0]*raw[0]+raw[1]*raw[1]+raw[2]*raw[2]),f[3]={raw[0]/n,raw[1]/n,raw[2]/n};
    auto along=[&](float t,float side,float* out){const float s[3]={f[1],-f[0],0};const float l=std::sqrt(s[0]*s[0]+s[1]*s[1]);for(unsigned k=0;k<3;++k)out[k]=eye[k]+f[k]*t+s[k]/l*side;};
    // Self sphere: 6 yd around the last self, independent of the ray.
    {const float self[3]={-8870,579,96};const Anchor a=anchor(self,eye,raw,15);assert(a.self&&a.ray);
     const float in[3]={self[0]+5.9f,self[1],self[2]},out[3]={self[0],self[1]+6.1f,self[2]};
     assert(test(a,in)&Self);assert(!(test(a,out)&Self));
     const Anchor none=anchor(nullptr,eye,raw,15);assert(!none.self&&test(none,in)==(test(a,in)&Ray));}
    // Capsule: pivot distance 20, a stale estimate by +-10 yd still finds the player at t=20; +-14 does not.
    {float p[3];along(20,0,p);
     for(float stale:{-10.f,10.f,0.f})assert(test(anchor(nullptr,eye,raw,20+stale),p)==Ray);
     for(float stale:{-14.f,14.f})assert(test(anchor(nullptr,eye,raw,20+stale),p)==None);
     const Anchor a=anchor(nullptr,eye,raw,20);float q[3];
     along(20,3.4f,q);assert(test(a,q)==Ray);along(20,3.6f,q);assert(test(a,q)==None); /* 3.5 yd miss */
     along(8.1f,0,q);assert(test(a,q)==Ray);along(7.9f,0,q);assert(test(a,q)==None); /* window 20-12 */
     along(31.9f,0,q);assert(test(a,q)==Ray);along(32.1f,0,q);assert(test(a,q)==None);}
    // Ray limits: t never below 0.5 (the camera itself) nor above 80.
    {const Anchor close=anchor(nullptr,eye,raw,3);assert(close.low==RayMin&&close.high==15);float q[3];
     along(.4f,0,q);assert(test(close,q)==None);along(.6f,0,q);assert(test(close,q)==Ray);
     const Anchor far=anchor(nullptr,eye,raw,75);assert(far.low==63&&far.high==RayMax);
     along(79.9f,0,q);assert(test(far,q)==Ray);along(80.1f,0,q);assert(test(far,q)==None);
     const float behind[3]={eye[0]-f[0]*5,eye[1]-f[1]*5,eye[2]-f[2]*5};assert(test(close,behind)==None);}
    // Unknown and non-finite roots are never near; a degenerate anchor has no ray.
    {const float self[3]={0,0,0};const Anchor a=anchor(self,eye,raw,20);
     const float nan[3]={NAN,0,0},inf[3]={0,INFINITY,0};assert(test(a,nullptr)==None&&test(a,nan)==None&&test(a,inf)==None);
     const float zero[3]={0,0,0},bad[3]={NAN,0,0};assert(!anchor(nullptr,eye,zero,20).ray&&!anchor(nullptr,bad,raw,20).ray&&!anchor(nullptr,eye,raw,NAN).ray);
     assert(!anchor(bad,eye,raw,20).self);}
    // Both anchors at once.
    {float p[3];along(20,0,p);const Anchor a=anchor(p,eye,raw,20);assert(test(a,p)==(Self|Ray));}
    std::puts("PASS near reserve: self sphere 6 yd, view-ray capsule 3.5 yd over pivotDistance +-12 (stale +-10 inside, +-14 outside), t in [0.5, 80], unknown/non-finite roots never near, no self = capsule only");
}
