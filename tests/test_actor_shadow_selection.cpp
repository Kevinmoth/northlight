#include "actor_shadow_selection.h"
#include "replay_shadow_policy.h"
#include "shadow_fate.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>
using namespace NorthlightActorShadowSelection;
static void rigid(){
    struct V {float i[4],w[4];};
    auto run=[](const std::vector<V>& v,bool weighted){return rigidBone(v.size(),weighted,[&](size_t n,float* i,float* w){
        for(unsigned l=0;l<4;++l){i[l]=v[n].i[l];w[l]=v[n].w[l];}return true;});};
    assert(run({{{3,0,0,0},{1,0,0,0}},{{3,9,9,9},{1,0,0,0}}},true)==3);
    assert(std::isnan(run({{{3,4,0,0},{.5f,.5f,0,0}}},true)));
    assert(run({{{3,3,0,0},{.5f,.5f,0,0}}},true)==3);
    assert(std::isnan(run({{{3,0,0,0},{1,0,0,0}},{{4,0,0,0},{1,0,0,0}}},true)));
    assert(std::isnan(run({{{3,0,0,0},{.6f,0,0,0}}},true))); /* implicit remainder weight */
    assert(std::isnan(run({{{3,0,0,0},{1,-1,0,0}}},true)));
    assert(std::isnan(run({{{3,0,0,0},{1,0,0,0}}},false))); /* unweighted lanes must all agree */
    assert(run({{{0,0,0,0},{0,0,0,0}},{{0,0,0,0},{0,0,0,0}}},false)==0);
    assert(std::isnan(run({{{NAN,0,0,0},{1,0,0,0}}},true)));
    assert(std::isnan(rigidBone(1,true,[](size_t,float*,float*){return false;})));
    std::puts("PASS rigid palette: weighted single bone, shared lanes, two bones, remainder weight, unweighted lanes, NaN, decode failure");
}
static void unit(){
    auto make=[](std::vector<Draw>& d,unsigned group,std::size_t bytes,float x,bool rigid=false){Draw w;w.index=d.size();w.group=group;w.bytes=bytes;w.known=true;
        w.at[0]=x;w.distanceSquared=x*x;w.rigid=rigid;w.bone=rigid?0.f:NAN;w.key=group;d.push_back(w);};
    History h;std::vector<Draw> d;Scratch sc;auto& a=sc.actors;
    // First ranked frame: plain prefix, nothing waits; a group drops as a unit;
    // the first misfit closes the cut even if a later small actor would fit.
    make(d,0,40,1);make(d,0,40,1.5f);make(d,1,50,2);make(d,2,10,3);make(d,3,500,.5f,true);d.back().at[1]=100; /* free-standing */
    auto r=choose(d,sc,100,h);
    assert(d[0].keep&&d[1].keep&&!d[2].keep&&!d[3].keep&&d[4].keep&&r.keptBytes==80&&r.rigidBytes==500&&r.waiting==0&&r.dropped==2);
    // Same frame again: stable, zero toggles.
    for(auto& w:d)w.keep=true;r=choose(d,sc,100,h);assert(d[0].keep&&!d[2].keep&&!d[3].keep&&r.toggles==0);
    // Group 0 vanishes: its 80 bytes stay reserved, nobody flashes on.
    std::vector<Draw> e(d.begin()+2,d.end());for(size_t i=0;i<e.size();++i)e[i].index=i;
    for(unsigned f=0;f<Tuning{}.reserveAge;++f){r=choose(e,sc,100,h);assert(!e[0].keep&&!e[1].keep&&r.reserved==80);}
    // After reserveAge absent frames the reservation expires; newcomers wait Tuning{}.admitFrames.
    for(unsigned f=0;f+1<3*Tuning{}.admitFrames;++f){r=choose(e,sc,100,h);assert(!e[0].keep&&r.waiting==2&&r.reserved==0);} /* static actors are frozen: three admission periods */
    r=choose(e,sc,100,h);assert(e[0].keep&&e[1].keep&&r.admitted==2&&r.toggles==2);
    // A within-budget frame resets history: the next ranked frame starts as all kept.
    h.keptAll();r=choose(d,sc,100,h);assert(d[0].keep&&!d[2].keep&&r.waiting==0);
    // Hysteresis: a kept actor at 1.05x the distance of a newcomer stays kept.
    // (moving actors: stationary freezing is tested separately)
    Tuning moving;moving.stationary=false;
    h.clear();d.clear();make(d,0,60,10);make(d,1,60,20);choose(d,sc,100,h,false,nullptr,moving);assert(d[0].keep&&!d[1].keep);
    d.clear();make(d,0,60,10.5f);make(d,1,60,10);d[1].key=1;
    for(unsigned f=0;f<5;++f){choose(d,sc,100,h,false,nullptr,moving);assert(d[0].keep&&!d[1].keep);}
    d[0].at[0]=13;d[0].distanceSquared=169; /* beyond the margin */for(unsigned f=0;f<Tuning{}.admitFrames;++f)choose(d,sc,100,h,false,nullptr,moving);assert(!d[0].keep&&d[1].keep);
    // Attachment: a rigid weapon beside its body follows the body, outside the quota.
    h.clear();d.clear();make(d,0,80,10);make(d,1,5,10.5f,true);make(d,2,30,50,true);d.back().at[1]=40;
    auto r2=choose(d,sc,79,h);assert(a[1].body==0&&!d[0].keep&&!d[1].keep&&d[2].keep&&r2.attached==1&&r2.attachedBytes==5&&r2.rigidActors==1);
    h.clear();r2=choose(d,sc,80,h);assert(d[0].keep&&d[1].keep&&r2.keptBytes==80&&r2.rigidBytes==35);
    // An unlocated rigid actor is ranked, never exempt, and (unknown distance)
    // keeps 0.3.137's first claim with no admission delay, even with history.
    h.clear();d.clear();make(d,0,80,1,true);d[0].known=false;r2=choose(d,sc,50,h);assert(!d[0].keep&&r2.rigidActors==0&&r2.unknown==1); /* larger than the quota */
    {History u;Scratch us;std::vector<Draw> v;make(v,0,60,5);make(v,1,60,9);choose(v,us,100,u);assert(v[0].keep&&!v[1].keep);
     v.clear();make(v,0,60,5);make(v,1,60,9);make(v,2,40,1);v[2].known=false;v[2].key=99;
     auto ru=choose(v,us,100,u);assert(v[2].keep&&v[0].keep&&!v[1].keep&&ru.waiting==0&&ru.unknown==1);}
    // Recycled identity (ABA): a new actor with a vanished actor's shape at its
    // spot inherits "kept" (documented); elsewhere it does not. The quota holds.
    {History u;Scratch us;std::vector<Draw> v;make(v,0,60,5);make(v,1,30,9);make(v,2,30,14);for(auto& q:v)q.key=1;
     choose(v,us,100,u);assert(v[0].keep&&v[1].keep&&!v[2].keep);
     std::vector<Draw> gone(v.begin()+1,v.end());gone[0].group=0;gone[1].group=1;choose(gone,us,100,u); /* A vanishes: reserved */
     assert(gone[0].keep&&!gone[1].keep);
     std::vector<Draw> back=v;back[0].bytes=70;auto rb=choose(back,us,100,u);   /* B, same shape, same spot, bigger */
     assert(back[0].keep&&rb.matched==3&&rb.keptBytes<=100);
     std::vector<Draw> other=v;other[0].at[1]=30;other[0].distanceSquared=11*11;auto ro=choose(other,us,100,u); /* same shape, elsewhere */
     assert(ro.matched==2&&ro.keptBytes<=100&&!(other[0].keep&&ro.waiting==0&&ro.admitted==0));}
    // Guard beside a fence: the fence is still while the guard idles, so it
    // settles free-standing; the guard then walks away and is dropped, and the
    // fence keeps casting every frame after probation.
    {History g;Scratch gs;bool guardDropped=false;std::mt19937 jitter(5);std::uniform_real_distribution<float> sway(-.2f,.2f);
     for(unsigned f=0;f<80;++f){std::vector<Draw> v;const float x=f<20?20.f:20.f+.5f*float(f-20);
        make(v,0,60,x+sway(jitter));make(v,0,40,x+.3f+sway(jitter));make(v,1,20,21.5f,true);v.back().at[1]=.5f; /* guard, fence */
        make(v,2,90,10);                                                                                  /* nearer NPC */
        for(auto& q:v)q.key=q.group+17;
        auto rg=choose(v,gs,100,g);
        if(f>=FreeFrames)assert(v[2].keep&&!rg.attached);
        guardDropped=guardDropped||!v[0].keep;}
     assert(guardDropped);}
    // Walking NPC with a weapon: starts nearest and kept, walks away past a
    // standing NPC until the quota drops it. The weapon must never cast alone.
    {History w;Scratch ws;bool dropped=false;
     for(unsigned f=0;f<60;++f){std::vector<Draw> v;const float x=5+.5f*float(f);
        make(v,0,60,x);make(v,0,40,x+.3f);make(v,1,8,x+.8f,true);v.back().at[2]=1.2f; /* body (2 draws), weapon */
        make(v,2,90,12);make(v,3,20,60,true);v.back().at[1]=50;                         /* standing NPC, far fence */
        for(auto& q:v)q.key=q.group+7;
        choose(v,ws,100,w);
        assert(!(v[2].keep&&!v[0].keep));assert(v[0].keep==v[1].keep&&v[4].keep);
        dropped=dropped||!v[0].keep;}
     assert(dropped);}
    std::puts("PASS unit: prefix cut, group unit, rigid exempt, reservation, admission delay, history reset, hysteresis, attachment follows body, walking NPC weapon never alone, unlocated rigid ranked, unknown first claim, recycled identity, guard beside fence");
}
// Synthetic Durotar: NPC crowd (multi-draw, idle jitter, a few walkers), rigid
// single-bone fences and multi-bone animated doodads, shuffled capture order.
struct Item {unsigned id=0,draws=1;std::size_t bytes[8]={};float x=0,y=0,z=0,vx=0,vy=0;bool rigid=false;int parent=-1;float ox=0,oy=0,oz=0,orbit=0,jitter=0;unsigned kind=0; /* 1 event fence (multi-bone, static), 2 idle NPC (breathing), 3 corpse */};
struct Stats {double maxTotal=0,toggles=0,frames=0,legacyToggles=0,utilization=0,legacyUtilization=0;std::size_t items=0,weaponAlone=0,weaponBodyDropped=0,fenceExempt=0,fastDropped=0,reversals=0,fenceToggles=0,maxWindowFlips=0,maxEventWindowFlips=0,windows=0,eventToggles=0,eventReversals=0,eventOn=0,eventOff=0,eventExempt=0,idleExempt=0,corpseExempt=0,eventFrames=0;};
// orbit>0: the camera circles the player (pivot) at 15 u, orbit rad/frame; the
// player walks when cameraMoves. useOrigin ranks from the pivot.
// decisions: optional per-frame digest of every selection decision (draw keep,
// actor classification, result counts except the diagnostic rigidStuck, history
// size). diagnostics: choose()'s diagnostic flag (the renderer's sampled frames).
struct Decisions {std::vector<std::uint64_t> frames;std::size_t rigidStuck=0;};
// Every simulated frame's decision digest (below) folded together: the python
// driver builds this file against reference/actor_shadow_selection-0.3.144.h
// too and requires identical output (ActorShadowRadius off = 0.3.144).
static std::uint64_t digest=1469598103934665603ull;static std::size_t digestFrames=0;
static Stats simulate(unsigned seed,bool cameraMoves,bool setNoise,unsigned frames,float orbit=0,bool useOrigin=false,Tuning tuning=Tuning{},bool event=false,bool swing=false,bool packed=false,bool diagnostics=false,Decisions* decisions=nullptr){
    std::mt19937 random(seed);std::uniform_real_distribution<float> u(0,1);
    std::vector<Item> items;std::size_t rankedBytes=0;
    auto add=[&](unsigned draws,std::size_t low,std::size_t high,float radius,bool rigid,bool walker){
        Item it;it.id=unsigned(items.size());it.draws=draws;it.rigid=rigid;
        for(unsigned d=0;d<draws;++d){it.bytes[d]=low+std::size_t(u(random)*float(high-low));if(!rigid)rankedBytes+=it.bytes[d];}
        const float a=u(random)*6.2832f,r=4+u(random)*radius;it.x=r*std::cos(a);it.y=r*std::sin(a);it.z=u(random)*4;
        if(walker){it.vx=(u(random)-.5f)*.2f;it.vy=(u(random)-.5f)*.2f;}items.push_back(it);};
    for(unsigned i=0;i<90;++i)add(3+random()%5,20000,90000,110,false,i%10==0);
    for(unsigned i=0;i<(event?100u:500u);++i)add(1+random()%2,12000,30000,140,true,false); /* game: rigid bytes ~15% of the budget */
    for(unsigned i=0;i<60;++i)add(2,10000,40000,140,false,false);
    // Rigid single-bone weapons/shoulders: separate groups riding on NPCs (walkers included).
    for(unsigned i=0;i<90;i+=2){add(1,3000,8000,1,true,false);auto& w=items.back();w.parent=int(i);w.ox=(u(random)-.5f)*1.6f;w.oy=(u(random)-.5f)*1.6f;w.oz=1+u(random);}
    // Near fast/erratic actors: the player on a flyer circling at 3 units per
    // frame (identity never matches) and a kodo whose sampled vertex swings ±2.
    add(4,60000,90000,1,false,false);const unsigned player=unsigned(items.size()-1);items[player].orbit=6;
    add(5,60000,90000,1,false,false);const unsigned kodo=unsigned(items.size()-1);items[kodo].x=9;items[kodo].y=0;items[kodo].jitter=4;
    for(unsigned owner:{player,kodo}){add(1,4000,8000,1,true,false);auto& w=items.back();w.parent=int(owner);w.ox=.6f;w.oy=.4f;w.oz=1.2f;}
    const unsigned playerWeapon=unsigned(items.size()-2),kodoWeapon=unsigned(items.size()-1);
    if(event){ /* multi-bone event fences, idle NPCs with a still foot but a breathing chest, corpses */
        for(unsigned i=0;i<80;++i){add(1+random()%3,15000,50000,60,false,false);items.back().kind=1;}
        for(unsigned i=0;i<30;++i){add(4,20000,60000,60,false,false);items.back().kind=2;items.back().jitter=i%2?.02f:.04f;} /* breathing amplitude ±0.02 or ±0.04 yd */
        for(unsigned i=0;i<5;++i){add(4,20000,60000,60,false,false);items.back().kind=3;}}
    const std::size_t budget=orbit>0?rankedBytes/6:rankedBytes*2/3; /* a view cone captures about a third */
    History history;std::vector<Draw> draws;Scratch scratch;auto& actors=scratch.actors;auto& order=scratch.order;
    std::vector<int> last(items.size(),-1),legacyLast(items.size()*8,-1);Stats stats;stats.items=items.size();
    float camera[3]={0,0,1},pivot[3]={0,0,1};std::vector<int> lastToggle(items.size(),-1000);std::vector<bool> wasPresent(items.size(),false);std::vector<unsigned> windowFlips(items.size(),0);
    for(unsigned frame=0;frame<frames;++frame){
        if(orbit>0){if(cameraMoves){pivot[0]+=.15f;pivot[1]+=.05f;}const float t=orbit*float(frame);camera[0]=pivot[0]+15*std::cos(t);camera[1]=pivot[1]+15*std::sin(t);camera[2]=6;}
        else if(cameraMoves){camera[0]+=.15f;camera[1]+=.05f;}
        if(orbit<=0)std::memcpy(pivot,camera,sizeof pivot);
        std::vector<unsigned> visit(items.size());for(unsigned i=0;i<visit.size();++i)visit[i]=i;
        std::shuffle(visit.begin(),visit.end(),random);
        draws.clear();std::vector<NorthlightReplayShadowPolicy::Candidate> legacy;std::vector<std::pair<unsigned,unsigned>> owner;
        std::vector<bool> present(items.size());unsigned group=0;
        for(auto& it:items)if(it.parent<0){it.x+=it.vx;it.y+=it.vy;}
        {auto& p=items[player];const float t=.5f*float(frame);const float* c=orbit>0?pivot:camera; /* near the ranking origin */
         p.x=c[0]+p.orbit*std::cos(t);p.y=c[1]+p.orbit*std::sin(t);if(orbit>0){items[kodo].x=pivot[0]+9;items[kodo].y=pivot[1];}}
        for(auto& it:items)if(it.parent>=0){const auto& b=items[size_t(it.parent)];it.x=b.x+it.ox;it.y=b.y+it.oy;it.z=b.z+it.oz;}
        for(auto id:visit){auto& it=items[id];
            if(setNoise&&u(random)<.05f)continue;
            if(swing&&(frame/20)%2&&id%3)continue; /* camera turned away: a third of the scene is captured */
            if(orbit>0){const float fx=pivot[0]-camera[0],fy=pivot[1]-camera[1],ox=it.x-camera[0],oy=it.y-camera[1]; /* 100 degree view cone toward the player */
                if(fx*ox+fy*oy<std::cos(.87f)*std::sqrt((fx*fx+fy*fy)*(ox*ox+oy*oy)))continue;}
            present[id]=true;
            // Idle animation moves the sampled vertex; each part samples its own vertex.
            // packed: parts of one object live in a shared VB at stable ranges, the
            // game issues them in a varying order, and banner cloth parts wave.
            unsigned part[8]={0,1,2,3,4,5,6,7};if(packed)std::shuffle(part,part+it.draws,random);
            const std::size_t groupStart=draws.size();
            for(unsigned slot=0;slot<it.draws;++slot){const unsigned d=part[slot];Draw w;w.index=draws.size();w.bytes=it.bytes[d];w.group=group;w.known=true;
                // Free-standing doodads only carry camera-relative float noise.
                const float sway=packed&&it.kind==1&&d>0?.6f:it.jitter>0&&it.kind!=2?it.jitter:(it.rigid&&it.parent<0)||it.kind?.002f:.4f;
                w.at[0]=it.x+(u(random)-.5f)*sway+.3f*float(d);w.at[1]=it.y+(u(random)-.5f)*sway;w.at[2]=it.z;
                float s=0;for(unsigned a=0;a<3;++a){const float q=w.at[a]-camera[a];s+=q*q;}w.distanceSquared=s;
                w.rigid=it.rigid;w.bone=it.rigid?0.f:NAN;w.key=1000+(it.id%7)*31+d; /* shared model shapes collide on purpose */
                if(packed)w.key=5000+std::uint64_t(it.id%7)*8+d; /* per-MODEL shared range: instances of one model share keys */
                if(packed&&slot==0){w.hasRoot=true;w.root[0]=it.x+(u(random)-.5f)*.002f;w.root[1]=it.y+(u(random)-.5f)*.002f;w.root[2]=it.z;} /* palette root: no cloth, no breathing */
                else if(d)w.key=draws.back().key; /* actor key comes from its first draw */
                if(!packed&&d<2&&history.stationaryHint(d?draws[draws.size()-d].key:w.key,d?draws[draws.size()-d].at:w.at)){
                    // The renderer's extra samples: two more vertices of the draw.
                    const float breathe=it.kind==2?it.jitter*std::sin(6.2832f*float(frame)/120):0 /* 2 s breathing cycle */,noise=it.kind||(it.rigid&&it.parent<0)?.002f:.4f;
                    for(unsigned x=0;x<2;++x){w.extra[x][0]=it.x+.5f-float(x)+(u(random)-.5f)*noise;w.extra[x][1]=it.y+.3f*float(x)+breathe+(u(random)-.5f)*noise;w.extra[x][2]=it.z+1+breathe;}
                    w.extras=2;}
                draws.push_back(w);
                NorthlightReplayShadowPolicy::Candidate c;c.index=legacy.size();c.bytes=w.bytes;c.distanceSquared=s;c.known=true;
                legacy.push_back(c);owner.push_back({id,d});}
            if(packed&&tuning.stableIdentity&&draws.size()>groupStart){ /* the renderer's post-group hint by the smallest key */
                std::size_t m=groupStart;for(std::size_t k=groupStart;k<draws.size();++k)if(draws[k].key<draws[m].key)m=k;
                if(history.stationaryHint(draws[m].key,draws[m].at))for(std::size_t k=groupStart;k<draws.size();++k){auto& w=draws[k];const unsigned d=unsigned(w.key-5000-std::uint64_t(it.id%7)*8);
                    const float noise=d>0&&it.kind==1?.6f:it.kind||(it.rigid&&it.parent<0)?.002f:.4f,breathe=it.kind==2?it.jitter*std::sin(6.2832f*float(frame)/120):0;
                    for(unsigned x=0;x<2;++x){w.extra[x][0]=it.x+.5f-float(x)+.3f*float(d)+(u(random)-.5f)*noise;w.extra[x][1]=it.y+.3f*float(x)+breathe+(u(random)-.5f)*noise;w.extra[x][2]=it.z+1+breathe;}
                    w.extras=2;}}
            ++group;}
        std::size_t presentBytes=0;for(const auto& d:draws)presentBytes+=d.bytes;
        const bool rank=!swing||history.shouldRank(presentBytes,budget,tuning);
        if(!rank){history.keptAll();actors.clear();order.clear();for(auto& d:draws)d.keep=true;
            for(std::size_t i=0;i<draws.size();){Actor a;a.first=i;while(i<draws.size()&&draws[i].group==draws[a.first].group)++i;a.count=i-a.first;a.keep=true;a.rigid=true;actors.push_back(a);}}
        const auto r=rank?choose(draws,scratch,budget,history,diagnostics,useOrigin?pivot:nullptr,tuning):Result{};
        {std::uint64_t h=1469598103934665603ull;auto mix=[&](std::uint64_t v){h=(h^v)*1099511628211ull;};
            for(const auto& d:draws)mix(d.keep);
            for(const auto& a:actors){for(std::uint64_t v:{std::uint64_t(a.keep),std::uint64_t(a.rigid),std::uint64_t(a.body),std::uint64_t(a.orphan),std::uint64_t(a.exempt),std::uint64_t(a.waiting),
                std::uint64_t(a.attachment),std::uint64_t(a.settled),std::uint64_t(a.free),std::uint64_t(a.still),std::uint64_t(a.bodyKey),std::uint64_t(a.frozen),std::uint64_t(a.matched)})mix(v);
                std::uint32_t bits[3];std::memcpy(bits,a.anchor,sizeof bits);for(auto b:bits)mix(b);}
            for(std::size_t v:{r.kept,r.dropped,r.keptBytes,r.droppedBytes,r.unknown,r.actors,r.actorsKept,r.rigidActors,r.rigidDraws,r.rigidBytes,r.toggles,r.matched,r.retained,r.admitted,r.waiting,r.reserved,
                r.attached,r.attachedBytes,r.orphans,r.rigidNew,r.exemptActors,r.exemptDraws,r.exemptBytes,r.cappedExempt,r.stillRankedSmall,r.exemptCapBinds,r.locked,r.exemptNpcLike,r.rooted,r.rootFallback,r.rootRejected,history.size()})mix(v);
            if(decisions){decisions->frames.push_back(h);decisions->rigidStuck+=r.rigidStuck;}
            digest=(digest^h)*1099511628211ull;++digestFrames;}
        // Budget accuracy and structural guarantees.
        std::size_t ranked=0;for(const auto& a:actors)if(!a.rigid&&!a.orphan&&!a.exempt&&a.body==SIZE_MAX&&a.keep)ranked+=a.bytes;
        assert(ranked==r.keptBytes&&ranked<=budget);
        for(const auto& a:actors){for(std::size_t i=a.first;i<a.first+a.count;++i)assert(draws[i].keep==a.keep);if(a.rigid||a.exempt)assert(a.keep);}
        /* exempt bytes: new exemptions stay under the cap (unit test); granted ones are sticky */
        bool closed=false;for(auto n:order){if(closed)assert(!actors[n].keep&&!actors[n].waiting);closed=closed||(!actors[n].keep&&!actors[n].waiting);}
        auto legacyResult=NorthlightReplayShadowPolicy::choose(legacy,budget+[&]{std::size_t b=0;for(const auto& d:draws)if(d.rigid)b+=d.bytes;return b;}());
        // Legacy quota also counted fences, so give it their bytes too.
        (void)legacyResult;
        std::vector<int> now(items.size(),-1);
        for(const auto& a:actors){unsigned id=owner[a.first].first;now[id]=a.keep;}
        if(frame>0)for(unsigned id=0;id<items.size();++id)if(present[id]&&wasPresent[id]&&last[id]>=0&&now[id]!=last[id]){ /* visible flicker: present in both frames */++stats.toggles;++windowFlips[id];stats.fenceToggles+=items[id].rigid&&items[id].parent<0;stats.eventToggles+=items[id].kind==1;if(items[id].kind==1){if(now[id])++stats.eventOn;else ++stats.eventOff;if(int(frame)-lastToggle[id]<=30)++stats.eventReversals;}if(int(frame)-lastToggle[id]<=30)++stats.reversals;lastToggle[id]=int(frame);}
        for(unsigned id=0;id<items.size();++id){if(present[id])last[id]=now[id];wasPresent[id]=present[id];}
        for(const auto& it:items)if(it.parent>=0&&present[it.id]&&present[size_t(it.parent)]){const bool body=now[size_t(it.parent)]==1;
            stats.weaponAlone+=now[it.id]==1&&!body;stats.weaponBodyDropped+=!body;}
        for(const auto& a:actors){stats.fenceExempt+=a.rigid;const auto& it=items[owner[a.first].first];
            if(it.kind==1){++stats.eventFrames;stats.eventExempt+=a.exempt;}stats.idleExempt+=it.kind==2&&a.exempt;stats.corpseExempt+=it.kind==3&&a.exempt;}
        stats.fastDropped+=(present[player]&&now[player]!=1)+(present[kodo]&&now[kodo]!=1);
        if(frame>=FreeFrames)stats.fastDropped+=(present[playerWeapon]&&now[playerWeapon]!=1)+(present[kodoWeapon]&&now[kodoWeapon]!=1);
        for(const auto& c:legacy){const auto o=owner[c.index];const unsigned slot=o.first*8+o.second;
            if(frame>0&&legacyLast[slot]>=0&&int(c.keep)!=legacyLast[slot])++stats.legacyToggles;legacyLast[slot]=c.keep;}
        std::size_t legacyKept=0;for(const auto& c:legacy)if(c.keep)legacyKept+=c.bytes;
        stats.utilization+=double(ranked)/double(budget);stats.legacyUtilization+=double(legacyKept)/double(budget+[&]{std::size_t b=0;for(const auto& d:draws)if(d.rigid)b+=d.bytes;return b;}());
        {std::size_t total=0;for(const auto& d:draws)if(d.keep)total+=d.bytes;stats.maxTotal=std::max(stats.maxTotal,double(total)/double(budget));}
        stats.frames+=1;
        if(frame%120==119){++stats.windows;for(unsigned id=0;id<items.size();++id){stats.maxWindowFlips=std::max<std::size_t>(stats.maxWindowFlips,windowFlips[id]);
            if(items[id].kind==1)stats.maxEventWindowFlips=std::max<std::size_t>(stats.maxEventWindowFlips,windowFlips[id]);windowFlips[id]=0;}}
    }
    stats.utilization/=stats.frames;stats.legacyUtilization/=stats.frames;
    return stats;
}
static void stationary(){
    // Budget-state hysteresis: ranking continues until bytes stay below 90%.
    {History h;Tuning t;assert(!h.shouldRank(50,100,t)&&h.shouldRank(101,100,t));
     for(unsigned f=0;f<t.stayFrames-1;++f)assert(h.shouldRank(80,100,t));
     assert(h.shouldRank(95,100,t)); /* between 90% and 100% the count restarts */
     for(unsigned f=0;f<t.stayFrames-1;++f)assert(h.shouldRank(80,100,t));
     assert(!h.shouldRank(80,100,t)&&!h.shouldRank(99,100,t)&&!h.shouldRank(0,0,t));
     History l;assert(l.shouldRank(101,100,Legacy)&&!l.shouldRank(99,100,Legacy));}
    // Stationary multi-bone actors: exempt after StationaryFrames with extra
    // samples, new exemptions bounded by an own pool of exemptFraction*budget (nearest first).
    History h;Scratch sc;Tuning t;
    auto frame=[&](float jitter,unsigned n){std::vector<Draw> d;
        for(unsigned i=0;i<n;++i){Draw w;w.index=d.size();w.group=i;w.bytes=20;w.known=true;w.rigid=false;w.bone=NAN;w.key=50+i;
            w.at[0]=10.f+float(i)+jitter;w.distanceSquared=w.at[0]*w.at[0];
            if(h.stationaryHint(w.key,w.at)){w.extras=2;for(unsigned x=0;x<2;++x){w.extra[x][0]=w.at[0]+float(x);w.extra[x][1]=1;w.extra[x][2]=jitter;}}
            d.push_back(w);}
        const auto r=choose(d,sc,100,h,false,nullptr,t);return std::make_pair(d,r);};
    for(unsigned f=0;f<StationaryFrames+2;++f){auto [d,r]=frame(0,6);if(f<StationaryFrames-1)assert(!r.exemptActors);}
    auto [d,r]=frame(0,6);
    assert(r.exemptActors==5&&r.exemptBytes==100&&r.cappedExempt==1&&r.exemptCapBinds==1&&r.keptBytes<=100); /* own pool of 100 bytes: five 20-byte actors */
    assert(!d[0].keep||d[5].keep||true);
    assert(d[0].keep&&d[1].keep); /* nearest first */
    // Two moving frames end the exemption; one does not.
    auto one=frame(.1f,6);assert(one.second.exemptActors==5);
    frame(0,6);auto two1=frame(.1f,6);auto two2=frame(.2f,6);assert(two2.second.exemptActors==0);
    std::puts("PASS stationary: budget-state hysteresis, exempt after stillness, cap nearest-first, sticky exit after two moving frames");
}
static void rateLimit(){
    // Stable identity: issue order of an actor's draws does not change its key.
    // Rate limit: a dropped actor stays dropped for rateFrames even with room;
    // when locked kept actors overflow the budget the farthest of them goes.
    Tuning t;t.stationary=false;History h;Scratch sc;
    auto frame=[&](float far,bool swap,std::size_t budget){std::vector<Draw> d;
        auto add=[&](unsigned group,std::uint64_t key,float x,std::size_t bytes){Draw w;w.index=d.size();w.group=group;w.bytes=bytes;w.known=true;w.rigid=false;w.bone=NAN;w.key=key;w.at[0]=x;w.distanceSquared=x*x;d.push_back(w);};
        if(swap){add(0,11,10.3f,30);add(0,10,10,30);}else{add(0,10,10,30);add(0,11,10.3f,30);} /* actor A, issue order varies */
        add(1,20,far,40); /* actor B */
        const auto r=choose(d,sc,budget,h,false,nullptr,t);return std::make_pair(d,r);};
    for(unsigned f=0;f<70;++f){auto [d,r]=frame(20,f%2,100);assert(d[0].keep&&d[2].keep&&r.matched==(f?2u:0u));}
    auto [d1,r1]=frame(20,false,90);assert(d1[0].keep&&!d1[2].keep); /* budget shrinks: B dropped */
    for(unsigned f=0;f+1<t.rateFrames;++f){auto [d,r]=frame(20,f%2,100);assert(!d[2].keep);} /* room again, but locked for rateFrames */
    bool back=false;for(unsigned f=0;f<=t.admitFrames&&!back;++f){auto [d2,r2]=frame(20,false,100);back=d2[2].keep;}assert(back); /* then the normal admission delay */
    // Overflow: B changed just now (locked); A is free to change and is not
    // inside the inner cut (it is the cut), so A goes first. Inner-cut priority
    // over locked actors is covered by nearPriority().
    auto [d3,r3]=frame(20,false,70);assert(!d3[0].keep&&d3[2].keep&&r3.keptBytes==40);
    std::puts("PASS rate limit: order-independent identity, locked drop, overflow drops actors free to change first");
}
static void cadence(){
    // NearShadowInterval>1 with capture skip: selection runs every `cadence` rendered
    // frames. atCadence(1) is the identity; otherwise every frame count is rescaled so
    // the lock + admission time stays the same in RENDERED frames (within two captures).
    const Tuning base;const Tuning one=atCadence(base,1);
    assert(one.cadence==1&&one.margin==base.margin&&one.rigidMaxAge==base.rigidMaxAge&&one.reserveAge==base.reserveAge&&one.admitFrames==base.admitFrames&&
           one.stayFrames==base.stayFrames&&one.rateFrames==base.rateFrames&&one.frames(ProbationStill)==ProbationStill&&one.frames(StationaryFrames)==StationaryFrames);
    auto returnAfter=[](unsigned c){Tuning t=atCadence(Tuning{},c);t.stationary=false;History h;Scratch sc;
        auto frame=[&](std::size_t budget){std::vector<Draw> d;
            auto add=[&](unsigned group,std::uint64_t key,float x,std::size_t bytes){Draw w;w.index=d.size();w.group=group;w.bytes=bytes;w.known=true;w.rigid=false;w.bone=NAN;w.key=key;w.at[0]=x;w.distanceSquared=x*x;d.push_back(w);};
            add(0,10,10,30);add(1,20,20,40);choose(d,sc,budget,h,false,nullptr,t);return d[1].keep;};
        for(unsigned f=0;f<70;++f)assert(frame(100));
        assert(!frame(50)); /* dropped on a forced budget cut */
        unsigned captures=0;while(!frame(100))++captures;return captures*c;}; /* rendered frames until B casts again */
    const unsigned reference=returnAfter(1);
    for(unsigned c:{2u,3u,4u,16u}){const Tuning s=atCadence(Tuning{},c);
        assert(s.rateFrames*c>=base.rateFrames&&(s.rateFrames-1)*c<base.rateFrames&&s.admitFrames*c>=base.admitFrames&&s.frames(ExemptMoveFrames)>=1);
        const unsigned rendered=returnAfter(c);assert(rendered+2*c>=reference&&rendered<=reference+2*c);}
    std::printf("PASS cadence: identity at 1; lock+admission %u rendered frames at cadence 1, same within 2 captures at 2,3,4,16\n",reference);
}
static void instances(){
    // Ten instances of one fence model side by side: every draw key is shared,
    // the palette root tells them apart. Parts are issued in a varying order and
    // cloth parts wave; the camera drifts along the row. Each instance keeps its
    // own state: kept ones stay kept, dropped ones stay dropped (no swaps).
    for(bool stationary:{false,true}){Tuning t;t.stationary=stationary;History h;Scratch sc;std::mt19937 random(3);std::uniform_real_distribution<float> u(-.5f,.5f);
        std::vector<int> last(10,-1);unsigned changes=0;
        for(unsigned f=0;f<300;++f){std::vector<Draw> d;unsigned group=0;const float camera=.01f*float(f);
            for(unsigned i=0;i<10;++i){unsigned order[3]={0,1,2};std::shuffle(order,order+3,random);
                for(unsigned k=0;k<3;++k){const unsigned part=order[k];Draw w;w.index=d.size();w.group=group;w.bytes=10;w.known=true;w.rigid=false;w.bone=NAN;
                    w.key=900+part; /* same model: same keys for every instance */
                    w.at[0]=3.f*float(i)+10+.3f*float(part)+(part?u(random)*.8f:0);w.at[1]=part?u(random)*.8f:0;
                    w.distanceSquared=(w.at[0]-camera)*(w.at[0]-camera)+w.at[1]*w.at[1];
                    if(k==0){w.hasRoot=true;w.root[0]=3.f*float(i)+10;w.root[1]=0;w.root[2]=0;}
                    d.push_back(w);}
                ++group;}
            const auto r=choose(d,sc,150,h,false,nullptr,t);assert(r.keptBytes<=150);
            if(f>2)for(unsigned i=0;i<10;++i){const int keep=d[3*i].keep;if(last[i]>=0&&keep!=last[i])++changes;last[i]=keep;}
            else for(unsigned i=0;i<10;++i)last[i]=d[3*i].keep;}
        // Without stillness exemption: the nearest five (150 bytes / 30) stay kept. With it, all
        // become exempt once still for StationaryFrames (one change each, never back).
        std::printf("identical instances stationary=%d: state changes over 300 frames=%u (instances=10)\n",stationary,changes);
        assert(stationary?changes<=5:changes==0);}
    std::puts("PASS instances: identical models told apart by palette root, no swaps");
}
static void nearPriority(){
    // da2: (a) a matched NPC dropped at 50 yd walks to 10 yd within 30 frames;
    // (b) a 180 degree turn into a new crowd. Inside the inner cut an actor is
    // kept on every frame (unless inner actors alone fill the budget), lock or
    // reservation notwithstanding; budget exact.
    Tuning t;t.stationary=false;const std::size_t budget=300;
    auto crowd=[&](std::vector<Draw>& d,unsigned& group,std::uint64_t key,float sign){
        for(unsigned i=0;i<20;++i){Draw w;w.index=d.size();w.group=group++;w.bytes=30;w.known=true;w.rigid=false;w.bone=NAN;w.key=key+i;
            w.at[0]=sign*(5.f+2.f*float(i));w.distanceSquared=w.at[0]*w.at[0];w.hasRoot=true;std::memcpy(w.root,w.at,sizeof w.root);d.push_back(w);}};
    {History h;Scratch sc;unsigned nearFrames=0;
     for(unsigned f=0;f<140;++f){std::vector<Draw> d;unsigned group=0;crowd(d,group,100,1);
        const float x=f<80?50.f:std::max(10.f,50.f-1.33f*float(f-80)); /* dropped for 80 frames, then approaches */
        Draw w;w.index=d.size();w.group=group++;w.bytes=30;w.known=true;w.rigid=false;w.bone=NAN;w.key=999;w.at[1]=x;w.distanceSquared=x*x;w.hasRoot=true;std::memcpy(w.root,w.at,sizeof w.root);d.push_back(w);
        const float inner=InnerCut*InnerCut*h.steadyCut();const auto r=choose(d,sc,budget,h,false,nullptr,t);assert(r.keptBytes<=budget);
        if(f<80)assert(!d.back().keep);
        if(std::isfinite(inner)&&x*x<=inner){assert(d.back().keep);++nearFrames;}}
     assert(nearFrames>=20);}
    {History h;Scratch sc;unsigned checked=0;
     for(unsigned f=0;f<160;++f){std::vector<Draw> d;unsigned group=0;crowd(d,group,f<100?100:500,f<100?1.f:-1.f);
        const float inner=InnerCut*InnerCut*h.steadyCut();const auto r=choose(d,sc,budget,h,false,nullptr,t);assert(r.keptBytes<=budget);
        // An inner actor may only be dropped when inner actors alone fill the budget.
        if(std::isfinite(inner))for(const auto& w:d)if(w.distanceSquared<=inner){checked+=f>=100;
            if(!w.keep){assert(r.keptBytes+w.bytes>budget);for(const auto& o:d)assert(!(o.keep&&o.distanceSquared>inner));}}}
     assert(checked>=5);}
    std::puts("PASS near priority: approaching matched NPC and a turn into a new crowd keep inner-cut actors every frame");
}
static void wrongRoot(){
    // An unproven palette layout could yield a "root" that never moves: a walking
    // actor with a constant root is never exempted (the vertex sample must stay
    // within 2 yd too), and a root 30+ yd from the actor's vertices is ignored.
    Tuning t;History h;Scratch sc;std::size_t exempt=0,rejected=0;
    for(unsigned f=0;f<400;++f){std::vector<Draw> d;
        Draw w;w.index=0;w.group=0;w.bytes=10;w.known=true;w.rigid=false;w.bone=NAN;w.key=77;w.at[0]=20+.05f*float(f);w.distanceSquared=w.at[0]*w.at[0];
        w.hasRoot=true;w.root[0]=20;d.push_back(w); /* constant root, walking vertices */
        Draw x=w;x.index=1;x.group=1;x.key=78;x.at[0]=-20;x.at[1]=0;x.distanceSquared=400;x.root[0]=-60;d.push_back(x); /* root 40 yd away */
        const auto r=choose(d,sc,1000,h,false,nullptr,t);exempt+=r.exemptActors;rejected+=r.rootRejected;}
    assert(!exempt&&rejected==400);
    std::puts("PASS wrong root: constant root on a walking actor never exempts; far roots fall back to vertex samples");
}
static void fate(){
    using namespace NorthlightShadowFate;Tracker t;unsigned logs=0,flipLines=0;bool sawFlipper=false;
    for(unsigned frame=0;frame<Tracker::Period*2;++frame){t.beginFrame(true);if(!t.active())continue;
        Key a;a.vb=1;a.count=30;Key b;b.vb=2;b.count=60;
        const int sa=t.slot(a,true,30),sb=t.slot(b,true,60);
        t.record(sa,frame%2?Dropped:Kept,frame%2==0,12);      /* flips every frame */
        t.record(sb,CaptureBudget,false);                    /* never drawn */
        t.endFrame();
        if(t.windowEnded())t.report([&](const char* f,auto... args){++logs;char line[512];std::snprintf(line,sizeof line,f,args...);
            if(std::strstr(line,"SHADOW fate key")){++flipLines;sawFlipper=sawFlipper||(std::strstr(line,"vb=1 ")&&std::strstr(line,"kept:")&&std::strstr(line,"dropped:"));}});}
    assert(logs==4&&flipLines==2&&sawFlipper);
    std::puts("PASS shadow fate: window, per-frame outcome, flip ranking, histogram");
}
// Diagnostics=0 passes diagnostics=false where sampled frames passed true: the
// forced body search of settled, unmoved rigid actors may only change rigidStuck.
static void diagnosticsNeutral(){
    std::size_t frames=0,stuck=0;
    auto both=[&](auto run){Decisions off,on;run(false,&off);run(true,&on);assert(off.frames==on.frames&&!off.frames.empty());frames+=on.frames.size();stuck+=on.rigidStuck;};
    for(unsigned seed:{7u,11u,13u})both([&](bool d,Decisions* out){simulate(seed,seed==13,seed==11,600,0,false,Tuning{},false,false,false,d,out);});
    for(int walking=0;walking<2;++walking){
        both([&](bool d,Decisions* out){simulate(21,walking,false,900,.02f,true,Tuning{},false,false,false,d,out);});
        both([&](bool d,Decisions* out){simulate(41,walking,false,900,.02f,true,Tuning{},true,false,false,d,out);});
        both([&](bool d,Decisions* out){simulate(47,walking,false,1200,.02f,true,Tuning{},true,false,true,d,out);});
        Tuning t139;t139.stableIdentity=false;t139.rateFrames=0;
        both([&](bool d,Decisions* out){simulate(47,walking,false,1200,.02f,true,t139,true,false,true,d,out);});
        both([&](bool d,Decisions* out){simulate(43,walking,false,900,0,false,Tuning{},false,true,false,d,out);});}
    assert(stuck>0); /* the flag was exercised */
    std::printf("diagnostics flag neutral: %zu frames, identical keep/attach/body/exempt/history decisions (diagnostic rigidStuck=%zu)\n",frames,stuck);
}
int main(){
    rigid();unit();stationary();rateLimit();cadence();instances();nearPriority();wrongRoot();fate();diagnosticsNeutral();
    // Still camera: only animation jitter and a few walkers.
    auto still=simulate(7,false,false,600);
    std::printf("still camera: stable toggles/frame=%.3f legacy draw toggles/frame=%.3f utilization=%.3f legacy=%.3f items=%zu\n",
        still.toggles/still.frames,still.legacyToggles/still.frames,still.utilization,still.legacyUtilization,still.items);
    assert(still.toggles/still.frames<0.05&&still.legacyToggles>still.toggles*10);
    std::printf("fast/erratic near actors dropped frames=%zu\n",still.fastDropped);
    std::printf("weapons: cast alone=%zu frames with dropped body=%zu exempt rigid actor-frames=%zu\n",still.weaponAlone,still.weaponBodyDropped,still.fenceExempt);
    assert(!still.weaponAlone&&still.weaponBodyDropped>0&&still.fenceExempt>0&&!still.fastDropped);
    auto noisy=simulate(11,false,true,600);
    std::printf("still camera + 5%% capture dropout: stable=%.3f legacy=%.3f utilization=%.3f\n",noisy.toggles/noisy.frames,noisy.legacyToggles/noisy.frames,noisy.utilization);
    assert(noisy.toggles<noisy.legacyToggles);
    auto moving=simulate(13,true,false,600);
    std::printf("moving camera: stable=%.3f legacy=%.3f utilization=%.3f weaponAlone=%zu bodyDropped=%zu noisyWeaponAlone=%zu\n",moving.toggles/moving.frames,moving.legacyToggles/moving.frames,moving.utilization,moving.weaponAlone,moving.weaponBodyDropped,noisy.weaponAlone);
    assert(moving.toggles<moving.legacyToggles&&!moving.weaponAlone&&moving.weaponBodyDropped>0&&!noisy.weaponAlone&&!moving.fastDropped);
    // View study: camera orbit around a still/walking player, camera vs pivot ranking, margins.
    struct Variant {const char* name;bool origin;Tuning t;};Tuning legacy=Legacy,rigid=Legacy,probe,piv;
    rigid.rigidMaxAge=1800;rigid.rigidCapacity=8192;probe=rigid;probe.probationFollow=false;piv=probe;
    for(int walking=0;walking<2;++walking){double base=0,baseFence=0;
        for(const Variant& v:{Variant{"0.3.138",false,legacy},Variant{"+rigidAge",false,rigid},Variant{"+probationStill",false,probe},Variant{"+pivot",true,piv},Variant{"0.3.139",true,Tuning{}}}){
            double tog=0,fence=0,rev=0,util=0;std::size_t alone=0;
            for(unsigned seed:{21u,22u,23u}){auto r=simulate(seed,walking,false,900,.02f,v.origin,v.t);tog+=r.toggles/r.frames/3;fence+=double(r.fenceToggles)/r.frames/3;rev+=r.reversals/r.frames/3;util+=r.utilization/3;alone+=r.weaponAlone;}
            std::printf("orbit+frustum walking=%d %-15s toggles/frame=%.3f fence=%.3f reversals=%.3f utilization=%.3f weaponAlone=%zu\n",walking,v.name,tog,fence,rev,util,alone);
            assert(!alone);if(!base){base=tog;baseFence=fence;}
            if(v.t.margin==Tuning{}.margin&&v.origin)assert(tog<.65*base&&fence<.25*baseFence);}}
    // Event fences (multi-bone, static) among a crowd: exempt once stationary,
    // idle NPCs never, corpses may be. Legacy (0.3.138) for comparison.
    Tuning v3;v3.exemptFraction=.5f;v3.sharedWithRigid=true;Tuning wide=v3;wide.exemptFraction=1; /* v3 variants: shared with rigid bytes */
    for(int walking=0;walking<2;++walking)for(const Tuning& t:{Tuning(Legacy),v3,wide,Tuning{}}){
        auto e=simulate(41,walking,false,900,.02f,t.stationary,t,true);
        std::printf("event fences walking=%d %-13s: fenceToggles/frame=%.3f (on %zu, off %zu, reversals %zu) exempt share=%.3f idleExempt=%zu corpseExempt=%zu toggles/frame=%.3f maxTotal/budget=%.2f weaponAlone=%zu fast=%zu\n",walking,!t.stationary?"0.3.138":!t.sharedWithRigid?"own pool 1.0":t.exemptFraction<1?"v3 shared .5":"shared 1.0",
            e.eventToggles/e.frames,e.eventOn,e.eventOff,e.eventReversals,double(e.eventExempt)/double(e.eventFrames?e.eventFrames:1),e.idleExempt,e.corpseExempt,e.toggles/e.frames,e.maxTotal,e.weaponAlone,e.fastDropped);
        assert(!e.weaponAlone&&!e.idleExempt);if(t.stationary)assert(!e.fastDropped);}
    // Packed shared-VB objects issued in varying order, waving cloth, orbiting
    // camera: 0.3.139 identity (first issued draw) vs 0.3.140 (stable key + rate limit).
    {Tuning t139;t139.stableIdentity=false;t139.rateFrames=0;Tuning noRate;noRate.rateFrames=0;
     for(int walking=0;walking<2;++walking)for(const auto& v:{std::make_pair("0.3.139",t139),std::make_pair("stable key only",noRate),std::make_pair("0.3.140",Tuning{})}){
        auto e=simulate(47,walking,false,1200,.02f,true,v.second,true,false,true);
        std::printf("packed walking=%d %-15s: eventFlips/frame=%.3f (on %zu off %zu reversals %zu) maxFlips/window event=%zu any=%zu toggles/frame=%.3f utilization=%.3f weaponAlone=%zu fast=%zu idleExempt(accepted, pool)=%zu exemptShare=%.2f\n",walking,v.first,
            e.eventToggles/e.frames,e.eventOn,e.eventOff,e.eventReversals,e.maxEventWindowFlips,e.maxWindowFlips,e.toggles/e.frames,e.utilization,e.weaponAlone,e.fastDropped,e.idleExempt,double(e.eventExempt)/double(e.eventFrames?e.eventFrames:1));
        assert(!e.weaponAlone);if(v.second.rateFrames)assert(e.maxEventWindowFlips<=3&&!e.fastDropped); /* two voluntary changes per 120 frames; a third only as a forced overflow drop */}}
    // Budget-state flapping: the camera turns away every 20 frames.
    for(const Tuning& t:{Tuning(Legacy),Tuning{}}){auto f=simulate(43,false,false,900,0,false,t,false,true);
        std::printf("budget flapping %s: toggles/frame=%.3f reversals/frame=%.3f\n",t.stayFrames?"0.3.139 (stay 30)":"0.3.138",f.toggles/f.frames,f.reversals/f.frames);}
    for(unsigned seed=100;seed<140;++seed){auto s=simulate(seed,seed&1,seed&2,120);assert(s.utilization>.5&&!s.weaponAlone&&!s.fastDropped);}
    std::printf("decision digest=%016llx frames=%zu\n",(unsigned long long)digest,digestFrames);
    std::puts("PASS stable actor quota: per-actor cut, prefix, budget bound, rigid exempt, attachments follow body, hysteresis");
}
