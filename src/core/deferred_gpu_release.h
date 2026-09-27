#pragma once
#include "streaming_budget.h"
#include <array>
#include <vector>

namespace NorthlightStreaming {
// COM releases stay on the device thread. Entire batches remain accounted
// until drained; queue saturation delays publication, never discards a caster.
template<class T> class DeferredRelease {
    struct Batch {std::vector<T*> values;size_t cursor=0,bytes=0;};
    std::array<Batch,8> batches_;
    size_t head_=0,count_=0,bytes_=0;
public:
    static constexpr size_t MaxBytes=64u<<20;
    ~DeferredRelease(){clear();}
    bool enqueue(std::vector<T*>& values,size_t bytes){
        if(values.empty())return true;
        if(count_==batches_.size()||bytes>MaxBytes-bytes_)return false;
        auto& b=batches_[(head_+count_)%batches_.size()];
        b.values.swap(values);b.cursor=0;b.bytes=bytes;++count_;bytes_+=bytes;return true;
    }
    void drain(Budget& budget,unsigned maxReleases=8){
        const double start=budget.elapsedMs();
        while(count_&&maxReleases&&budget.available()&&budget.elapsedMs()-start<.15){
            auto& b=batches_[head_];
            if(b.cursor<b.values.size()){auto*& p=b.values[b.cursor++];if(p){p->Release();p=nullptr;}--maxReleases;}
            if(b.cursor==b.values.size()){b.values.clear();bytes_-=b.bytes;b.bytes=0;head_=(head_+1)%batches_.size();--count_;}
        }
    }
    size_t bytes()const{return bytes_;}
    void clear(){for(auto& b:batches_){for(size_t i=b.cursor;i<b.values.size();++i)if(b.values[i])b.values[i]->Release();b.values.clear();b.cursor=b.bytes=0;}head_=count_=bytes_=0;}
};
}
