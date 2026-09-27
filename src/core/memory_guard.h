#pragma once
#include <cstdint>

// Low address-space guard for the 32-bit (Large Address Aware) client. The
// async sampler thread measures; the render thread only compares numbers and,
// at the frame boundary, drops mod caches that are rebuilt on demand. The mod
// owns a few hundred MiB of a 4 GiB space: this delays, it cannot prevent,
// exhaustion caused by the game or driver. Admission checks stay synchronous.
namespace NorthlightMemoryGuard {
constexpr std::uint64_t MiB=1024ull*1024ull;
struct Policy {
    // Pressure: largest free region or aggregate free VA below these.
    std::uint64_t enterLargest=160*MiB,enterAvailable=400*MiB;
    // Recovery: both above these for recoverHoldMs of consecutive samples.
    std::uint64_t recoverLargest=256*MiB,recoverAvailable=640*MiB;
    std::uint32_t recoverHoldMs=10000,trimCooldownMs=20000;
    // Address-space walk cadence (sampler thread, ~6-50 ms each on Windows).
    std::uint32_t sampleMs=3000,pressureSampleMs=1000,reportMs=10000;
};
struct Decision {
    bool trim=false,entered=false,recovered=false;
    bool afterTrim=false; // first sample walked after the last completed trim
    bool report=false;    // periodic diagnostic line due (caller gates on Diagnostics)
};
// Ticks are GetTickCount() milliseconds; every comparison is wrap-safe. Feed
// only exact full address-space walks, never admission-witness lower bounds.
class Guard {
    Policy policy_;
    bool pressure_=false,trimmed_=false,healthy_=false,requested_=false,reported_=false,afterPending_=false;
    std::uint32_t lastTrim_=0,trimDone_=0,healthySince_=0,lastRequest_=0,lastReport_=0;
    std::uint64_t minAvailable_=~0ull,minLargest_=~0ull;
    unsigned trims_=0,entries_=0,samples_=0;
    static bool after(std::uint32_t tick,std::uint32_t reference){return std::int32_t(tick-reference)>0;}
public:
    explicit Guard(Policy policy=Policy{}):policy_(policy){}
    const Policy& policy()const{return policy_;}
    // True when a new address-space walk should be requested now.
    bool requestDue(std::uint32_t now){
        if(requested_&&now-lastRequest_<(pressure_?policy_.pressureSampleMs:policy_.sampleMs))return false;
        requested_=true;lastRequest_=now;return true;
    }
    // One valid sample; sampleTick is when the walk finished.
    Decision update(std::uint64_t available,std::uint64_t largest,std::uint32_t sampleTick){
        Decision d;++samples_;
        if(available<minAvailable_)minAvailable_=available;
        if(largest<minLargest_)minLargest_=largest;
        if(afterPending_&&after(sampleTick,trimDone_)){afterPending_=false;d.afterTrim=true;}
        const bool low=largest<policy_.enterLargest||available<policy_.enterAvailable;
        const bool healthy=largest>=policy_.recoverLargest&&available>=policy_.recoverAvailable;
        if(low){
            healthy_=false;
            if(!pressure_){pressure_=true;++entries_;d.entered=true;}
            // Repeated trims while still low, at most once per cooldown.
            if(!trimmed_||sampleTick-lastTrim_>=policy_.trimCooldownMs){trimmed_=true;lastTrim_=sampleTick;++trims_;d.trim=true;}
        }else if(pressure_&&healthy){
            if(!healthy_){healthy_=true;healthySince_=sampleTick;}
            if(sampleTick-healthySince_>=policy_.recoverHoldMs){pressure_=false;healthy_=false;d.recovered=true;}
        }else healthy_=false; // between the thresholds: keep the current state
        if(!reported_||sampleTick-lastReport_>=policy_.reportMs||d.trim||d.entered||d.recovered){reported_=true;lastReport_=sampleTick;d.report=true;}
        return d;
    }
    // The caller finished the trim a decision asked for (frame tick).
    void trimCompleted(std::uint32_t now){trimDone_=now;afterPending_=true;}
    bool pressure()const{return pressure_;}
    unsigned trims()const{return trims_;}
    unsigned entries()const{return entries_;}
    unsigned samples()const{return samples_;}
    std::uint64_t minAvailable()const{return samples_?minAvailable_:0;}
    std::uint64_t minLargest()const{return samples_?minLargest_:0;}
};
}
