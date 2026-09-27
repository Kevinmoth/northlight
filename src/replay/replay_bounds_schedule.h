#pragma once
#include <cstddef>
namespace NorthlightReplaySchedule {
// A pending packet may be cold or merely too expensive for the remaining
// budget. Keep visiting affordable packets until the phase's actual budget is
// exhausted. Rotate past a first packet that consumes an entire phase so it
// cannot pin every following draw indefinitely.
template<class CanWork,class Visit>
size_t pass(size_t count,size_t start,CanWork canWork,Visit visit){
    if(!count)return 0;
    start%=count;
    for(size_t offset=0;offset<count;++offset){
        const size_t index=(start+offset)%count;
        if(!canWork())return index==start?(index+1)%count:index;
        const bool pending=visit(index);
        if(!canWork())return pending&&index!=start?index:(index+1)%count;
    }
    return (start+1)%count;
}
}
