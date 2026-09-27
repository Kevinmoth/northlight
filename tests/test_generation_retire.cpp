/* 0.3.138 generation retirement simulation. Real CpuRetirement, real
   RetirementBacklog::retireOrFree and real Generations<BVH,2>; a BVH whose
   destructor sleeps stands in for the ~200 MB generation free (measured
   0.3.137: up to 8.4 ms, p90 4.8 ms). For identical schedules the worker's
   generation deferrals are compared between 0.3.137 (synchronous free on the
   render thread) and 0.3.138 (reaper + bounded worker wait). Requirement:
   0.3.138 never defers where 0.3.137 did not, and never defers once the
   render thread has swapped the generation out. */
#include "cpu_retirement.h"
#include "geometry_memory.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <random>
#include <thread>
#include <vector>
using Clock=std::chrono::steady_clock;
struct BVH {unsigned freeMs;~BVH(){std::this_thread::sleep_for(std::chrono::milliseconds(freeMs));}};
struct Snapshot {std::shared_ptr<BVH> bvh;};
struct Result {unsigned deferrals=0,renderStallsOver2Ms=0,waits=0;double renderPeakMs=0,waitPeakMs=0;};
/* One generation switch: worker holds gen N+1 (published), render holds the
   snapshot of gen N and swaps it out at `swapAt`; the worker tests admission
   for gen N+2 at `checkAt` (both relative to start). */
static Result run(bool reaper,const std::vector<std::array<unsigned,3>>& schedule,NorthlightStreaming::CpuRetirement& queue){
    Result result;NorthlightStreaming::RetirementBacklog backlog;
    std::atomic<bool> noise{true};
    /* Background producers (static textures/models) contend for the reaper. */
    std::thread producer([&]{std::mt19937 rng(7);while(noise){auto small=std::make_shared<std::vector<char>>(1024);queue.retire(small,1024);std::this_thread::sleep_for(std::chrono::microseconds(200+rng()%800));}});
    for(const auto& item:schedule){
        const unsigned freeMs=item[0],swapAt=item[1],checkAt=item[2];
        NorthlightGeometryMemory::Generations<BVH> generations;
        auto current=std::make_shared<BVH>(BVH{0});assert(generations.track(current));
        auto old=std::make_shared<Snapshot>();old->bvh=std::make_shared<BVH>(BVH{freeMs});assert(generations.track(old->bvh));
        std::weak_ptr<BVH> watch=old->bvh;
        const auto start=Clock::now();
        std::thread render([&]{
            std::this_thread::sleep_until(start+std::chrono::milliseconds(swapAt));
            const auto t=Clock::now();
            std::shared_ptr<Snapshot> retired=std::move(old);
            auto bvh=std::move(retired->bvh);retired.reset(); /* snapshot owns the last BVH reference */
            if(reaper)backlog.retireOrFree(queue,bvh,size_t(200)<<20);else bvh.reset();
            const double ms=std::chrono::duration<double,std::milli>(Clock::now()-t).count();
            result.renderPeakMs=std::max(result.renderPeakMs,ms);result.renderStallsOver2Ms+=ms>2;
        });
        std::this_thread::sleep_until(start+std::chrono::milliseconds(checkAt));
        /* Production order (world_renderer.h work()): wait, then re-test. */
        if(!generations.canAdmit()&&reaper){const auto t=Clock::now();queue.waitIdle(NorthlightStreaming::RetireWaitMs);++result.waits;
            result.waitPeakMs=std::max(result.waitPeakMs,std::chrono::duration<double,std::milli>(Clock::now()-t).count());}
        if(!generations.canAdmit())++result.deferrals;
        render.join();
        while(!watch.expired())std::this_thread::yield();
        queue.waitIdle(1000);
    }
    noise=false;producer.join();return result;
}
int main(){
    std::mt19937 rng(138);
    std::vector<std::array<unsigned,3>> late,any;
    /* late: the worker's next admission comes after the render swap (the
       normal case: publish -> swap within a frame -> 32 units of travel). */
    for(unsigned i=0;i<60;++i){unsigned freeMs=1+rng()%12,swap=rng()%5;late.push_back({freeMs,swap,swap+3+rng()%(freeMs+2)});}
    /* any: adversarial, including admission before the swap (flying mount). */
    for(unsigned i=0;i<60;++i){unsigned freeMs=1+rng()%12;any.push_back({freeMs,rng()%10,rng()%14});}
    NorthlightStreaming::CpuRetirement queue(true);
    const Result oldLate=run(false,late,queue),newLate=run(true,late,queue);
    const Result oldAny=run(false,any,queue),newAny=run(true,any,queue);
    std::printf("late: 0.3.137 deferrals=%u renderPeak=%.2fms stalls>2ms=%u | 0.3.138 deferrals=%u renderPeak=%.2fms stalls>2ms=%u waits=%u waitPeak=%.2fms\n",
        oldLate.deferrals,oldLate.renderPeakMs,oldLate.renderStallsOver2Ms,newLate.deferrals,newLate.renderPeakMs,newLate.renderStallsOver2Ms,newLate.waits,newLate.waitPeakMs);
    std::printf("adversarial: 0.3.137 deferrals=%u | 0.3.138 deferrals=%u waits=%u waitPeak=%.2fms\n",oldAny.deferrals,newAny.deferrals,newAny.waits,newAny.waitPeakMs);
    /* Admission after the swap never defers with the reaper wait. */
    assert(newLate.deferrals==0);
    /* Never worse than 0.3.137 on the same schedule. Timing slack: an
       admission within ~1 ms of the swap may race either way in both runs. */
    assert(newAny.deferrals<=oldAny.deferrals+2);
    assert(newLate.waitPeakMs<NorthlightStreaming::RetireWaitMs);
    /* Render thread no longer pays the free (reaper accepted it). */
    assert(newLate.renderPeakMs<oldLate.renderPeakMs);
    std::puts("generation retirement simulation passed");
}
