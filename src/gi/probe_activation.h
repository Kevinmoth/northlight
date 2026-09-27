#pragma once
#include "world_probe_cache.h"
#include <string>
#include <vector>

// GPU receives the first valid time for each exact world key. Re-publication
// and camera movement never restart a resident probe's fade. Modulo collisions
// and map changes must not inherit another location's activation time.
class NorthlightProbeActivation {
    struct Slot {NorthlightGI::ProbeGridKey key;float born=0;bool valid=false;};
    std::vector<Slot> slots;
    std::string map;
public:
    void reset(){slots.assign(NorthlightGI::probeLayout().atlasSize(),Slot{});map.clear();}
    // Sized by the process probe layout (0.3.153), which is fixed before the first upload.
    void begin(const std::string& next){if(map!=next||slots.size()!=NorthlightGI::probeLayout().atlasSize()){reset();map=next;}}
    float update(size_t index,const NorthlightGI::ProbeAtlasEntry& entry,float now){
        auto& slot=slots.at(index);
        if(!entry.occupied||!entry.probe.valid){slot.valid=false;return -1.f;}
        if(!slot.valid||!(slot.key==entry.key)){slot.key=entry.key;slot.born=now;slot.valid=true;}
        return slot.born;
    }
};
