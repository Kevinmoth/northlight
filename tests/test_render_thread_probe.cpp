// 0.3.149 render-thread instrumentation (RenderProfile): native tests of the
// production render_thread_probe.h (profiler sampler vs near/far interval phases,
// percentiles, near/far frame cost window with exclusions, replay loop policies,
// probe schedule) and the RenderProfile/DiagReplayProbe quality keys. Fake clocks
// only; no Win32, D3D, Wine or game.
#include "render_thread_probe.h"
#include "quality_settings.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <random>
#include <sstream>
#include <string>
#include <vector>
namespace P=NorthlightRenderThreadProbe;

struct FakeClock {static inline int64_t t=0,step=0;static inline unsigned reads=0;static int64_t now(){++reads;t+=step;return t;}};
struct SlowReadClock {static inline int64_t t=0;static inline unsigned reads=0;static int64_t now(){++reads;t+=reads==10?700:7;return t;}}; /* read 10: 100x slower */
struct Packet {int id;};

static void sampling(){
    // RenderProfile=0: exactly the 0.3.148 frames (frame%120==0).
    for(unsigned f=0;f<120*1000;++f)assert(P::sampleFrame(f,false)==(f%120==0));
    // RenderProfile=1: one frame in 127, never on the mirror audit frame (frame%120==60) more than by coincidence.
    unsigned n=0;for(unsigned f=0;f<127*1000;++f)n+=P::sampleFrame(f,true);assert(n==1000);
    // Near/FarShadowInterval 1..16 in lock-step with the frame counter: the profiler reaches every
    // reachable (near phase, far phase) pair; frame%120==0 misses them (e.g. Near=2: odd phase never).
    unsigned oldMissing=0;
    for(unsigned nearI=1;nearI<=16;++nearI)for(unsigned farI=1;farI<=16;++farI){
        unsigned l=nearI;while(l%farI)l+=nearI; /* lcm */
        std::vector<char> reachable(nearI*farI),seen(nearI*farI),seenOld(nearI*farI);
        for(unsigned f=0;f<l;++f)reachable[(f%nearI)*farI+f%farI]=1;
        for(unsigned f=0;f<127*l;++f)if(P::sampleFrame(f,true))seen[(f%nearI)*farI+f%farI]=1;
        for(unsigned f=0;f<120*l;++f)if(P::sampleFrame(f,false))seenOld[(f%nearI)*farI+f%farI]=1;
        assert(seen==reachable);oldMissing+=seenOld!=reachable;
    }
    assert(oldMissing>0);
    // NearShadowInterval=2 lock-step: old rule 400/0 by parity, new rule balanced.
    unsigned oldEven=0,oldOdd=0,even=0,odd=0;
    for(unsigned f=0;f<120*127*4;++f){if(P::sampleFrame(f,false))(f&1?oldOdd:oldEven)++;if(P::sampleFrame(f,true))(f&1?odd:even)++;}
    assert(oldOdd==0&&oldEven>0&&even==odd);
    std::printf("PASS sampling: RenderProfile=0 is frame%%120==0; RenderProfile=1 samples 1/127 and reaches every near/far phase pair for intervals 1..16 (frame%%120 misses %u of 256 interval pairs); parity %u/%u\n",oldMissing,even,odd);
}

static double nearestRank(std::vector<double> v,double q){std::sort(v.begin(),v.end());const size_t r=size_t(std::ceil(q*double(v.size())-1e-9));return v[r?r-1:0];}
static void percentiles(){
    std::mt19937 rng(149);
    for(unsigned n:{1u,2u,3u,7u,100u,119u,120u,121u,240u,1000u}){
        std::vector<double> v(n);for(auto& x:v)x=std::uniform_real_distribution<double>(0,50)(rng);
        std::vector<double> w=v;const auto s=P::summarize(w.data(),n);
        double sum=0;for(double x:v)sum+=x;
        assert(s.count==n&&std::fabs(s.mean-sum/n)<1e-9);
        assert(s.p50==nearestRank(v,.5)&&s.p95==nearestRank(v,.95)&&s.p99==nearestRank(v,.99)&&s.max==*std::max_element(v.begin(),v.end()));
    }
    // n=120: the same p95 index as NorthlightFrameIntervals (113).
    std::vector<double> v(120);for(unsigned i=0;i<120;++i)v[i]=double(119-i);
    const auto s=P::summarize(v.data(),120);assert(s.p95==113&&s.p50==59&&s.p99==118&&s.max==119&&s.mean==59.5);
    assert(P::summarize(static_cast<double*>(nullptr),0).count==0);
    P::Samples<4> window;window.add(4);window.add(1);window.add(3);window.add(2);window.add(9); /* the fifth is dropped */
    assert(window.count()==4);const auto out=window.take();
    assert(out.count==4&&out.p50==2&&out.max==4&&out.mean==2.5&&window.count()==0);
    std::puts("PASS percentiles: nearest rank matches a brute-force reference for n=1..1000; p95 index 113 at n=120; fixed sample store drops overflow and restarts");
}

