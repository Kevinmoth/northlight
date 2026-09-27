#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

// Diagnostics and the memory guard's trim trigger (memory_guard.h). Never use
// this delayed sample for memory admission, which must retain its synchronous,
// fresh allocation checks.
namespace NorthlightMemoryDiagnostics {
struct Sample {
    std::uint64_t availableVirtual=0,totalVirtual=0,availablePhysical=0;
    std::uint64_t largestFree=0,totalFree=0,regions=0;
    std::uint64_t tick=0,wallNanoseconds=0;
    bool valid=false;
};
class Sampler {
public:
    using Query=Sample(*)(void*);
private:
    Query query_;void* context_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_=false,pending_=false;
    Sample result_;
    std::atomic<bool> available_{false};
    std::thread worker_;
    void run(){
        std::unique_lock<std::mutex> lock(mutex_);
        for(;;){
            wake_.wait(lock,[&]{return stopping_||pending_;});
            if(stopping_)return;
            pending_=false;
            lock.unlock();
            Sample next;
            try{if(query_)next=query_(context_);}catch(...){next={};}
            lock.lock();
            if(stopping_)return;
            result_=next;available_.store(true,std::memory_order_release);
        }
    }
public:
    explicit Sampler(Query query,void* context=nullptr):query_(query),context_(context),worker_([this]{run();}){}
    Sampler(const Sampler&)=delete;Sampler& operator=(const Sampler&)=delete;
    ~Sampler(){
        {std::lock_guard<std::mutex> lock(mutex_);stopping_=true;pending_=false;}
        wake_.notify_one();if(worker_.joinable())worker_.join();
    }
    // At most one queued request and one completed result. A slow query never
    // holds this mutex or accumulates an unbounded queue on the render thread.
    void request(){
        {std::lock_guard<std::mutex> lock(mutex_);if(stopping_)return;pending_=true;}
        wake_.notify_one();
    }
    bool take(Sample& sample){
        if(!available_.load(std::memory_order_acquire))return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(!available_.exchange(false,std::memory_order_relaxed))return false;
        sample=result_;return true;
    }
};
}
