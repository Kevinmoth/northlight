#pragma once
#include <algorithm>
#include <chrono>
#include <cstddef>

namespace NorthlightStreaming {
// A soft CPU submission deadline, never a guarantee about individual driver
// calls. Mandatory current-frame geometry is not dropped when it expires.
class Budget {
    using Clock=std::chrono::steady_clock;
    Clock::time_point start_=Clock::now();
    Clock::time_point pausedAt_{};bool paused_=false;
    size_t remaining_;
    double milliseconds_;
public:
    explicit Budget(size_t bytes=2u<<20,double milliseconds=1.0)
        :remaining_(bytes),milliseconds_(milliseconds){}
    double elapsedMs()const{return std::chrono::duration<double,std::milli>((paused_?pausedAt_:Clock::now())-start_).count();}
    bool available()const{return remaining_&&elapsedMs()<milliseconds_;}
    size_t remainingBytes()const{return remaining_;}
    size_t chunk(size_t desired)const{return available()?std::min(desired,remaining_):0;}
    void consume(size_t bytes){remaining_-=std::min(bytes,remaining_);}
    // Exclude mandatory live frame preparation: it cannot be delayed, and
    // charging it here could starve all new static casters indefinitely.
    void pause(){if(!paused_){pausedAt_=Clock::now();paused_=true;}}
    void resume(){if(paused_){start_+=Clock::now()-pausedAt_;paused_=false;}}
};
}