static void frameCost(){
    P::FrameCostWindow w;const int64_t f=1000000; /* ticks per second: 1 tick = 1 us */
    int64_t t=5000000;unsigned long long logs=0;
    auto frame=[&](double effectsMs,P::Kind kind,int64_t finishUs,int64_t presentUs,int64_t intervalUs,bool excluded=false,bool logged=false){
        w.close(effectsMs,kind,excluded);t+=intervalUs;if(logged)++logs;const int64_t entry=t;w.presented(entry,entry+finishUs,entry+finishUs+presentUs,f,logs);};
    frame(1,P::NearFar,100,200,16000);frame(1,P::NearFar,100,200,16000); /* priming, then the dirty first frame */
    assert(w.count()==0);
    for(unsigned i=0;i<240;++i){const bool skip=i&1;frame(skip?.5:2.0,skip?P::None:P::NearOnly,skip?50:300,skip?1000:4000,skip?10000:20000);}
    assert(w.count()==240);
    auto r=w.take();
    const auto& nearOnly=r.kinds[P::NearOnly];const auto& none=r.kinds[P::None];
    assert(r.frames==240&&r.excluded==1&&nearOnly[P::Frame].count==120&&none[P::Frame].count==120&&r.kinds[P::Bare][P::Frame].count==0&&r.all[P::Frame].count==240);
    assert(std::fabs(nearOnly[P::Frame].p50-20.0)<1e-4&&std::fabs(none[P::Frame].p50-10.0)<1e-4&&std::fabs(r.all[P::Frame].mean-15.0)<1e-4);
    assert(nearOnly[P::Effects].max==2.0&&none[P::Effects].max==.5&&std::fabs(nearOnly[P::Finish].mean-.3)<1e-4&&std::fabs(none[P::Present].mean-1.0)<1e-4&&std::fabs(nearOnly[P::Present].p99-4.0)<1e-4);
    // Exclusions: a sample frame; a frame that logged and the frame after it.
    frame(1,P::FarOnly,1,1,5000,true);frame(1,P::FarOnly,1,1,5000,false,true);frame(1,P::FarOnly,1,1,5000);frame(1,P::FarOnly,1,1,5000);
    r=w.take();assert(r.frames==1&&r.excluded==3&&r.kinds[P::FarOnly][P::Frame].count==1&&std::fabs(r.kinds[P::FarOnly][P::Frame].max-5.0)<1e-4);
    // Present without a preceding close (extension fault) re-primes only; bad ticks reset.
    t+=16000;w.presented(t,t+1,t+2,f,logs);w.close(1,P::Bare,false);w.presented(t+10,t+5,t+20,f,logs);w.close(1,P::Bare,false);w.presented(t+30,t+31,t+32,0,logs);
    assert(w.count()==0);
    for(unsigned i=0;i<P::FrameCostWindow::Capacity+10;++i)frame(0,P::Bare,1,1,33000); /* prime + dirty first frame, then 8 over capacity: 9 excluded */
    assert(w.count()==P::FrameCostWindow::Capacity);r=w.take();assert(r.excluded==9&&r.kinds[P::Bare][P::Frame].count==P::FrameCostWindow::Capacity&&std::fabs(r.kinds[P::Bare][P::Frame].p95-33.0)<1e-4);
    assert(P::kindOf(false,true,true)==P::Bare&&P::kindOf(true,true,true)==P::NearFar&&P::kindOf(true,true,false)==P::NearOnly&&P::kindOf(true,false,true)==P::FarOnly&&P::kindOf(true,false,false)==P::None);
    assert(std::string(P::kindName(P::NearFar))=="near+far"&&std::string(P::kindName(P::None))=="none"&&std::string(P::kindName(P::Kinds))=="all");
    std::puts("PASS frame cost: near/far kinds and all-frame percentiles of frame/effects/finish/present; sample and logging frames excluded; priming, fault, reset and capacity paths");
}

