#pragma once
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

// 0.3.152: in-frame fork/join for the static shadow plans of the later cascade
// slots (StaticShadow::GpuCache::kickPlans). One persistent thread (cloned from
// NorthlightReplayBoundsJob::Worker), one job of up to MaxItems items run in order.
// Items are claimed under the mutex: the worker takes a pending item, or the
// render thread steals it back (claim) and builds inline. No D3D on the worker.
namespace NorthlightStaticPlanJob {
// false: every plan is built synchronously at its first use (0.3.151).
constexpr bool Async=true;
constexpr unsigned MaxItems=4;
enum class State {Pending,Running,Done,Stolen};
class Worker {
    using Clock=std::chrono::steady_clock;
    std::mutex mutex_;std::condition_variable wake_,done_;std::thread thread_;
    bool stop_=false,queued_=false,running_=false,inFlight_=false,cancel_=false;
    std::function<void(unsigned)> run_;unsigned count_=0;State states_[MaxItems]={};
    Clock::time_point queued_at_;double startLagMs_=0;unsigned retractions_=0;
    void loop(){
        for(;;){
            {std::unique_lock<std::mutex> lock(mutex_);wake_.wait(lock,[&]{return stop_||queued_;});
             if(!queued_)return;
#ifdef STATIC_SHADOW_GPU_TEST
             if(const unsigned delay=testDequeueDelayUs){lock.unlock();std::this_thread::sleep_for(std::chrono::microseconds(delay));lock.lock();if(!queued_)continue;} /* a late-scheduled worker */
#endif
             queued_=false;running_=true;startLagMs_=std::chrono::duration<double,std::milli>(Clock::now()-queued_at_).count();}
            for(unsigned i=0;;++i){
                {std::lock_guard<std::mutex> lock(mutex_);
                 if(i>=count_||cancel_){running_=false;break;}
                 if(states_[i]!=State::Pending)continue; /* stolen */
                 states_[i]=State::Running;}
                try{run_(i);}catch(...){} /* run_ reports its own failures */
                {std::lock_guard<std::mutex> lock(mutex_);states_[i]=State::Done;}
                done_.notify_all();
            }
            done_.notify_all();
        }
    }
public:
    Worker()=default;Worker(const Worker&)=delete;Worker& operator=(const Worker&)=delete;
    ~Worker(){finish(true);{std::lock_guard<std::mutex> lock(mutex_);stop_=true;}wake_.notify_all();if(thread_.joinable())thread_.join();}
    bool pending()const{return inFlight_;}
    // false: no thread (creation failed) or a job in flight; the caller builds synchronously.
    bool start(unsigned count,std::function<void(unsigned)> run){
        if(inFlight_||!count||count>MaxItems)return false;
        if(!thread_.joinable()){try{thread_=std::thread([this]{loop();});}catch(...){return false;}}
        {std::lock_guard<std::mutex> lock(mutex_);run_=std::move(run);count_=count;cancel_=false;
         for(unsigned i=0;i<count;++i)states_[i]=State::Pending;queued_at_=Clock::now();queued_=true;}
        wake_.notify_one();inFlight_=true;return true;
    }
    // Render thread: a pending item is stolen (Stolen: build it inline), a
    // running one is awaited; Done: its results are visible to the caller.
    State claim(unsigned i){
        std::unique_lock<std::mutex> lock(mutex_);
        if(states_[i]==State::Pending){states_[i]=State::Stolen;return State::Stolen;}
        done_.wait(lock,[&]{return states_[i]==State::Done||states_[i]==State::Stolen;});return states_[i];
    }
    // Waits until the worker has left the job. cancel: pending items become Stolen.
    // 0.3.152 R2: a job the worker has not dequeued yet, with nothing left pending,
    // is retracted here: no wait for the OS to schedule the thread (it keeps sleeping).
    void finish(bool cancel){
        if(!inFlight_)return;
        std::unique_lock<std::mutex> lock(mutex_);
        if(cancel){cancel_=true;for(unsigned i=0;i<count_;++i)if(states_[i]==State::Pending)states_[i]=State::Stolen;}
        if(queued_){bool pending=false;for(unsigned i=0;i<count_;++i)pending|=states_[i]==State::Pending;if(!pending){queued_=false;++retractions_;}}
        done_.wait(lock,[&]{return !queued_&&!running_;});
        inFlight_=false;run_=nullptr;
    }
    double startLagMs(){std::lock_guard<std::mutex> lock(mutex_);return startLagMs_;}
#ifdef STATIC_SHADOW_GPU_TEST
    unsigned testDequeueDelayUs=0; /* under the mutex: a retracted worker may still be in its delay */
    void testDequeueDelay(unsigned us){std::lock_guard<std::mutex> lock(mutex_);testDequeueDelayUs=us;}
    unsigned retractions(){std::lock_guard<std::mutex> lock(mutex_);return retractions_;}
#endif
};
}
