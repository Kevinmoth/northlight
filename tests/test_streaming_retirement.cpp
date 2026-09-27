#include "streaming_budget.h"
#include "deferred_gpu_release.h"
#include "cpu_retirement.h"
#include <atomic>
#include <cassert>
#include <iostream>
using namespace NorthlightStreaming;
struct Gpu {inline static unsigned alive=0,released=0;Gpu(){++alive;}void Release(){--alive;++released;delete this;}};
struct Cpu {
    std::atomic<bool>& entered;std::atomic<bool>& proceed;std::atomic<bool>& destroyed;
    std::thread::id producer;
    ~Cpu(){assert(std::this_thread::get_id()!=producer);entered=true;while(!proceed)std::this_thread::yield();destroyed=true;}
};
int main(){
    Budget bytes(20,1000);assert(bytes.chunk(100)==20);bytes.consume(7);assert(bytes.remainingBytes()==13);bytes.consume(100);assert(!bytes.available());
    Budget expired(100,0);assert(!expired.available()&&expired.chunk(2)==0);
    Budget paused(100,1000);paused.pause();auto time=paused.elapsedMs();assert(paused.elapsedMs()==time);paused.resume();assert(paused.elapsedMs()>=time);
    {
        DeferredRelease<Gpu> queue;
        std::vector<Gpu*> batch;for(unsigned i=0;i<20;++i)batch.push_back(new Gpu);
        assert(queue.enqueue(batch,100)&&batch.empty()&&queue.bytes()==100);
        queue.drain(expired,8);assert(Gpu::released==0);
        Budget work(100,1000);queue.drain(work,3);assert(Gpu::released==3&&queue.bytes()==100);
        std::vector<Gpu*> oversized{new Gpu};assert(!queue.enqueue(oversized,DeferredRelease<Gpu>::MaxBytes+1));assert(oversized.size()==1);oversized[0]->Release();
        for(unsigned i=0;i<7;++i){std::vector<Gpu*> next{new Gpu};assert(queue.enqueue(next,1));}
        std::vector<Gpu*> refused{new Gpu};assert(!queue.enqueue(refused,1)&&refused.size()==1);refused[0]->Release();
        queue.clear();assert(queue.bytes()==0&&Gpu::alive==0);
    }
    std::atomic<bool> entered{false},proceed{false},destroyed{false};
    {
        CpuRetirement queue;
        auto p=std::shared_ptr<Cpu>(new Cpu{entered,proceed,destroyed,std::this_thread::get_id()});
        assert(queue.retire(p,128u<<20));assert(!p);while(!entered)std::this_thread::yield();
        auto other=std::make_shared<int>(1);assert(!queue.retire(other,1)); // in-flight destruction remains charged
        proceed=true;
    }
    assert(destroyed);std::cout<<"shared budget, bounded GPU retirement, background CPU destruction passed\n";
}
