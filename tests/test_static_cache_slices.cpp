// 0.3.151 StaticCacheSlices band split and cycle bookkeeping (static_cache_slices.h). Portable, no device.
#include "static_cache_slices.h"
#include <cassert>
#include <cstdio>
#include <random>
using namespace NorthlightStaticSlices;
using NorthlightShadowBounds::TexelRect;
static long long area(const std::vector<TexelRect>& rects){long long a=0;for(const auto& r:rects)a+=r.area();return a;}
static bool covered(const std::vector<TexelRect>& rects,long x,long y){for(const auto& r:rects)if(x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom)return true;return false;}
int main(){
    std::mt19937 rng(151);
    unsigned checked=0;
    for(unsigned trial=0;trial<400;++trial){
        // Disjoint input as the renderer has it: dirtyRects over random footprints.
        std::vector<TexelRect> footprints,rects;const unsigned count=1+rng()%6;
        for(unsigned i=0;i<count;++i){const long x=long(rng()%1200),y=long(rng()%1200);footprints.push_back({x,y,std::min(1280l,x+1+long(rng()%300)),std::min(1280l,y+1+long(rng()%300))});}
        NorthlightShadowBounds::dirtyRects(footprints,1280,64,8,rects);
        for(unsigned n=1;n<=4;++n){
            std::vector<Band> out;bands(rects,n,out);
            assert(!out.empty()&&out.size()<=n);
            if(n==1){assert(out.size()==1&&out[0].size()==rects.size());for(size_t i=0;i<rects.size();++i){const auto& a=out[0][i];const auto& b=rects[i];assert(a.left==b.left&&a.top==b.top&&a.right==b.right&&a.bottom==b.bottom);}}
            // Disjoint across and within bands, union exactly the input (area plus sampled coverage).
            std::vector<TexelRect> all;for(const auto& band:out){assert(!band.empty());for(const auto& r:band){assert(!r.empty());all.push_back(r);}}
            for(size_t i=0;i<all.size();++i)for(size_t j=i+1;j<all.size();++j)assert(!NorthlightShadowBounds::intersects(all[i],all[j]));
            assert(area(all)==area(rects));
            for(unsigned k=0;k<64;++k){const long x=long(rng()%1280),y=long(rng()%1280);assert(covered(all,x,y)==covered(rects,x,y));}
            // Horizontal bands in order; area balanced within one texel row of the widest extent.
            long widest=0;for(const auto& r:rects)widest+=r.right-r.left;
            for(size_t b=0;b<out.size();++b){
                if(b)for(const auto& r:out[b])for(const auto& q:out[b-1])assert(r.top>=q.bottom);
                if(out.size()==n)assert(std::llabs(area(out[b])*n-area(rects))<=(long long)widest*n);
            }
            ++checked;
        }
    }
    // Tall single rect (the common far-cascade case): four equal bands.
    {std::vector<Band> out;bands({{100,200,300,600}},4,out);assert(out.size()==4);for(const auto& b:out)assert(b.size()==1&&b[0].area()==200*100);}
    // One texel row cannot be split; empty input gives no band.
    {std::vector<Band> out;bands({{0,5,64,6}},4,out);assert(out.size()==1);bands({},4,out);assert(out.empty());bands({{3,3,3,9}},2,out);assert(out.empty());}
    // Cycle: start, bands in order, completion only at the last band; drawn accumulates.
    {Cycle c;const std::vector<TexelRect> dirty={{0,0,64,256}};
     assert(!c.active&&!c.current(1,2));
     c.start(dirty,4,"static-models-partial",1,2,1280,64,8);assert(c.active&&c.restarts==0&&c.pending.size()==4&&c.current(1,2)&&!c.current(1,3)&&!c.current(9,2));
     for(unsigned i=0;i<3;++i){assert(!c.last());c.drew();}
     assert(c.last()&&area(c.drawn)==64*192);
     // Restart after two bands: the new diff elsewhere plus every drawn band.
     Cycle r;r.start(dirty,4,"static-models-partial",1,2,1280,64,8);r.drew();r.drew();const auto drawn=r.drawn;
     r.start({{640,640,704,704}},4,"static-models-partial",5,2,1280,64,8);assert(r.restarts==1&&r.next==0&&r.current(5,2)&&area(r.drawn)==area(drawn));
     std::vector<TexelRect> pending;for(const auto& b:r.pending)pending.insert(pending.end(),b.begin(),b.end());
     for(const auto& d:drawn)for(long y=d.top;y<d.bottom;y+=7)for(long x=d.left;x<d.right;x+=7)assert(covered(pending,x,y));
     assert(covered(pending,650,650));
     // Restart cap: beyond MaxRestarts the whole set is one band, finished this frame.
     for(unsigned i=0;i<MaxRestarts;++i){r.drew();r.start({{0,900,64,964}},4,"static-models-partial",6+i,2,1280,64,8);}
     assert(r.restarts==MaxRestarts+1&&r.pending.size()==1&&r.last());
     r.reset();assert(!r.active&&r.drawn.empty()&&r.restarts==0&&!r.current(6,2));
     // Nothing visible: one empty band, finished at once.
     Cycle e;e.start({},4,"persistent-casters-partial",1,1,1280,64,8);assert(e.pending.size()==1&&e.band().empty()&&e.last());}
    // Forced single-frame finishes.
    assert(!sliceable(1,true,false,false,false)); /* the default: 0.3.150 path */
    assert(sliceable(2,true,false,false,false)&&sliceable(4,true,false,false,false));
    assert(!sliceable(4,false,false,false,false)); /* full redraws (any non-partial reason) */
    assert(!sliceable(4,true,true,false,false));  /* removed persistent casters */
    assert(!sliceable(4,true,false,true,false));  /* GPU diagnostic capture */
    assert(!sliceable(4,true,false,false,true));  /* rect self-check */
    std::printf("static cache slices: %u band splits disjoint, exact union, ordered, area-balanced; cycle/restart/cap/finish table passed\n",checked);
}
