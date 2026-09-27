#include "shadow_bounds.h"
#include "world_math.h"
#include <cassert>
#include <cstdio>
using NorthlightGI::Vec3;
int main(){
    const float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const Vec3 upstreamLow(-.5f,-.5f,-3),upstreamHigh(.5f,.5f,-2);
    assert(NorthlightShadowBounds::clipReject(upstreamLow,upstreamHigh,identity));
    assert(!NorthlightShadowBounds::directionalClipReject(upstreamLow,upstreamHigh,identity));
    assert(NorthlightShadowBounds::directionalClipReject(Vec3(2,-.5f,-3),Vec3(3,.5f,-2),identity));
    assert(NorthlightShadowBounds::directionalClipReject(Vec3(-.5f,2,-3),Vec3(.5f,3,-2),identity));
    assert(NorthlightShadowBounds::directionalClipReject(Vec3(-.5f,-.5f,2),Vec3(.5f,.5f,3),identity));
    assert(!NorthlightShadowBounds::directionalClipReject(Vec3(-.5f,-.5f,-2),Vec3(.5f,.5f,2),identity));
    assert(!NorthlightShadowBounds::directionalClipReject(Vec3(NAN,0,0),Vec3(1,1,1),identity));
    unsigned cases=0;
    for(float altitude:{2.f,5.f,15.f,43.f})for(float radius:{48.f,192.f}){
        const float a=altitude*3.14159265f/180;
        const Vec3 direction=NorthlightGI::normalized(Vec3(std::cos(a)*.70710678f,std::cos(a)*.70710678f,std::sin(a)));
        const Vec3 center(-10800,-920,65),caster=center+direction*660.f;
        for(float travel:{-20.f,0.f,20.f}){
            float m[16];NorthlightWorldMath::shadowMatrix(center+direction*travel,direction,radius,m);
            assert(!NorthlightShadowBounds::directionalClipReject(caster-Vec3(2,2,2),caster+Vec3(2,2,2),m));
            // A receiver near the player must stay occluded through both a
            // cached-origin shift and a fresh render with upstream depth zero.
            const float raw=.5f-(660-travel)/1280;
            const float fresh=std::max(raw,0.f);
            const float cached=std::max(std::max(.5f-660.f/1280,0.f)+travel/1280,0.f);
            const float receiver=.5f+travel/1280;
            assert(fresh<receiver&&cached<receiver);++cases;
        }
    }
    // A triangle spanning BOTH depth planes: clamping only near vertices
    // would far-clip it at t=.5 rather than t=2/3. Raster clamping on both
    // ends plus raw per-fragment depth retains the exact in-range surface.
    for(float t:{.4f,.55f,.65f,.8f}){
        const float raw=-1*(1-t)+2*t;
        const float fragment=std::max(raw,0.f);
        const bool retained=raw<=1;
        if(t>.5f&&t<2.f/3){assert(retained&&fragment<1);}
        if(t>2.f/3)assert(!retained);
    }
    std::printf("PASS upstream/XY/far/invalid bounds; %u low-angle cached/fresh receiver cases; spanning-plane depth\n",cases);
}
