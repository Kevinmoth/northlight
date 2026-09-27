#pragma once

// Inspect a warmed capture, not the first draw following frame invalidation.
// Keep driver readback diagnostics off the 120-frame CPU profiling samples.
// Low-activity frames may never reach this point; coverage is logged separately.
class NorthlightMirrorAuditSchedule {
    unsigned frame_=~0u,candidates_=0;
public:
    bool afterWorldCapture(unsigned frame,bool fullPaletteKnown){
        if(frame%120!=60)return false;
        if(frame_!=frame){frame_=frame;candidates_=0;}
        // Alternate broad state checks with full-palette captures. Otherwise
        // the early terrain draws can monopolize every sample in a busy city.
        if((frame/120)%2&&!fullPaletteKnown)return false;
        const unsigned target=16+(frame/120)%32;
        if(candidates_>=target)return false;
        return ++candidates_==target;
    }
};
