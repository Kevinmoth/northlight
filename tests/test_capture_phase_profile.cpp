#include <cassert>
#include <cstdint>
#include <cstdio>
struct LARGE_INTEGER {std::int64_t QuadPart=0;};
bool QueryPerformanceCounter(LARGE_INTEGER*){assert(false&&"test must use its explicit clock");return false;}
#include "capture_phase_profile.h"
struct Clock {
    static std::int64_t tick;static unsigned reads;
    static std::int64_t read(){++reads;return tick;}
};
std::int64_t Clock::tick=0;unsigned Clock::reads=0;
using Stats=NorthlightCapturePhases::Stats<3>;
using Scope=NorthlightCapturePhases::Scope<3,Clock>;
int main(){
    // Unsampled frames never ask for time, even across all phase boundaries.
    {Scope p(nullptr);for(unsigned i=0;i<10000;++i)p.next(i%3);p.stop();}assert(Clock::reads==0);
    Stats s;
    {Scope p(&s);Clock::tick=7;p.next(1);Clock::tick=12;p.next(2);Clock::tick=21;p.stop();Clock::tick=500;p.stop();}
    assert((s.ticks==std::array<std::int64_t,3>{7,5,9}));assert(s.clockReads==4&&Clock::reads==4);
    s.clear();assert(s.clockReads==0);for(auto t:s.ticks)assert(t==0);
    // Early rejection still charges exactly the active phase via RAII.
    {Scope p(&s,1);Clock::tick+=11;}
    assert(s.ticks[0]==0&&s.ticks[1]==11&&s.ticks[2]==0&&s.clockReads==2);
    s.clear();
    // A retry ends its parent interval first; child time is counted once.
    {Scope parent(&s);Clock::tick+=2;parent.next(1);Clock::tick+=3;parent.stop();
     {Scope retry(&s);Clock::tick+=5;retry.next(1);Clock::tick+=7;}}
    assert(s.ticks[0]==7&&s.ticks[1]==10&&s.ticks[2]==0&&s.clockReads==6);
    std::puts("PASS sampled phase timing: disabled zero-clock path, boundaries, early returns, reset and nonoverlapping retry");
}