// The world renderer's loop shape: gating (continue), own span, submit (state/own/state/draw marks), record.
template<class Split> static unsigned loop(const std::vector<Packet>& packets,Split& split,std::vector<int>& issued){
    split.start();unsigned drawn=0;
    for(const auto& p:packets){
        if(p.id%5==0)continue; /* gated out: its time joins the next own span */
        split.mark(P::Own);
        split.mark(P::State);split.mark(P::Own);split.mark(P::State);
        split.constant(P::Floats);issued.push_back(p.id);split.mark(P::Draw);
        split.drawn(&p);++drawn;
    }
    split.mark(P::Own);return drawn;
}
static void split(){
    std::vector<Packet> packets;for(int i=0;i<23;++i)packets.push_back({i});
    // Off: no clock read, no recording, the identical issue sequence.
    FakeClock::reads=0;std::vector<int> offIssued,splitIssued;P::Off off;const unsigned drawnOff=loop(packets,off,offIssued);
    assert(FakeClock::reads==0&&drawnOff==18);
    // Split timed + recording: one read per span end plus start; every tick lands in exactly one bucket.
    std::vector<const Packet*> record;P::Split<FakeClock,Packet> timed;timed.timed=true;timed.record=&record;
    FakeClock::t=0;FakeClock::step=10;FakeClock::reads=0;const unsigned drawn=loop(packets,timed,splitIssued);
    assert(drawn==drawnOff&&splitIssued==offIssued&&record.size()==drawn);
    for(size_t i=0;i<record.size();++i)assert(record[i]->id==offIssued[i]);
    assert(timed.marks[P::Draw]==drawn&&timed.marks[P::State]==2*drawn&&timed.marks[P::Own]==2*drawn+1);
    assert(FakeClock::reads==timed.clockReads()&&timed.clockReads()==1+5*drawn+1);
    assert(timed.ticks[P::Own]+timed.ticks[P::State]+timed.ticks[P::Draw]==int64_t(10*(timed.clockReads()-1)));
    assert(timed.ticks[P::Draw]==int64_t(10*drawn)&&timed.ticks[P::State]==int64_t(20*drawn));
    // Recording without timing (DiagReplayProbe on an unsampled frame): no clock read.
    std::vector<const Packet*> only;P::Split<FakeClock,Packet> rec;rec.record=&only;FakeClock::reads=0;std::vector<int> ignored;
    loop(packets,rec,ignored);assert(FakeClock::reads==0&&only.size()==drawn&&rec.clockReads()==0);
    FakeClock::step=7;FakeClock::reads=0;assert(P::clockCost<FakeClock>(32)==7.0&&FakeClock::reads==34);
    // 0.3.150: the cheapest of several runs; a run with a slow read (a preemption) does not inflate it.
    FakeClock::reads=0;assert(P::clockCostMin<FakeClock>(32,8)==7.0&&FakeClock::reads==8*34);
    SlowReadClock::reads=0;assert(P::clockCost<SlowReadClock>(32)>7.0);SlowReadClock::reads=0;assert(P::clockCostMin<SlowReadClock>(32,8)==7.0&&SlowReadClock::reads==8*34);
    assert(timed.constants[P::Floats]==drawn&&timed.constants[P::Bools]==0);
    static_assert(P::Off::Draws&&P::Off::Calls&&!P::StateOnly::Draws&&P::StateOnly::Calls&&!P::LogicOnly::Draws&&!P::LogicOnly::Calls,"probe modes");
    static_assert(P::Split<FakeClock,Packet>::Draws&&P::Split<FakeClock,Packet>::Calls,"split issues everything");
    std::puts("PASS replay split: Off reads no clock and records nothing; Split attributes every tick to own/state/draw, reads 5 per draw + 2, records the drawn packets in issue order; clock cost: the cheapest of 8 runs");
}

// Informational (native, not the game): added cost of a timed Split over Off for a
// 543-draw pass (the Stormwind near cascade). In game, WORLD replay split logs clockNs.
struct SteadyClock {static int64_t now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}};
static void bench(){
    std::vector<Packet> packets;for(int i=0;i<543;++i)packets.push_back({i*5+1});
    std::vector<int> sink;sink.reserve(1<<20);
    auto run=[&](auto& policy){const auto t0=std::chrono::steady_clock::now();for(int k=0;k<200;++k){sink.clear();loop(packets,policy,sink);}
        return std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count()/200;};
    P::Off off;P::Split<SteadyClock,Packet> timed;timed.timed=true;
    const double offUs=run(off),timedUs=run(timed);
    std::printf("BENCH 543-draw pass: Off %.2f us, timed Split %.2f us, added %.1f ns per draw (5 clock reads per draw, clock %.1f ns)\n",
        offUs,timedUs,(timedUs-offUs)*1000.0/543,P::clockCost<SteadyClock>(1000));
}

