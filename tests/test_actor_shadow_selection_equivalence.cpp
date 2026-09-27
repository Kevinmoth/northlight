// Differential test: the optimized selection with Tuning Legacy and camera
// ranking must reproduce 0.3.138 (legacy_selection.h, extracted by the python
// driver) decision for decision on randomized crowds. The new side also gets
// the draw lists the renderer now builds: identity keys only on a group's first
// draw and no palette result after a group's first multi-bone draw.
#include "legacy_selection.h"
#include "actor_shadow_selection.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
namespace N=NorthlightActorShadowSelection;namespace L=LegacyActorShadowSelection;
struct Thing {float x,y,z,vx,vy;unsigned draws,shape;std::size_t bytes[6];bool rigid;int parent;float sway;};
int main(){
    std::size_t frames=0,draws=0,kept=0,dropped=0,attached=0,orphans=0,waiting=0;double oldUs=0,newUs=0;
    for(unsigned scene=0;scene<60;++scene){
        std::mt19937 random(9000+scene);std::uniform_real_distribution<float> u(0,1);
        std::vector<Thing> things;
        const unsigned npcs=40+random()%120,fences=random()%400,shapes=3+random()%12;
        auto add=[&](bool rigid,unsigned n,float radius,std::size_t low,std::size_t high,int parent){Thing t{};t.rigid=rigid;t.draws=n;t.parent=parent;t.shape=random()%shapes;
            const float a=u(random)*6.2832f,r=2+u(random)*radius;t.x=r*std::cos(a);t.y=r*std::sin(a);t.z=u(random)*3;
            if(!rigid&&u(random)<.2f){t.vx=(u(random)-.5f)*.4f;t.vy=(u(random)-.5f)*.4f;}
            for(unsigned d=0;d<n;++d)t.bytes[d]=low+std::size_t(u(random)*float(high-low));
            t.sway=rigid&&parent<0?.002f:u(random)<.1f?3.f:.3f;things.push_back(t);};
        for(unsigned i=0;i<npcs;++i)add(false,1+random()%6,60,15000,90000,-1);
        for(unsigned i=0;i<fences;++i)add(true,1+random()%2,80,8000,30000,-1);
        for(unsigned i=0;i<npcs;i+=2){add(true,1,1,2000,8000,int(i));auto& w=things.back();w.x=(u(random)-.5f)*1.5f;w.y=(u(random)-.5f)*1.5f;w.z=1;}
        std::size_t ranked=0;for(const auto& t:things)if(!t.rigid)for(unsigned d=0;d<t.draws;++d)ranked+=t.bytes[d];
        const std::size_t budget=std::size_t(double(ranked)*(.3+.6*u(random)));
        const bool dropout=scene%3==0,orbit=scene%2==1;
        L::History oldHistory;N::History newHistory;L::Scratch oldScratch;N::Scratch newScratch;
        float camera[3]={0,0,2};
        for(unsigned frame=0;frame<240;++frame){
            if(orbit){camera[0]=15*std::cos(.03f*float(frame));camera[1]=15*std::sin(.03f*float(frame));camera[2]=6;}else{camera[0]+=.1f;}
            for(auto& t:things)if(t.parent<0){t.x+=t.vx;t.y+=t.vy;}
            std::vector<unsigned> visit(things.size());for(unsigned i=0;i<visit.size();++i)visit[i]=i;std::shuffle(visit.begin(),visit.end(),random);
            std::vector<L::Draw> oldDraws;std::vector<N::Draw> newDraws;unsigned group=0;
            for(auto id:visit){const auto& t=things[id];if(dropout&&u(random)<.05f)continue;
                float bx=t.x,by=t.y,bz=t.z;if(t.parent>=0){const auto& b=things[size_t(t.parent)];bx+=b.x;by+=b.y;bz+=b.z;}
                bool groupRigid=true;
                for(unsigned d=0;d<t.draws;++d){L::Draw w;w.index=oldDraws.size();w.bytes=t.bytes[d];w.group=group;
                    w.at[0]=bx+(u(random)-.5f)*t.sway+.3f*float(d);w.at[1]=by+(u(random)-.5f)*t.sway;w.at[2]=bz;
                    w.known=u(random)>.01f;float s=0;for(unsigned a=0;a<3;++a){const float q=w.at[a]-camera[a];s+=q*q;}w.distanceSquared=w.known?s:0;
                    w.rigid=t.rigid&&u(random)>.02f;w.bone=w.rigid?float(t.parent>=0?3:0):NAN;w.key=1000+t.shape*97+d;
                    oldDraws.push_back(w);
                    N::Draw n;n.index=w.index;n.bytes=w.bytes;n.group=w.group;std::memcpy(n.at,w.at,sizeof n.at);n.known=w.known;n.distanceSquared=w.distanceSquared;
                    n.key=d?0:w.key; /* only the first draw carries the identity */
                    if(groupRigid){n.bone=w.bone;n.rigid=w.rigid;groupRigid=w.rigid;}else{n.bone=NAN;n.rigid=false;} /* short-circuited palette test */
                    newDraws.push_back(n);}
                ++group;}
            const bool diagnostics=frame%4==0;
            auto t0=std::chrono::steady_clock::now();const auto a=L::choose(oldDraws,oldScratch,budget,oldHistory);
            auto t1=std::chrono::steady_clock::now();const auto b=N::choose(newDraws,newScratch,budget,newHistory,diagnostics,nullptr,N::Legacy);
            auto t2=std::chrono::steady_clock::now();
            oldUs+=std::chrono::duration<double,std::micro>(t1-t0).count();newUs+=std::chrono::duration<double,std::micro>(t2-t1).count();
            for(std::size_t i=0;i<oldDraws.size();++i)assert(oldDraws[i].keep==newDraws[i].keep);
            assert(a.kept==b.kept&&a.dropped==b.dropped&&a.keptBytes==b.keptBytes&&a.droppedBytes==b.droppedBytes&&a.unknown==b.unknown);
            assert(a.actors==b.actors&&a.actorsKept==b.actorsKept&&a.rigidActors==b.rigidActors&&a.rigidDraws==b.rigidDraws&&a.rigidBytes==b.rigidBytes);
            assert(a.toggles==b.toggles&&a.matched==b.matched&&a.retained==b.retained&&a.admitted==b.admitted&&a.waiting==b.waiting&&a.reserved==b.reserved);
            assert(a.attached==b.attached&&a.attachedBytes==b.attachedBytes&&a.orphans==b.orphans);
            if(diagnostics)assert(a.rigidStuck==b.rigidStuck);
            assert(oldHistory.size()==newHistory.size());
            ++frames;draws+=oldDraws.size();kept+=a.kept;dropped+=a.dropped;attached+=a.attached;orphans+=a.orphans;waiting+=a.waiting;
        }
    }
    std::printf("PASS equivalence with 0.3.138: frames=%zu draws=%zu kept=%zu dropped=%zu attached=%zu orphans=%zu waiting=%zu oldUsPerFrame=%.1f newUsPerFrame=%.1f\n",
        frames,draws,kept,dropped,attached,orphans,waiting,oldUs/double(frames),newUs/double(frames));
}
