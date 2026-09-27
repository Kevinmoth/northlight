#pragma once
// 0.3.159 jump-stable cascade anchor. The directional cascades are centred on the
// camera's orbit pivot, which follows a jump 1:1; every pivot-relative shader band (far edge
// fade, near/far cross-fade) then rides up and down with the character and mountain shadows
// 100-500 yd away brighten, darken or step. The anchor keeps the pivot's x/y and replaces its
// z with the minimum pivot z over the last Window milliseconds: a jump (up and back in under
// about .85 s) only adds an upward excursion and leaves the anchor bit-identical, walking
// downhill or falling follows at once, climbing lags by at most Window of climb (continuously).
// An instant rise (mounting, stepping onto a ledge) moves the bands once, Window later.
// The anchor never sits more than MaxLag below the pivot, so the player stays well inside the
// near map (48 u) whatever rises in under a second. A new map, a camera jump over
// ResetDistance or reset() starts a fresh window. Portable and allocation-free: the caller
// supplies the time; V needs x/y/z members and a V(x,y,z) constructor.
#include <cstdint>
namespace NorthlightCascadeAnchor {
constexpr uint32_t Window=1000; // ms
constexpr float ResetDistance=40; // camera move in one frame that counts as a teleport (yd)
constexpr float MaxLag=24; // yd; faster than any flying climb (~22 yd/s) over one Window
constexpr unsigned Capacity=256; // window entries; when full the oldest drops (a shorter window)
struct Anchor {
    struct Sample {uint32_t ms;float z;};
    Sample minima[Capacity]; /* ring, increasing z and time from the front: front is the window minimum */
    unsigned head=0,count=0;
    float cameraX=0,cameraY=0,cameraZ=0;bool valid=false;
    void reset(){count=0;valid=false;}
    const Sample& front()const{return minima[head];}
    const Sample& back()const{return minima[(head+count-1)%Capacity];}
    template<class V> V update(const V& pivot,const V& camera,uint32_t nowMs,bool newMap){
        const float dx=camera.x-cameraX,dy=camera.y-cameraY,dz=camera.z-cameraZ;
        if(!valid||newMap||dx*dx+dy*dy+dz*dz>ResetDistance*ResetDistance)count=0;
        cameraX=camera.x;cameraY=camera.y;cameraZ=camera.z;valid=true;
        while(count&&back().z>=pivot.z)--count;
        if(count==Capacity){head=(head+1)%Capacity;--count;}
        minima[(head+count)%Capacity]={nowMs,pivot.z};++count;
        while(uint32_t(nowMs-front().ms)>Window){head=(head+1)%Capacity;--count;} /* the newest sample always stays */
        const float lowest=pivot.z-MaxLag;
        return V(pivot.x,pivot.y,front().z>lowest?front().z:lowest);
    }
};
}
