#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

// Stable quality policy for the optional actor shadow quota (budget>0 only).
// Draws of one constant group are one actor: they cast or drop together.
// Rigid actors (every draw bound to the same single palette bone) are either
// free-standing (doodads, fences: cast unconditionally and never consume the
// quota, exactly as with budget 0) or attachments (weapons, shoulders, held
// items: follow their body's decision, so they never cast alone). Rigid bytes
// of either class stay outside the quota; charging attachments to their body
// would make every capture gap of a weapon move the cut.
// The class is sticky per rigid identity. During the first FreeFrames frames
// (probation) a rigid actor within AttachRadius of a non-rigid sampled vertex
// follows that body, and becomes a confirmed attachment once it moves more than
// CoMoveTolerance from where it was first seen (it moves with the body); one
// that stays still settles as free-standing, so a fence beside a guard keeps
// casting when the guard walks away or is dropped. A confirmed attachment keeps
// last frame's body (else the nearest) within KeepReach*AttachRadius, holds its
// last decision while no body is in reach and frees only after FreeFrames such
// frames. A settled free-standing actor re-attaches only if it moves more than
// MoveTolerance while a body is within AttachRadius, so a patrol passing a fence
// never captures it. An attachment casts only if its body and every other body
// within AttachRadius cast. A rigid actor without a sampled position is ranked
// like any other actor.
// Remaining actors are ranked by nearest sampled vertex; actors kept last frame
// rank with distance scaled by (1-margin). Walking that ranking, the first actor
// that does not fit closes the cut: everything after it drops (monotonic
// prefix). An actor not kept last frame must fit on AdmitFrames consecutive
// frames before it is admitted, unless it is unlocated (first claim, as in
// 0.3.137) or within InnerCut of last frame's cut distance (no identity needed
// there, so fast or erratic near actors are never starved). While waiting it is
// dropped and reserves nothing, so a one-frame capture gap elsewhere cannot
// flash it on. Actors kept recently but missing from this frame's capture keep
// their bytes reserved for up to MaxAge frames; an unmatched actor releases the
// nearest such reservation of its shape. Identity: first draw's shader/
// declaration/mesh shape plus a sampled world position within a tolerance.
namespace NorthlightActorShadowSelection {
constexpr bool Enabled=true;
constexpr float InnerCut=.8f,ClaimRadius=8.f,CoMoveTolerance=.05f,HysteresisMargin=.10f,IdentityTolerance=1.5f,AttachRadius=4.f,KeepReach=1.5f,MoveTolerance=.25f;
constexpr unsigned AdmitFrames=3,MaxAge=8,FreeFrames=8;
struct Draw {
    std::size_t index=0,bytes=0;unsigned group=0;
    float distanceSquared=0,at[3]={};bool known=false,rigid=false;float bone=0;
    std::uint64_t key=0;bool keep=true;
};
struct Actor {
    std::size_t first=0,count=0,bytes=0,body=SIZE_MAX;float distanceSquared=0,rank=0,at[3]={},anchor[3]={},bodyAt[3]={};
    std::uint64_t key=0,bodyKey=0;unsigned pending=0,free=0;
    bool known=false,rigid=false,located=false,previous=false,matched=false,waiting=false,keep=true;
    bool rigidTracked=false,attachment=false,settled=false,orphan=false;
};
struct Point {float x=0,y=0,z=0;std::size_t actor=0;};
struct Scratch {std::vector<Actor> actors;std::vector<std::size_t> order;std::vector<Point> points;std::vector<std::pair<std::size_t,std::size_t>> links;};
struct Result {
    std::size_t kept=0,dropped=0,keptBytes=0,droppedBytes=0,unknown=0;
    std::size_t actors=0,actorsKept=0,rigidActors=0,rigidDraws=0,rigidBytes=0,toggles=0,matched=0,retained=0,admitted=0,waiting=0,reserved=0;
    std::size_t attached=0,attachedBytes=0,orphans=0,rigidStuck=0;
};
namespace detail {
template<class Entry> Entry* nearest(std::vector<Entry>& entries,std::uint64_t key,const float* at){
    auto it=std::lower_bound(entries.begin(),entries.end(),key,[](const Entry& e,std::uint64_t k){return e.key<k;});
    Entry* best=nullptr;float bestSquared=IdentityTolerance*IdentityTolerance;
    for(;it!=entries.end()&&it->key==key;++it){if(it->used)continue;float d=0;
        for(unsigned a=0;a<3;++a){const float x=it->at[a]-at[a];d+=x*x;}
        if(d<=bestSquared){bestSquared=d;best=&*it;}}
    return best;
}
}
class History {
    struct Entry {std::uint64_t key=0;std::size_t bytes=0;float at[3]={};unsigned pending=0,age=0;bool kept=false,used=false;};
    struct Rigid {std::uint64_t key=0,bodyKey=0;float at[3]={},anchor[3]={},bodyAt[3]={};unsigned free=0,age=0;bool attachment=false,settled=false,kept=false,used=false;};
    std::vector<Entry> entries_,next_;std::vector<Rigid> rigid_,nextRigid_;bool allKept_=false;float lastCut_=INFINITY;
public:
    // Frames within budget keep everything; no ranking identities are needed
    // then. Rigid classes survive them: they do not depend on the budget.
    void keptAll(){entries_.clear();allKept_=true;lastCut_=INFINITY;}
    void clear(){entries_.clear();rigid_.clear();allKept_=false;lastCut_=INFINITY;}
    // Distance squared of the farthest kept actor when the last ranked frame dropped any.
    float lastCut()const{return lastCut_;}
    // An unmatched actor releases the nearest stale reservation of its shape
    // within ClaimRadius (a fast mover would otherwise reserve its bytes again
    // every frame); same-model actors farther away keep theirs.
    void claim(std::uint64_t key,const float* at){
        auto it=std::lower_bound(entries_.begin(),entries_.end(),key,[](const Entry& e,std::uint64_t k){return e.key<k;});
        Entry* best=nullptr;float bestSquared=ClaimRadius*ClaimRadius;
        for(;it!=entries_.end()&&it->key==key;++it){if(it->used)continue;float d=0;
            for(unsigned a=0;a<3;++a){const float x=it->at[a]-at[a];d+=x*x;}
            if(d<=bestSquared){bestSquared=d;best=&*it;}}
        if(best)best->used=true;}
    // No usable history (first ranked frame, after reset or a within-budget
    // frame): every actor counts as kept last frame.
    bool allKept()const{return allKept_||entries_.empty();}
    std::size_t size()const{return entries_.size()+rigid_.size();}
    Entry* find(std::uint64_t key,const float* at){return detail::nearest(entries_,key,at);}
    Rigid* findRigid(std::uint64_t key,const float* at){return detail::nearest(rigid_,key,at);}
    // Bytes of actors kept recently but absent from this frame's capture.
    std::size_t reserved()const{std::size_t n=0;for(const auto& e:entries_)if(!e.used&&e.kept&&e.age<MaxAge)n+=e.bytes;return n;}
    void publish(const std::vector<Actor>& actors,float cut){
        next_.clear();nextRigid_.clear();lastCut_=cut;
        for(const auto& a:actors){
            if(a.rigidTracked){Rigid r;r.key=a.key;std::memcpy(r.at,a.at,sizeof r.at);std::memcpy(r.anchor,a.anchor,sizeof r.anchor);r.free=a.free;r.attachment=a.attachment;r.settled=a.settled;r.kept=a.keep;r.bodyKey=a.bodyKey;std::memcpy(r.bodyAt,a.bodyAt,sizeof r.bodyAt);nextRigid_.push_back(r);}
            else if(!a.rigid&&a.located){Entry e;e.key=a.key;e.bytes=a.bytes;std::memcpy(e.at,a.at,sizeof e.at);e.kept=a.keep;e.pending=a.pending;next_.push_back(e);}}
        if(!allKept_)for(const auto& e:entries_)if(!e.used&&e.age<MaxAge){next_.push_back(e);++next_.back().age;}
        for(const auto& r:rigid_)if(!r.used&&r.age<MaxAge){nextRigid_.push_back(r);++nextRigid_.back().age;}
        std::sort(next_.begin(),next_.end(),[](const Entry& a,const Entry& b){return a.key<b.key;});
        std::sort(nextRigid_.begin(),nextRigid_.end(),[](const Rigid& a,const Rigid& b){return a.key<b.key;});
        entries_.swap(next_);rigid_.swap(nextRigid_);allKept_=false;
    }
};
// draws: capture order. Writes draws[i].keep.
inline Result choose(std::vector<Draw>& draws,Scratch& scratch,std::size_t budget,History& history){
    auto& actors=scratch.actors;auto& order=scratch.order;auto& points=scratch.points;auto& links=scratch.links;
    Result result;actors.clear();order.clear();points.clear();links.clear();
    for(std::size_t i=0;i<draws.size();){
        Actor a;a.first=i;a.key=draws[i].key;a.rigid=true;const float bone=draws[i].bone;
        std::size_t j=i;
        for(;j<draws.size()&&draws[j].group==draws[i].group;++j){const auto& d=draws[j];a.bytes+=d.bytes;
            a.rigid=a.rigid&&d.rigid&&d.bone==bone;
            const bool known=d.known&&std::isfinite(d.distanceSquared)&&d.distanceSquared>=0;
            if(known&&(!a.known||d.distanceSquared<a.distanceSquared))a.distanceSquared=d.distanceSquared;
            if(known&&!a.located){a.located=true;std::memcpy(a.at,d.at,sizeof a.at);}
            a.known=a.known||known;}
        a.count=j-i;actors.push_back(a);i=j;
    }
    // Unlocated rigid actors cannot be classified: rank them.
    for(auto& a:actors)if(a.rigid&&!a.located)a.rigid=false;
    // Every sampled vertex of every non-rigid actor, sorted by x, for the reach query.
    for(std::size_t n=0;n<actors.size();++n){const auto& a=actors[n];if(a.rigid)continue;
        for(std::size_t i=a.first;i<a.first+a.count;++i){const auto& d=draws[i];
            if(d.known&&std::isfinite(d.distanceSquared)&&d.distanceSquared>=0)points.push_back({d.at[0],d.at[1],d.at[2],n});}}
    std::sort(points.begin(),points.end(),[](const Point& a,const Point& b){return a.x<b.x||(a.x==b.x&&a.actor<b.actor);});
    const float reach=AttachRadius*KeepReach;
    for(auto& a:actors){if(!a.rigid)continue;float best=reach*reach;std::size_t inReach=SIZE_MAX,same=SIZE_MAX;
        auto* e=history.findRigid(a.key,a.at);const std::size_t linked=links.size();
        // Nearest body over all draws of the rigid actor; ties go to the earlier
        // actor. An attachment stays with last frame's body while it is in reach.
        for(std::size_t i=a.first;i<a.first+a.count;++i){const auto& d=draws[i];
            if(!(d.known&&std::isfinite(d.distanceSquared)&&d.distanceSquared>=0))continue;
            auto it=std::lower_bound(points.begin(),points.end(),d.at[0]-reach,[](const Point& p,float x){return p.x<x;});
            for(;it!=points.end()&&it->x<=d.at[0]+reach;++it){const float x=it->x-d.at[0],y=it->y-d.at[1],z=it->z-d.at[2],q=x*x+y*y+z*z;
                if(q>reach*reach)continue;
                if(q<=AttachRadius*AttachRadius&&(links.size()==linked||links.back().second!=it->actor))links.push_back({std::size_t(&a-actors.data()),it->actor});
                if(e&&e->attachment&&same==SIZE_MAX&&actors[it->actor].key==e->bodyKey&&actors[it->actor].located){float b=0;
                    for(unsigned k=0;k<3;++k){const float t=actors[it->actor].at[k]-e->bodyAt[k];b+=t*t;}
                    if(b<=IdentityTolerance*IdentityTolerance)same=it->actor;}
                if(q<best||(q==best&&inReach!=SIZE_MAX&&it->actor<inReach)){best=q;inReach=it->actor;}}}
        a.rigidTracked=true;std::memcpy(a.anchor,a.at,sizeof a.anchor);
        const std::size_t tight=best<=AttachRadius*AttachRadius?inReach:SIZE_MAX; /* new attachments need the tight radius */
        auto moved=[&](const float* anchor,float limit){float m=0;for(unsigned k=0;k<3;++k){const float x=a.at[k]-anchor[k];m+=x*x;}return m>limit*limit;};
        bool follow=false;if(e)e->used=true;
        if(e&&e->attachment){ /* confirmed: keep the body; free only after FreeFrames without one */
            if(same!=SIZE_MAX)inReach=same;a.previous=e->kept;a.free=inReach!=SIZE_MAX?0:e->free+1;
            a.attachment=a.free<FreeFrames;follow=a.attachment;if(!a.attachment)a.settled=true;}
        else if(e&&e->settled){ /* free-standing: only real movement beside a body re-attaches */
            a.settled=true;std::memcpy(a.anchor,e->anchor,sizeof a.anchor);
            if(!moved(e->anchor,MoveTolerance))result.rigidStuck+=inReach!=SIZE_MAX;
            else if(tight!=SIZE_MAX){a.settled=false;a.attachment=follow=true;inReach=tight;}
            else std::memcpy(a.anchor,a.at,sizeof a.anchor);}
        else{ /* probation: attach on co-movement beside a body; follow a body meanwhile */
            if(e)std::memcpy(a.anchor,e->anchor,sizeof a.anchor);a.free=e?e->free+1:1;inReach=tight;
            if(tight!=SIZE_MAX){follow=true;a.previous=e&&e->kept;a.attachment=e&&moved(e->anchor,CoMoveTolerance);}
            if(!a.attachment&&a.free>=FreeFrames)a.settled=true;
            if(a.attachment)a.free=0;}
        if(!follow){links.resize(linked);continue;}
        a.rigid=false;
        if(inReach!=SIZE_MAX){a.body=inReach;a.bodyKey=actors[inReach].key;std::memcpy(a.bodyAt,actors[inReach].at,sizeof a.bodyAt);++result.attached;result.attachedBytes+=a.bytes;}
        else{a.orphan=true;++result.orphans;}}
    const bool allKept=history.allKept();const float scale=(1-HysteresisMargin)*(1-HysteresisMargin);
    for(std::size_t n=0;n<actors.size();++n){auto& a=actors[n];
        if(a.rigid){++result.rigidActors;result.rigidDraws+=a.count;result.rigidBytes+=a.bytes;continue;}
        if(a.body!=SIZE_MAX||a.orphan)continue;
        ++result.actors;result.unknown+=!a.known;
        if(allKept)a.previous=true;
        else if(a.located)if(auto* e=history.find(a.key,a.at)){e->used=true;a.matched=true;a.previous=e->kept;a.pending=e->pending;++result.matched;}
        a.rank=a.previous?a.distanceSquared*scale:a.distanceSquared;order.push_back(n);
    }
    if(!allKept)for(auto n:order)if(!actors[n].matched&&actors[n].located)history.claim(actors[n].key,actors[n].at);
    // Admission delay applies only near the cut: unlocated actors keep their first
    // claim and actors well inside the previous cut need no identity at all.
    const float inner=InnerCut*InnerCut*history.lastCut();
    std::sort(order.begin(),order.end(),[&](std::size_t x,std::size_t y){const auto& a=actors[x];const auto& b=actors[y];
        if(a.known!=b.known)return !a.known;
        if(a.known&&a.rank!=b.rank)return a.rank<b.rank;
        return a.first<b.first;});
    std::size_t used=allKept?0:std::min(budget,history.reserved());bool open=true;result.reserved=used;
    for(auto n:order){auto& a=actors[n];
        const bool fits=open&&a.bytes<=budget-used;
        if(!fits){a.keep=false;a.pending=0;open=false;}
        else if(!a.previous&&a.located&&!(a.distanceSquared<=inner)&&a.pending+1<AdmitFrames){a.keep=false;a.waiting=true;++a.pending;++result.waiting;}
        else{a.keep=true;a.pending=0;used+=a.bytes;result.admitted+=!a.previous;}
        if(a.keep){++result.actorsKept;result.retained+=a.previous;}
        result.toggles+=(allKept||a.matched)&&a.keep!=a.previous;
    }
    for(auto& a:actors){if(a.body!=SIZE_MAX)a.keep=actors[a.body].keep;if(a.orphan)a.keep=a.previous;}
    // Two bodies in reach (a crowd): an attachment casts only if every one of them does.
    for(const auto& l:links)if(actors[l.first].body!=SIZE_MAX&&!actors[l.second].keep)actors[l.first].keep=false;
    for(const auto& a:actors)for(std::size_t i=a.first;i<a.first+a.count;++i){auto& d=draws[i];d.keep=a.keep;
        if(a.rigid)continue;
        if(a.orphan||a.body!=SIZE_MAX){if(a.keep){++result.rigidDraws;result.rigidBytes+=d.bytes;}continue;} /* rigid bytes stay outside the quota */
        if(a.keep){++result.kept;result.keptBytes+=d.bytes;}else{++result.dropped;result.droppedBytes+=d.bytes;}}
    float cut=0;bool cutting=false;
    for(auto n:order){const auto& a=actors[n];cutting=cutting||!a.keep;if(a.keep&&a.known)cut=std::max(cut,a.distanceSquared);}
    history.publish(actors,cutting?cut:INFINITY);
    return result;
}
// One palette bone for every vertex, or NaN. Weighted lanes must agree and sum
// to one; without weights all four index lanes must agree. read(v,index,weight)
// decodes vertex v and fails closed on anything unreadable.
template<class Read> float rigidBone(std::size_t vertices,bool weighted,Read read){
    float bone=NAN;
    for(std::size_t v=0;v<vertices;++v){float index[4],weight[4]={1,1,1,1},sum=0;
        if(!read(v,index,weight))return NAN;
        for(unsigned lane=0;lane<4;++lane){
            if(weighted){if(!std::isfinite(weight[lane])||weight[lane]<0)return NAN;if(weight[lane]==0)continue;sum+=weight[lane];}
            if(!std::isfinite(index[lane])||(!std::isnan(bone)&&index[lane]!=bone))return NAN;bone=index[lane];}
        if(weighted&&std::fabs(sum-1)>.02f)return NAN;
    }
    return bone;
}
}
