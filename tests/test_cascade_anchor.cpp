// 0.3.159 jump-stable cascade anchor (cascade_anchor.h): a jump leaves the anchor bit-identical,
// downhill follows at once, climbing lags by at most the 1 s window and never more than MaxLag,
// resets start a fresh window, the ring never grows.
#include "cascade_anchor.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
struct V {float x,y,z;V(float a=0,float b=0,float c=0):x(a),y(b),z(c){}};
using NorthlightCascadeAnchor::Anchor;using NorthlightCascadeAnchor::Window;using NorthlightCascadeAnchor::MaxLag;using NorthlightCascadeAnchor::Capacity;
static bool same(float a,float b){return std::memcmp(&a,&b,sizeof a)==0;}
static float arc(double s){return float(6.4*s*(1-s));} // WoW-like jump over s in [0,1] (.8 s): apex 1.6 yd
int main(){
    const V camera(-7209.3545f,-3688.1738f,19.4443f);
    for(unsigned fps:{30u,60u,144u}){
        const double dt=1000.0/fps;
        /* Stand, jump (apex 1.6 yd, .8 s), land, repeatedly: the anchor never moves. */
        Anchor a;double t=5000;unsigned frames=0,maxCount=0;const float ground=12.3456f;
        for(int jump=0;jump<6;++jump){
            for(int i=0;i<int(fps)/2;++i,t+=dt){const V p=a.update(V(100.25f,-40.5f,ground),camera,uint32_t(t),false);assert(same(p.z,ground)&&same(p.x,100.25f)&&same(p.y,-40.5f));++frames;}
            const double start=t;
            for(;t-start<800;t+=dt){const float z=ground+arc((t-start)/800);
                const V p=a.update(V(100.25f,-40.5f,z),V(camera.x,camera.y,camera.z+z-ground),uint32_t(t),false);
                assert(same(p.z,ground));++frames;maxCount=std::max(maxCount,a.count);}
        }
        assert(maxCount<=fps+1);
        /* Back-to-back jumps (bunny hops) with a single ground frame between them. */
        for(int jump=0;jump<8;++jump){
            assert(same(a.update(V(100.25f,-40.5f,ground),camera,uint32_t(t),false).z,ground));t+=dt;
            const double start=t;
            for(;t-start<800;t+=dt)assert(same(a.update(V(100.25f,-40.5f,ground+arc((t-start)/800)),camera,uint32_t(t),false).z,ground));
        }
        /* Jump onto a ledge 1.2 yd up: the anchor stays at the take-off z for a full window after the
           last ground frame, then rises without steps (the ascent of the arc, delayed) to the ledge. */
        {Anchor l;double t0=t;float previous=ground;
         for(;t-t0<500;t+=dt)l.update(V(0,0,ground),camera,uint32_t(t),false);
         const double takeoff=t-dt,start=t;const float ledge=ground+1.2f;double landed=0;
         for(;t-start<3000;t+=dt){
             const double s=(t-start)/800;const bool onLedge=s>=.5&&(s>=1||ground+arc(s)<=ledge);
             const float z=onLedge?ledge:ground+arc(s);if(onLedge&&!landed)landed=t;
             const V p=l.update(V(0,0,z),camera,uint32_t(t),false);
             if(t-takeoff<=Window)assert(same(p.z,ground));
             if(landed&&t-landed>Window+1)assert(same(p.z,ledge));
             assert(p.z>=previous&&p.z<=ledge&&p.z-previous<=float(2*8*dt/1000)+1e-5f);previous=p.z;}}
        /* Downhill and falling: the anchor equals the pivot every frame. */
        Anchor d;float z=50;
        for(int i=0;i<3*int(fps);++i,t+=dt){z-=float(12*dt/1000);const V p=d.update(V(0,0,z),camera,uint32_t(t),false);assert(same(p.z,z));}
        /* Climbing at 7 yd/s: the anchor is the pivot z of about one window ago (never more), rising
           without steps: per frame at most two frames of climb (millisecond ticks can expire two
           samples in one frame). */
        Anchor c;z=0;float previous=0;const double climbStart=t;
        for(int i=0;i<4*int(fps);++i,t+=dt){z+=float(7*dt/1000);const V p=c.update(V(0,0,z),camera,uint32_t(t),false);
            const double age=t-climbStart;const float lagged=float(7*std::max(0.0,age-Window-dt)/1000);
            assert(p.z<=z&&p.z>=lagged-1e-3f);
            if(age>Window+2*dt)assert(p.z-previous<=float(2*7*dt/1000)*1.001f+1e-5f&&p.z>=previous);
            previous=p.z;}
        /* Jumping while climbing: the anchor lies between the pure-climb anchor and that plus the climb
           during one jump (the window may open mid-arc), and moves continuously. */
        {Anchor pure,both;float zc=0,prev=0;const double t0=t;double jumpStart=t0;
         for(int i=0;i<5*int(fps);++i,t+=dt){zc+=float(7*dt/1000);
             if(t-jumpStart>=1000)jumpStart=t; /* a .8 s jump every second, .2 s on the slope between */
             const double s=(t-jumpStart)/800;const float zj=zc+(s<1?arc(s):0.f);
             const float anchorPure=pure.update(V(0,0,zc),camera,uint32_t(t),false).z,anchorBoth=both.update(V(0,0,zj),camera,uint32_t(t),false).z;
             assert(anchorBoth>=anchorPure-1e-5f&&anchorBoth<=anchorPure+float(7*.8)+1e-3f&&anchorBoth<=zj);
             if(t-t0>2*dt)assert(std::fabs(anchorBoth-prev)<=float(2*(7+8)*dt/1000)+1e-4f);
             prev=anchorBoth;}}
        /* Flying climb at 22 yd/s: lag at most one window of climb, below MaxLag, never clamped. A 40 yd/s
           rise is clamped to MaxLag below the pivot and still continuous. */
        {Anchor f;float zf=0,worst=0;
         for(int i=0;i<4*int(fps);++i,t+=dt){zf+=float(22*dt/1000);const V p=f.update(V(0,0,zf),camera,uint32_t(t),false);worst=std::max(worst,zf-p.z);}
         assert(worst<=22*(Window+dt)/1000+1e-3f&&worst<MaxLag);
         Anchor r;float zr=0,prev=0;
         for(int i=0;i<3*int(fps);++i,t+=dt){zr+=float(40*dt/1000);const V p=r.update(V(0,0,zr),camera,uint32_t(t),false);
             assert(zr-p.z<=MaxLag+1e-4f);if(i)assert(p.z-prev<=float(2*40*dt/1000)+1e-4f&&p.z>=prev);prev=p.z;}
         assert(zr-prev>=MaxLag-1e-3f);}
        /* A 35 yd rise in one frame (vertical port, knock-up; no 40 yd reset): at most MaxLag below. */
        {Anchor k;k.update(V(0,0,0),camera,uint32_t(t),false);t+=dt;
         assert(same(k.update(V(0,0,35),V(camera.x,camera.y,camera.z+35),uint32_t(t),false).z,35-MaxLag));t+=dt;}
        /* Resets: a new map, a camera jump over 40 yd and reset() take the current z at once;
           a 39 yd camera move keeps the window. */
        Anchor r;r.update(V(0,0,0),camera,uint32_t(t),false);t+=dt;
        assert(same(r.update(V(0,0,5),camera,uint32_t(t),false).z,0.f));t+=dt;
        assert(same(r.update(V(0,0,5),camera,uint32_t(t),true).z,5.f));t+=dt;
        r.update(V(0,0,1),camera,uint32_t(t),false);t+=dt;
        assert(same(r.update(V(0,0,6),V(camera.x+39,camera.y,camera.z),uint32_t(t),false).z,1.f));t+=dt;
        assert(same(r.update(V(0,0,7),V(camera.x+80,camera.y,camera.z),uint32_t(t),false).z,7.f));t+=dt;
        r.update(V(0,0,2),V(camera.x+80,camera.y,camera.z),uint32_t(t),false);r.reset();t+=dt;
        assert(same(r.update(V(0,0,9),V(camera.x+80,camera.y,camera.z),uint32_t(t),false).z,9.f));
        /* A render stall longer than the window: the old minimum has expired. */
        Anchor s;s.update(V(0,0,0),camera,uint32_t(t),false);t+=2500;
        assert(same(s.update(V(0,0,3),camera,uint32_t(t),false).z,3.f));
        std::printf("fps=%u: %u jump frames anchor bit-identical, ring entries <= %u\n",fps,frames,maxCount);
    }
    /* A full ring (a climb at 500 fps): the oldest entries drop, the window only shortens. */
    {Anchor c;float z=0;double t=0;unsigned most=0;
     for(int i=0;i<2000;++i,t+=2){z+=.01f;const V p=c.update(V(0,0,z),V(),uint32_t(t),false);most=std::max(most,c.count);
         assert(p.z<=z&&p.z>=z-.01f*Capacity-1e-3f);}
     assert(most==Capacity);}
    /* GetTickCount wrap. */
    Anchor w;w.update(V(0,0,1),V(),0xFFFFFF00u,false);
    assert(same(w.update(V(0,0,2),V(),0x00000010u,false).z,1.f));
    assert(same(w.update(V(0,0,3),V(),0x000003F0u,false).z,2.f));
    std::printf("cascade anchor: jumps, bunny hops, ledge, downhill, climb lag <= %u ms, jump while climbing, flying and MaxLag %.0f yd, resets, stall, full ring, tick wrap passed\n",Window,double(MaxLag));
}
