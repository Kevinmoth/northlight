#include "replay_bounds_schedule.h"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
using NorthlightReplaySchedule::pass;
int main(){
    assert(pass(0,7,[]{return true;},[](size_t){assert(false);return false;})==0);
    // Cold misses must not prevent ready models later in the phase from running.
    std::vector<size_t> seen;unsigned budget=8;
    auto next=pass(8,0,[&]{return budget>0;},[&](size_t i){seen.push_back(i);--budget;return i<5;});
    assert(seen.size()==8&&seen[5]==5&&next==0);
    // A pending packet halfway through construction resumes next frame; if
    // the first packet consumes a whole phase, the following model gets a turn.
    budget=3;next=pass(8,0,[&]{return budget>0;},[&](size_t){--budget;return true;});assert(next==2);
    budget=1;next=pass(8,next,[&]{return budget>0;},[&](size_t){--budget;return true;});assert(next==3);
    // Even a model that never finishes cannot pin the tail of a crowded list.
    std::array<unsigned,40> visits{};size_t cursor=0;
    for(unsigned frame=0;frame<80;++frame){budget=1;cursor=pass(visits.size(),cursor,[&]{return budget>0;},[&](size_t i){++visits[i];--budget;return true;});}
    for(auto count:visits)assert(count==2);
    // Ready and build phases advance independently; warm work consuming its
    // entire operation allowance cannot take the cold index-validation quota.
    unsigned readyOps=196608,buildOps=65536,buildVisits=0;
    pass(8,0,[&]{return readyOps>0;},[&](size_t){readyOps=0;return true;});
    pass(8,0,[&]{return buildOps>0;},[&](size_t){buildOps-=64;++buildVisits;return true;});
    assert(buildVisits==8&&buildOps==65536-512);
    // Draw-count changes and wrapped cursors never produce out-of-range indices.
    for(size_t count=1;count<60;++count)for(size_t start:{size_t(0),size_t(100003)}){
        seen.clear();next=pass(count,start,[]{return true;},[&](size_t i){assert(i<count);seen.push_back(i);return false;});
        assert(next<count&&seen.size()==count);std::vector<bool> once(count);for(auto i:seen){assert(!once[i]);once[i]=true;}
    }
    std::cout<<"bounds scheduler: cold misses, deferred construction, independent operation quotas, starvation and changing crowd size passed\n";
}
