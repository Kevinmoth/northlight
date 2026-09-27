#pragma once
#include "world_math.h"
#include <cmath>
#include <cstdint>
#include <cstring>
// 0.3.137: a lattice step of a light direction re-renders every active static
// shadow slot in one frame. In a frame that re-rendered nothing, prepare the
// static plan of the next lattice direction (linear extrapolation of the raw
// direction, same pivot and frame math) through the ordinary plan cache. The
// step frame finds a valid plan only for its exact matrix, kept current by the
// same invalidation as every cached plan: what a synchronous build returns.
// Wasted work is capped: pivot bit-stable for StableFrames, one build per
// IntervalMs, each (source,cascade,target) built at most once, and a prebuilt
// plan that its step (or a newer target) did not use is discarded.
namespace NorthlightStaticPrebuild {
using NorthlightGI::Vec3;
// Next lattice direction of a raw direction moving at `velocity` per ms: the
// first component to cross its rounding boundary within leadMs steps by one.
// Bit-identical to quantizeDirection(v,steps) of any raw direction rounding to that
// lattice point: round(k/steps*steps)==k for integer |k|<=steps<=2048 (float error
// far below .5), so both produce the same round(.)/steps. steps=2048 is 0.3.137-static.
inline bool nextQuantizedDirection(Vec3 raw,Vec3 velocity,float leadMs,Vec3& out,float steps=2048){
    const float r[3]={raw.x,raw.y,raw.z},v[3]={velocity.x,velocity.y,velocity.z};float k[3];int axis=-1;float soonest=leadMs;
    for(int i=0;i<3;++i){k[i]=std::round(r[i]*steps);if(!(std::fabs(v[i])>0))continue;const float t=((k[i]+(v[i]>0?.5f:-.5f))/steps-r[i])/v[i];if(t>=0&&t<soonest){soonest=t;axis=i;}}
    if(axis<0)return false;k[axis]+=v[axis]>0?1.f:-1.f;
    out=NorthlightWorldMath::quantizeDirection(Vec3(k[0]/steps,k[1]/steps,k[2]/steps),steps);return true;
}
struct Scheduler {
    static constexpr bool Enabled=false; /* 0.3.138: in-game hit rate 0.29-0.40 < 0.5 */
    static constexpr float LeadMs=4000;static constexpr uint32_t IntervalMs=500,SampleMs=1000,StaleMs=5000;static constexpr unsigned StableFrames=30;
    struct Stats {unsigned built=0,hits=0,misses=0,failures=0,unstable=0,discarded=0;};
    Stats stats;
    float steps=2048; // ShadowDirectionSteps: the lattice of the renderer's quantizeDirection
    // Raw (unquantized) direction per frame; velocity from >=1 s samples.
    void observe(int source,bool active,Vec3 raw,uint32_t nowMs){
        auto& s=sources[source];s.raw=raw;
        if(!active){s=Source{};return;}
        if(!s.sampled){s.sampled=true;s.sampleMs=nowMs;s.sampleRaw=raw;return;}
        const uint32_t dt=nowMs-s.sampleMs;if(dt<SampleMs)return;
        s.velocity=(raw-s.sampleRaw)*(1.f/float(dt));s.velocityValid=dt<StaleMs;s.sampleMs=nowMs;s.sampleRaw=raw;
    }
    // Every slot re-render with its cached matrix. A matching prebuilt plan is
    // now the slot's own (forgotten here); a direction step that missed it
    // passed its target, so the unused plan is discarded.
    template<class Discard> void rerender(int source,int cascade,const float* matrix,bool direction,bool planReady,Discard discard){
        if(direction){if(planReady)++stats.hits;else ++stats.misses;}
        auto& s=sources[source];if(!s.owned[cascade])return;
        if(!std::memcmp(s.done[cascade],matrix,sizeof s.done[cascade])){s.owned[cascade]=false;return;}
        if(direction){discard(s.done[cascade]);s.owned[cascade]=false;++stats.discarded;}
    }
    // End of every shadow phase. matrix(source,cascade,direction,out) must be
    // the phase's own cached-matrix math; ready(m) no build; build(m) true=ok.
    template<class Matrix,class Ready,class Build,class Discard>
    void frame(Vec3 pivot,const Vec3* quantized,const bool* active,bool quiet,uint32_t nowMs,Matrix matrix,Ready ready,Build build,Discard discard){
        if(std::memcmp(&pivot,&lastPivot,sizeof pivot)==0){if(stableFrames<StableFrames)++stableFrames;}else{lastPivot=pivot;stableFrames=0;}
        if(!quiet||(lastBuildMs&&nowMs-lastBuildMs<IntervalMs))return;
        for(int source=0;source<2;++source){auto& s=sources[source];if(!active[source]||!s.velocityValid)continue;
            Vec3 next;if(!nextQuantizedDirection(s.raw,s.velocity,LeadMs,next,steps))continue;
            if(NorthlightGI::dot(next-quantized[source],next-quantized[source])<=1e-12f)continue;
            if(stableFrames<StableFrames){++stats.unstable;return;}
            for(int cascade=0;cascade<2;++cascade){float m[16];matrix(source,cascade,next,m);
                auto& done=s.done[cascade];if(!std::memcmp(done,m,sizeof m))continue;
                if(ready(m)){if(s.owned[cascade]){discard(done);++stats.discarded;s.owned[cascade]=false;}std::memcpy(done,m,sizeof m);continue;}
                if(s.owned[cascade]){discard(done);++stats.discarded;}
                std::memcpy(done,m,sizeof m);lastBuildMs=nowMs?nowMs:1;
                if(build(m)){++stats.built;s.owned[cascade]=true;}else{++stats.failures;s.owned[cascade]=false;}
                return;
            }
        }
    }
private:
    struct Source {Vec3 raw,sampleRaw,velocity;uint32_t sampleMs=0;bool sampled=false,velocityValid=false,owned[2]={};float done[2][16]={};};
    Source sources[2];Vec3 lastPivot{NAN,NAN,NAN};unsigned stableFrames=0;uint32_t lastBuildMs=0;
};
} // namespace NorthlightStaticPrebuild