static void schedule(){
    P::ProbeSchedule s;const uint32_t start=0xfffff000u; /* across the GetTickCount wrap */
    assert(s.window(start)==0&&P::ProbeSchedule::mode(0)==P::ProbeOff);
    const P::ProbeMode expected[12]={P::ProbeOff,P::ProbeFull,P::ProbeOff,P::ProbeState,P::ProbeOff,P::ProbeLogic,P::ProbeOff,P::ProbeFull,P::ProbeOff,P::ProbeState,P::ProbeOff,P::ProbeLogic};
    for(unsigned k=0;k<12;++k){const uint64_t w=s.window(start+k*P::ProbeSchedule::WindowMs+1);assert(w==k&&P::ProbeSchedule::mode(w)==expected[k]);}
    assert(s.window(start+P::ProbeSchedule::WindowMs-1)==0);
    s.reset();assert(s.window(12345)==0);
    // Every probing window sits between two off windows.
    for(uint64_t w=0;w<60;++w)if(P::ProbeSchedule::mode(w)!=P::ProbeOff)assert(w>0&&P::ProbeSchedule::mode(w-1)==P::ProbeOff&&P::ProbeSchedule::mode(w+1)==P::ProbeOff);
    assert(std::string(P::probeModeName(P::ProbeState))=="state");
    P::configure(false,true);assert(!P::profiling()&&!P::probing());P::configure(true,true);assert(P::profiling()&&P::probing());P::configure(true,false);assert(!P::probing());P::configure(false,false);
    std::puts("PASS probe schedule: 10 s windows off/full/off/state/off/logic across the tick wrap; probe needs RenderProfile");
}

static NorthlightQuality::Settings parse(const char* text,std::vector<std::string>& problems){std::istringstream in(text);return NorthlightQuality::load(&in,nullptr,problems);}
static void quality(){
    std::vector<std::string> problems;
    const NorthlightQuality::Settings d;assert(d.renderProfile==0&&d.diagReplayProbe==0&&!NorthlightQuality::renderProfile(d)&&!NorthlightQuality::replayProbe(d));
    for(auto preset:{NorthlightQuality::Preset::Quality,NorthlightQuality::Preset::Balanced,NorthlightQuality::Preset::Performance}){
        const auto s=NorthlightQuality::preset(preset);assert(s.renderProfile==0&&s.diagReplayProbe==0);}
    auto s=parse("[Quality]\nDiagReplayProbe=1\n",problems);assert(s.diagReplayProbe==1&&!NorthlightQuality::replayProbe(s)&&problems.empty()); /* needs RenderProfile */
    s=parse("[Quality]\nRenderProfile=1\n",problems);assert(NorthlightQuality::renderProfile(s)&&!NorthlightQuality::replayProbe(s));
    s=parse("[Quality]\nRenderProfile=1\nDiagReplayProbe=1\n",problems);assert(NorthlightQuality::renderProfile(s)&&NorthlightQuality::replayProbe(s));
    assert(NorthlightQuality::describe(s).find("RenderProfile=1(file) DiagReplayProbe=1(file)")!=std::string::npos&&NorthlightQuality::describe(s).find("renderProfileEffective=1 replayProbeEffective=1")!=std::string::npos);
    s=parse("[Quality]\nRenderProfile=1\nDiagReplayProbe=1\nDiagnostics=0\n",problems);assert(!NorthlightQuality::renderProfile(s)&&!NorthlightQuality::replayProbe(s)); /* Diagnostics=0 wins */
    s=parse("[Quality]\nRenderProfile=2\n",problems);assert(s.renderProfile==0&&problems.size()==1);
    unsigned i=0;for(const auto& k:NorthlightQuality::Keys){if(std::string(k.name)=="RenderProfile")break;++i;}
    assert(std::string(NorthlightQuality::Keys[i-1].name)=="Diagnostics"&&std::string(NorthlightQuality::Keys[i+1].name)=="DiagReplayProbe"&&i+2<sizeof(NorthlightQuality::Keys)/sizeof(NorthlightQuality::Keys[0]));
    std::puts("PASS RenderProfile/DiagReplayProbe keys: 0 in the code default and every preset, 0..1, probe needs RenderProfile, Diagnostics=0 wins, reported in QUALITY");
}

int main(){sampling();percentiles();frameCost();split();schedule();quality();bench();std::puts("PASS render-thread probe");}
