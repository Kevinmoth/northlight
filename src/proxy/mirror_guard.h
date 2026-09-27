#pragma once
#include <atomic>
#include <mutex>

// One real gate acquisition per game-facing call. Nested layers (Device ->
// resource unwrap -> MirrorDevice) see that an enclosing MirrorGuard on this
// thread already owns the SAME mutex and skip the recursive lock. Invariant:
// held_==&m only while this thread owns m through a live MirrorGuard; guards
// are strictly scoped, so restore-before-unlock keeps it exact. Other lock
// sites (RawScope, state blocks, resource proxies) stay ordinary recursive
// acquisitions and never set held_, so they are merely not accelerated.
static constexpr bool kMirrorSingleGate=true;
class MirrorGuard {
    std::recursive_mutex* owned_;
    const std::recursive_mutex* previous_;
    static inline thread_local const std::recursive_mutex* held_=nullptr;
    // 0.3.154: real acquisitions while counting (RenderProfile sample frames; the renderer sets
    // the flag once per frame). Relaxed load+store under the acquired gate, no RMW.
    static inline std::atomic<bool> counting_{false};
    static inline std::atomic<unsigned> acquisitions_{0};
public:
    explicit MirrorGuard(std::recursive_mutex& gate)
        :owned_(kMirrorSingleGate&&held_==&gate?nullptr:&gate),previous_(held_){
        if(owned_){gate.lock();held_=&gate;
            if(counting_.load(std::memory_order_relaxed))acquisitions_.store(acquisitions_.load(std::memory_order_relaxed)+1,std::memory_order_relaxed);}
    }
    ~MirrorGuard(){if(owned_){held_=previous_;owned_->unlock();}}
    MirrorGuard(const MirrorGuard&)=delete;
    MirrorGuard& operator=(const MirrorGuard&)=delete;
    static bool heldByThisThread(const std::recursive_mutex& gate){return held_==&gate;}
    static void count(bool on){counting_.store(on,std::memory_order_relaxed);}
    static unsigned takeAcquisitions(){return acquisitions_.exchange(0,std::memory_order_relaxed);}
};
