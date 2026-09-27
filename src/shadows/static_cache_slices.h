#pragma once
// 0.3.151 StaticCacheSlices: one static cache rect redraw spread over several
// frames. Portable, no device calls. Every band is an in-place partial redraw
// (clear + complete ordered draw list under its scissor), complete within its
// frame; the slot's key is recorded only when the last band is drawn.
#include "shadow_bounds.h"
#include <algorithm>
#include <cstdint>
#include <vector>
namespace NorthlightStaticSlices {
using NorthlightShadowBounds::TexelRect;
using Band=std::vector<TexelRect>;
// Content changes during a cycle re-band at most this often; then the cycle finishes in one frame.
static constexpr unsigned MaxRestarts=2;
// Splits disjoint rects into at most n horizontal bands of about equal texel
// area. The bands are disjoint and their union is exactly `rects`; empty bands
// are dropped. n<=1 (or one texel row) gives the input as the only band.
inline void bands(const std::vector<TexelRect>& rects,unsigned n,std::vector<Band>& out){
    out.clear();long long total=0;long top=0,bottom=0;bool any=false;
    for(const auto& r:rects){if(r.empty())continue;total+=r.area();top=any?std::min(top,r.top):r.top;bottom=any?std::max(bottom,r.bottom):r.bottom;any=true;}
    if(!any)return;
    auto below=[&](long y){long long a=0;for(const auto& r:rects)if(!r.empty()&&y>r.top)a+=(long long)(r.right-r.left)*(std::min(y,r.bottom)-r.top);return a;};
    long from=top;
    for(unsigned k=1;k<=std::max(n,1u);++k){
        long to=bottom;
        if(k<n){long lo=from,hi=bottom; /* smallest y with below(y)*n >= k*total */
            while(lo<hi){const long mid=lo+(hi-lo)/2;if(below(mid)*(long long)n>=(long long)k*total)hi=mid;else lo=mid+1;}
            to=lo;}
        if(to>from){Band band;for(const auto& r:rects){TexelRect c{r.left,std::max(r.top,from),r.right,std::min(r.bottom,to)};if(!c.empty())band.push_back(c);}
            if(!band.empty())out.push_back(std::move(band));}
        from=std::max(from,to);
    }
}
// Per-slot cycle. The recorded key keeps the pre-cycle content C0, so a new
// diff against it plus every band already drawn (which holds newer content)
// covers each texel that can differ from the newest content.
struct Cycle {
    bool active=false;const char* reason=nullptr;
    uint64_t staticSignature=0,persistentSignature=0; /* content the bands draw */
    std::vector<Band> pending;size_t next=0;
    std::vector<TexelRect> drawn; /* every band drawn since the cycle began */
    unsigned restarts=0;
    void reset(){active=false;reason=nullptr;pending.clear();next=0;drawn.clear();restarts=0;}
    bool last()const{return next+1>=pending.size();}
    const Band& band()const{return pending[next];}
    // New (or restarted) cycle over `dirty`; a restart adds the drawn bands and
    // the tile rounding of dirtyRects (a superset is still an exact redraw).
    // After MaxRestarts the whole set is one band: finished this frame.
    void start(const std::vector<TexelRect>& dirty,unsigned slices,const char* why,uint64_t staticNow,uint64_t persistentNow,long size,long tile,size_t maxRects){
        std::vector<TexelRect> all=dirty;
        if(active){all.insert(all.end(),drawn.begin(),drawn.end());std::vector<TexelRect> merged;NorthlightShadowBounds::dirtyRects(all,size,tile,maxRects,merged);all.swap(merged);++restarts;}
        else{restarts=0;drawn.clear();}
        bands(all,restarts>MaxRestarts?1:slices,pending);next=0;if(pending.empty())pending.emplace_back(); /* nothing visible: one empty band */
        active=true;reason=why;staticSignature=staticNow;persistentSignature=persistentNow;
    }
    bool current(uint64_t staticNow,uint64_t persistentNow)const{return active&&staticSignature==staticNow&&persistentSignature==persistentNow;}
    void drew(){drawn.insert(drawn.end(),pending[next].begin(),pending[next].end());++next;}
};
// A dirty set may be spread over frames only when every exit of the single-frame
// path is covered: removed persistent casters (their replay draws again at once),
// GPU diagnostics, the rect self-check and the 1-slice default finish this frame.
inline bool sliceable(unsigned slices,bool partial,bool persistentRemoved,bool diagnostic,bool verify){
    return slices>1&&partial&&!persistentRemoved&&!diagnostic&&!verify;
}
} // namespace NorthlightStaticSlices
