#pragma once
#include <cstddef>
#include <cstdint>

// Optional result memo only. This never changes source-envelope construction,
// bound validity, or the evaluator's existing operation/time budgets.
namespace NorthlightReplayMemo {
class Policy {
    std::size_t tests_=0,hits_=0;
    unsigned frames_=0,remaining_=0;
    std::uint64_t pauses_=0,retries_=0;
public:
    static constexpr unsigned CooldownFrames=120;
    bool enabled()const{return !remaining_;}
    unsigned remaining()const{return remaining_;}
    std::uint64_t pauses()const{return pauses_;}
    std::uint64_t retries()const{return retries_;}
    // Once at frame start, observing ONLY the preceding frame's memo work.
    // At least two useful frames allow a populated memo to be revisited.
    // Wait for 256 tests as well: sparse cold-source construction must not
    // disable reuse before the first real scan. Empty frames do not decide
    // profitability. A disabled memo periodically tries again so a
    // stationary/repeating view can regain exact-result reuse.
    void beginFrame(std::size_t tests,std::size_t hits){
        if(remaining_){if(!--remaining_)++retries_;return;}
        if(!tests)return;
        tests_+=tests;hits_+=hits;++frames_;
        if(frames_<2||tests_<256)return;
        if(hits_<(tests_+15)/16){remaining_=CooldownFrames;++pauses_;}
        tests_=hits_=0;frames_=0;
    }
};
}
