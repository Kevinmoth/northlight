#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#endif

// One real gate acquisition per game-facing call. Nested layers (Device ->
// resource unwrap -> MirrorDevice) see that an enclosing MirrorGuard on this
// thread already owns the SAME gate and skip the recursive lock. Invariant:
// held==&gate only while this thread owns gate.mutex through a live MirrorGuard;
// guards are strictly scoped, so restore-before-unlock keeps it exact.
// 0.3.180 (D0, r88): every lock site of a device gate is a MirrorGuard (device,
// registry, resource proxies and forwarders, state blocks, swap chains, RawScope),
// so their nested re-locks are skipped too, and each real acquisition feeds the
// thread census: foreign (not the CreateDevice caller) entries per site class.
static constexpr bool kMirrorSingleGate=true;
// Test counterfactual only: 1 keys "foreign" on the TLS held pointer instead of the thread id.
#ifndef NORTHLIGHT_GATE_CENSUS_BY_HELD
#define NORTHLIGHT_GATE_CENSUS_BY_HELD 0
#endif
enum class MirrorSite:unsigned {Device,Registry,Resource,StateBlock,SwapChain,Raw,Buffer,Count};
inline const char* mirrorSiteName(unsigned site){
    static const char* const names[]={"device","registry","resource","stateblock","swapchain","raw","buffer"};
    return site<unsigned(MirrorSite::Count)?names[site]:"none";
}
struct MirrorGate {
    static constexpr unsigned Sites=unsigned(MirrorSite::Count);
    std::recursive_mutex mutex;
    // The CreateDevice caller: the constructing thread (DeviceMirror is a Device member; the Device
    // constructor also sets it explicitly). Written before the gate is shared, read-only afterwards.
    std::uint32_t ownerTid;
    // Census (relaxed): foreign entries per site class; the first foreign {tid,site,frame}, claimed by
    // CAS and published by firstReady; the first Present/SwapChain::Present/draw callers.
    std::atomic<std::uint32_t> foreign[Sites]={};
    std::atomic<bool> firstClaimed{false},firstReady{false};
    std::uint32_t firstTid=0,firstSite=Sites,firstFrame=0;
    std::atomic<std::uint32_t> frame{0},presentTid{0},swapPresentTid{0},drawTid{0};
    // Owner real acquisitions per site class while counting (RenderProfile sample frames). Written by the
    // owner under the mutex; read and cleared by the owner under the mutex (DRAWGATE ab).
    std::atomic<bool> counting{false};
    std::atomic<std::uint32_t> acquired[Sites]={};
    // Called once per site class at its first foreign entry (the gate is held for gated sites).
    void(*report)(void*,unsigned site,std::uint32_t tid)noexcept=nullptr;void* reportContext=nullptr;
    MirrorGate();
    MirrorGate(const MirrorGate&)=delete;
    MirrorGate& operator=(const MirrorGate&)=delete;
    // A real acquisition (MirrorGuard's locking branch). Nested entries never reach it: they run on
    // the thread that owns the mutex already.
    void entered(MirrorSite site,bool foreignThread);
    // Tracked vertex/index buffers take no gate: Lock/Unlock/Release report their thread here.
    void noteBuffer();
    void noteFirst(std::atomic<std::uint32_t>& slot);
    std::uint32_t takeAcquired(MirrorSite site){return acquired[unsigned(site)].exchange(0,std::memory_order_relaxed);}
    [[gnu::cold]] [[gnu::noinline]] void foreignEntry(MirrorSite site,std::uint32_t tid);
};
// One TLS block per thread: the gate its innermost owning MirrorGuard holds, and its cached thread id.
struct MirrorThread {const MirrorGate* held=nullptr;std::uint32_t tid=0;};
class MirrorGuard {
    using Thread=MirrorThread;
    static inline thread_local Thread thread_;
    MirrorGate* owned_;
    const MirrorGate* previous_;
public:
    // The calling thread's id, from TLS after its first use (no GetCurrentThreadId call per entry).
    static std::uint32_t threadId(){
        Thread& t=thread_;
        if(!t.tid){
#ifdef _WIN32
            t.tid=GetCurrentThreadId();
#else
            static std::atomic<std::uint32_t> next{1};t.tid=next.fetch_add(1,std::memory_order_relaxed);
#endif
        }
        return t.tid;
    }
    explicit MirrorGuard(MirrorGate& gate,MirrorSite site=MirrorSite::Device):owned_(nullptr),previous_(thread_.held){
        if(kMirrorSingleGate&&previous_==&gate)return;
        gate.mutex.lock();owned_=&gate;thread_.held=&gate;
        gate.entered(site,NORTHLIGHT_GATE_CENSUS_BY_HELD?previous_!=nullptr:threadId()!=gate.ownerTid);
    }
    ~MirrorGuard(){if(owned_){thread_.held=previous_;owned_->mutex.unlock();}}
    MirrorGuard(const MirrorGuard&)=delete;
    MirrorGuard& operator=(const MirrorGuard&)=delete;
    static bool heldByThisThread(const MirrorGate& gate){return thread_.held==&gate;}
    // 0.3.180 (D0): r88 §3.3's owner entry and exit (thread id compare, xchg, seq_cst load, TLS held
    // set and restore, release store) on atomics the caller owns, never a live gate's: DRAWGATE ownerNs.
    static bool ownerProbe(const MirrorGate& gate,std::atomic<unsigned>& inside,const std::atomic<unsigned>& foreignActive){
        if(threadId()!=gate.ownerTid)return false;
        inside.exchange(1,std::memory_order_seq_cst);
        const bool elided=foreignActive.load(std::memory_order_seq_cst)==0;
        if(elided){Thread& t=thread_;const MirrorGate* previous=t.held;t.held=&gate;std::atomic_signal_fence(std::memory_order_seq_cst);t.held=previous;}
        inside.store(0,std::memory_order_release);
        return elided;
    }
};
inline MirrorGate::MirrorGate():ownerTid(MirrorGuard::threadId()){}
inline void MirrorGate::entered(MirrorSite site,bool foreignThread){
    if(foreignThread){foreignEntry(site,MirrorGuard::threadId());return;}
    if(counting.load(std::memory_order_relaxed)){auto& n=acquired[unsigned(site)];n.store(n.load(std::memory_order_relaxed)+1,std::memory_order_relaxed);}
}
inline void MirrorGate::noteBuffer(){const std::uint32_t tid=MirrorGuard::threadId();if(tid!=ownerTid)foreignEntry(MirrorSite::Buffer,tid);}
inline void MirrorGate::noteFirst(std::atomic<std::uint32_t>& slot){if(!slot.load(std::memory_order_relaxed))slot.store(MirrorGuard::threadId(),std::memory_order_relaxed);}
inline void MirrorGate::foreignEntry(MirrorSite site,std::uint32_t tid){
    const std::uint32_t before=foreign[unsigned(site)].fetch_add(1,std::memory_order_relaxed);
    bool expected=false;
    if(firstClaimed.compare_exchange_strong(expected,true,std::memory_order_relaxed)){
        firstTid=tid;firstSite=unsigned(site);firstFrame=frame.load(std::memory_order_relaxed);firstReady.store(true,std::memory_order_release);}
    if(!before&&report)report(reportContext,unsigned(site),tid);
}
