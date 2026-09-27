#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <iterator>
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
constexpr unsigned AdmitFrames=3,MaxAge=8,FreeFrames=8,ProbationStill=3,StationaryFrames=120,ExemptMoveFrames=2,Samples=12;
constexpr float StationaryTolerance=.02f,RootReach=30.f,LooseTolerance=2.f;
// Tunables that changed after 0.3.138; Legacy reproduces 0.3.138 decisions.
// Rigid classes are position-keyed and stationary, so they are kept while the
// camera looks away (frustum exits otherwise restarted probation, darkening a
// fence beside a dropped NPC for FreeFrames on every return).
// Margin .20 and a 10-frame admission beyond the inner cut halve reversals in
// the orbit/walk simulation; a new rigid actor follows a body only until it has
// been still for ProbationStill frames (a fence entering view near a dropped
// NPC no longer goes dark for the whole probation).
// stationary: a non-rigid actor (multi-bone doodad, e.g. an event GameObject)
// whose world samples stay within StationaryTolerance of their anchors for
// StationaryFrames is exempt like a free-standing rigid doodad, up to
// exemptFraction*budget of stationary exempt bytes in their own pool (rigid
// bytes are not charged unless sharedWithRigid; nearest first); it leaves the state
// only after ExemptMoveFrames consecutive moving frames. stayFrames/stay: keep
// ranking (and identities) until actor bytes stay below stay*budget that long.
// A still actor that is not (yet) exempt is frozen: kept ones rank with the
// margin applied twice, dropped ones need room for three admission periods
// (or the inner cut), so a moving cut rarely flips them before they are exempt.
struct Tuning {float margin=.20f;unsigned rigidMaxAge=1800;std::size_t rigidCapacity=8192;unsigned reserveAge=30,admitFrames=10;bool probationFollow=false;
    bool stationary=true;float exemptFraction=1.f;unsigned stayFrames=30;float stay=.9f;
    float settledMove=CoMoveTolerance; /* a weapon that settled while its body was off-screen re-attaches on the body's first steps */
    bool sharedWithRigid=false; /* v3 variant: rigid bytes charged to the stationary cap */
    // stableIdentity: an actor is identified by the smallest stable per-draw key
    // of its draws (snapshot-cache entry = VB/IB identity + range, shader, decl),
    // not by whichever draw the game happened to issue first; stationary samples
    // are ordered by that key too. rateFrames: an actor's kept/dropped state
    // changes at most once per rateFrames unless the budget forces a drop.
    bool stableIdentity=true;unsigned rateFrames=60;
    // cadence: rendered frames per selection (NearShadowInterval when model capture is
    // skipped between captures). Every frame count above is in RENDERED frames: atCadence()
    // divides the Tuning counts, frames() the constants below, rounding up. 1 = unchanged.
    unsigned cadence=1;
    constexpr unsigned frames(unsigned n)const{return cadence>1?(n+cadence-1)/cadence:n;}};
// FlickerFixes=false keeps only the (decision-identical) cost work of 0.3.139.
constexpr bool FlickerFixes=true;
constexpr Tuning Legacy{.10f,MaxAge,SIZE_MAX,MaxAge,AdmitFrames,true,false,0,0,1,MoveTolerance,false,false,0};
inline Tuning active(){return FlickerFixes?Tuning{}:Legacy;}
inline Tuning atCadence(Tuning t,unsigned cadence){if(cadence<=1)return t;t.cadence=cadence;
    t.rigidMaxAge=t.frames(t.rigidMaxAge);t.reserveAge=t.frames(t.reserveAge);t.admitFrames=t.frames(t.admitFrames);t.stayFrames=t.frames(t.stayFrames);t.rateFrames=t.frames(t.rateFrames);return t;}
struct Draw {
    std::size_t index=0,bytes=0;unsigned group=0;
    float distanceSquared=0,at[3]={};bool known=false,rigid=false;float bone=0;
    std::uint64_t key=0;bool keep=true;
    float extra[2][3]={};unsigned char extras=0; /* extra world samples, only for stationary candidates */
    float root[3]={};bool hasRoot=false; /* palette bone 0 origin in world space (per-instance anchor) */
};
struct RigidEntry {std::uint64_t key=0,bodyKey=0;float at[3]={},anchor[3]={},bodyAt[3]={};unsigned free=0,age=0,still=0;bool attachment=false,settled=false,kept=false,used=false;};
struct Actor {
    std::size_t first=0,count=0,bytes=0,body=SIZE_MAX;float distanceSquared=0,rank=0,at[3]={},anchor[3]={},bodyAt[3]={};
    std::uint64_t key=0,bodyKey=0;unsigned pending=0,free=0,still=0;
    bool known=false,rigid=false,located=false,previous=false,matched=false,waiting=false,keep=true;
    bool rigidTracked=false,attachment=false,settled=false,orphan=false,search=false;
    RigidEntry* entry=nullptr;
    float samples[Samples][3]={};unsigned mask=0,stillFrames=0,stillExtras=0,moving=0;bool exempt=false,exemptCandidate=false,wasExempt=false,frozen=false;
    unsigned sinceChange=~0u;bool locked=false,rooted=false;float vertexAt[3]={};
};
struct Point {std::uint64_t cell=0;float x=0,y=0,z=0;std::size_t actor=0;};
struct Scratch {std::vector<Actor> actors;std::vector<std::size_t> order,candidates;std::vector<Point> points;std::vector<std::pair<std::size_t,std::size_t>> links;};
struct Result {
    std::size_t kept=0,dropped=0,keptBytes=0,droppedBytes=0,unknown=0;
    std::size_t actors=0,actorsKept=0,rigidActors=0,rigidDraws=0,rigidBytes=0,toggles=0,matched=0,retained=0,admitted=0,waiting=0,reserved=0;
    std::size_t attached=0,attachedBytes=0,orphans=0,rigidStuck=0,rigidNew=0;
    std::size_t exemptActors=0,exemptDraws=0,exemptBytes=0,cappedExempt=0,stillRankedSmall=0,exemptCapBinds=0,locked=0,exemptNpcLike=0,rooted=0,rootFallback=0,rootRejected=0;
};
namespace detail {
// Entries are sorted by (key, x): same-model crowds are searched only inside
// the x window of the radius instead of across every entry of the model.
template<class Entry> bool before(const Entry& a,const Entry& b){return a.key<b.key||(a.key==b.key&&a.at[0]<b.at[0]);}
template<class Entry> Entry* nearest(std::vector<Entry>& entries,std::uint64_t key,const float* at,float radius){
    auto it=std::lower_bound(entries.begin(),entries.end(),std::make_pair(key,at[0]-radius),[](const Entry& e,const std::pair<std::uint64_t,float>& k){return e.key<k.first||(e.key==k.first&&e.at[0]<k.second);});
    Entry* best=nullptr;float bestSquared=radius*radius;
    for(;it!=entries.end()&&it->key==key&&it->at[0]<=at[0]+radius;++it){if(it->used)continue;float d=0;
        for(unsigned a=0;a<3;++a){const float x=it->at[a]-at[a];d+=x*x;}
        if(d<=bestSquared){bestSquared=d;best=&*it;}}
    return best;
}
}
class History {
    struct Entry {std::uint64_t key=0;std::size_t bytes=0;float at[3]={};unsigned pending=0,age=0;bool kept=false,used=false;
        float samples[Samples][3]={};unsigned mask=0,stillFrames=0,stillExtras=0,moving=0,sinceChange=~0u;bool exempt=false;};
    using Rigid=RigidEntry;
    std::vector<Entry> entries_,next_;std::vector<Rigid> rigid_,nextRigid_,mergeRigid_;bool allKept_=false,ranking_=false;unsigned under_=0;float lastCut_=INFINITY,steadyCut_=INFINITY;
public:
    // Frames within budget keep everything; no ranking identities are needed
    // then. Rigid classes survive them: they do not depend on the budget.
    void keptAll(){entries_.clear();allKept_=true;lastCut_=steadyCut_=INFINITY;ranking_=false;under_=0;}
    void clear(){entries_.clear();rigid_.clear();allKept_=false;lastCut_=steadyCut_=INFINITY;ranking_=false;under_=0;}
    // Budget-state hysteresis: once over budget, keep ranking (identities,
    // pending admissions, stationary state) until actor bytes stay below
    // stay*budget for stayFrames frames. Within-budget frames then keep all
    // except delayed re-admissions instead of resetting to "everything".
    bool shouldRank(std::size_t actorBytes,std::size_t budget,const Tuning& tuning){
        if(!budget)return false;
        if(actorBytes>budget){ranking_=true;under_=0;return true;}
        if(!ranking_||!tuning.stayFrames)return false;
        under_=double(actorBytes)<double(budget)*tuning.stay?under_+1:0;
        if(under_>=tuning.stayFrames){ranking_=false;under_=0;return false;}
        return true;
    }
    // Read-only: does the renderer need extra world samples for this actor?
    bool stationaryHint(std::uint64_t key,const float* at)const{
        auto it=std::lower_bound(entries_.begin(),entries_.end(),std::make_pair(key,at[0]-IdentityTolerance),[](const Entry& e,const std::pair<std::uint64_t,float>& k){return e.key<k.first||(e.key==k.first&&e.at[0]<k.second);});
        for(;it!=entries_.end()&&it->key==key&&it->at[0]<=at[0]+IdentityTolerance;++it){float d=0;for(unsigned a=0;a<3;++a){const float x=it->at[a]-at[a];d+=x*x;}
            if(d<=IdentityTolerance*IdentityTolerance&&(it->stillFrames||it->exempt))return true;}
        return false;
    }
    // Distance squared of the farthest kept actor when the last ranked frame dropped any.
    float lastCut()const{return lastCut_;}
    // The cut the inner-cut test uses with the rate limit: it follows a shrinking
    // cut at once but grows at most 5% per frame, so a frame-to-frame swing of
    // the cut cannot pull actors in past the lock and drop them the next frame.
    float steadyCut()const{return steadyCut_;}
    // An unmatched actor releases the nearest stale reservation of its shape
    // within ClaimRadius (a fast mover would otherwise reserve its bytes again
    // every frame); same-model actors farther away keep theirs.
    void claim(std::uint64_t key,const float* at){if(auto* e=detail::nearest(entries_,key,at,ClaimRadius))e->used=true;}
    // No usable history (first ranked frame, after reset or a within-budget
    // frame): every actor counts as kept last frame.
    bool allKept()const{return allKept_||entries_.empty();}
    std::size_t size()const{return entries_.size()+rigid_.size();}
    Entry* find(std::uint64_t key,const float* at){return detail::nearest(entries_,key,at,IdentityTolerance);}
    Rigid* findRigid(std::uint64_t key,const float* at){return detail::nearest(rigid_,key,at,IdentityTolerance);}
    // Bytes of actors kept recently but absent from this frame's capture.
    std::size_t reserved(unsigned age=MaxAge)const{std::size_t n=0;for(const auto& e:entries_)if(!e.used&&e.kept&&!e.exempt&&e.age<age)n+=e.bytes;return n;}
    // Exempt actors out of view keep their room under the exempt cap.
    std::size_t exemptReserved()const{std::size_t n=0;for(const auto& e:entries_)if(!e.used&&e.exempt)n+=e.bytes;return n;}
    // Rigid classes persist for long (rigidMaxAge): matched entries are updated
    // in place, unmatched ones age, new ones are merged in; the (key, x) order is
    // restored by an insertion pass (positions move little), so no full sort of
    // thousands of stationary fences per frame.
    void publish(const std::vector<Actor>& actors,float cut,const Tuning& tuning){
        next_.clear();nextRigid_.clear();lastCut_=cut;
        float grown=steadyCut_;for(unsigned n=0;n<tuning.cadence||!n;++n)grown=grown*1.05f*1.05f; /* 5% per rendered frame */
        steadyCut_=!(cut<steadyCut_)&&std::isfinite(steadyCut_)?std::min(cut,grown):cut;
        for(const auto& a:actors){
            if(a.rigidTracked){Rigid r;r.key=a.key;std::memcpy(r.at,a.at,sizeof r.at);std::memcpy(r.anchor,a.anchor,sizeof r.anchor);r.free=a.free;r.still=a.still;r.attachment=a.attachment;r.settled=a.settled;r.kept=a.keep;r.bodyKey=a.bodyKey;std::memcpy(r.bodyAt,a.bodyAt,sizeof r.bodyAt);
                if(a.entry){r.used=true;*a.entry=r;}else nextRigid_.push_back(r);}
            else if(!a.rigid&&a.located){Entry e;e.key=a.key;e.bytes=a.bytes;std::memcpy(e.at,a.at,sizeof e.at);e.kept=a.keep;e.pending=a.pending;
                std::memcpy(e.samples,a.samples,sizeof e.samples);e.mask=a.mask;e.stillFrames=a.stillFrames;e.stillExtras=a.stillExtras;e.moving=a.moving;e.exempt=a.exempt;
                e.sinceChange=!a.matched?tuning.rateFrames:a.keep!=a.previous?0:std::min(a.sinceChange+1,tuning.rateFrames);next_.push_back(e);}}
        // Stationary state (exempt or accumulating) outlives a look away like a
        // rigid class does; other identities keep the short reservation age.
        if(!allKept_)for(const auto& e:entries_)if(!e.used&&e.age<((e.exempt||e.stillFrames>=tuning.frames(ProbationStill))?tuning.rigidMaxAge:tuning.reserveAge)){next_.push_back(e);++next_.back().age;}
        std::size_t write=0;
        for(std::size_t i=0;i<rigid_.size();++i){auto& r=rigid_[i];
            if(r.used){r.used=false;r.age=0;}else if(r.age<tuning.rigidMaxAge)++r.age;else continue;
            if(write!=i)rigid_[write]=r;++write;}
        rigid_.resize(write);
        for(std::size_t i=1;i<rigid_.size();++i)if(detail::before(rigid_[i],rigid_[i-1])){
            Rigid r=rigid_[i];std::size_t j=i;for(;j&&detail::before(r,rigid_[j-1]);--j)rigid_[j]=rigid_[j-1];rigid_[j]=r;}
        std::sort(nextRigid_.begin(),nextRigid_.end(),detail::before<Rigid>);
        if(!nextRigid_.empty()){mergeRigid_.clear();mergeRigid_.reserve(rigid_.size()+nextRigid_.size());
            std::merge(rigid_.begin(),rigid_.end(),nextRigid_.begin(),nextRigid_.end(),std::back_inserter(mergeRigid_),detail::before<Rigid>);rigid_.swap(mergeRigid_);}
        if(rigid_.size()>tuning.rigidCapacity){ /* keep the most recently seen */
            std::nth_element(rigid_.begin(),rigid_.begin()+std::ptrdiff_t(tuning.rigidCapacity),rigid_.end(),[](const Rigid& a,const Rigid& b){return a.age<b.age;});
            rigid_.resize(tuning.rigidCapacity);std::sort(rigid_.begin(),rigid_.end(),detail::before<Rigid>);}
        std::sort(next_.begin(),next_.end(),detail::before<Entry>);
        entries_.swap(next_);allKept_=false;
    }
};
// draws: capture order. Writes draws[i].keep.
// origin: rank by distance from this point (the player/shadow pivot) instead
// of the camera, so orbiting the camera does not reorder the cut.
inline Result choose(std::vector<Draw>& draws,Scratch& scratch,std::size_t budget,History& history,bool diagnostics=false,
    const float* origin=nullptr,const Tuning& tuning=Tuning{}){
    auto& actors=scratch.actors;auto& order=scratch.order;auto& points=scratch.points;auto& links=scratch.links;
    Result result;actors.clear();order.clear();points.clear();links.clear();
    for(std::size_t i=0;i<draws.size();){
        Actor a;a.first=i;a.key=draws[i].key;a.rigid=true;const float bone=draws[i].bone;
        std::size_t j=i;
        for(;j<draws.size()&&draws[j].group==draws[i].group;++j){const auto& d=draws[j];a.bytes+=d.bytes;
            a.rigid=a.rigid&&d.rigid&&d.bone==bone;
            const bool known=d.known&&std::isfinite(d.distanceSquared)&&d.distanceSquared>=0;
            float q=d.distanceSquared;if(known&&origin){q=0;for(unsigned k=0;k<3;++k){const float t=d.at[k]-origin[k];q+=t*t;}}
            if(known&&(!a.known||q<a.distanceSquared))a.distanceSquared=q;
            if(known&&!a.located){a.located=true;std::memcpy(a.at,d.at,sizeof a.at);}
            if(tuning.stableIdentity&&(d.key<a.key||j==i)){a.key=d.key;if(known){a.located=true;std::memcpy(a.at,d.at,sizeof a.at);}}
            a.known=a.known||known;}
        // Stable identity: the smallest draw key; its position when it has one.
        if(tuning.stableIdentity&&a.located)for(std::size_t k=i;k<j;++k){const auto& d=draws[k];
            if(d.key==a.key&&d.known&&std::isfinite(d.distanceSquared)&&d.distanceSquared>=0){std::memcpy(a.at,d.at,sizeof a.at);break;}}
        std::memcpy(a.vertexAt,a.at,sizeof a.at);
        // Per-instance anchor: the palette root. Identical instances of one model
        // share every draw key; their roots differ, and waving parts do not move it.
        // A root far from the actor's own vertices means the palette layout is not
        // the proven one: keep the vertex sample then.
        if(tuning.stableIdentity&&!a.rigid&&a.located)for(std::size_t k=i;k<j;++k)if(draws[k].hasRoot){ /* rigid actors keep vertex samples: co-movement of attachments */
            float q=0;for(unsigned c=0;c<3;++c){const float t=draws[k].root[c]-a.vertexAt[c];q+=t*t;}
            if(q<=RootReach*RootReach){std::memcpy(a.at,draws[k].root,sizeof a.at);a.rooted=true;}else ++result.rootRejected;break;}
        a.count=j-i;actors.push_back(a);i=j;
    }
    // Unlocated rigid actors cannot be classified: rank them.
    for(auto& a:actors)if(a.rigid&&!a.located)a.rigid=false;
    // Pass 1, in actor order: identities (marking entries used exactly as a
    // single pass would). A settled free-standing actor that has not moved needs
    // no body search (the common fence); only diagnostics count bodies near it.
    bool anySearch=false;
    for(auto& a:actors){if(!a.rigid)continue;auto* e=history.findRigid(a.key,a.at);if(e)e->used=true;a.entry=e;result.rigidNew+=!e;
        float m=0;if(e&&e->settled)for(unsigned k=0;k<3;++k){const float x=a.at[k]-e->anchor[k];m+=x*x;}
        a.search=!(e&&e->settled&&!(m>tuning.settledMove*tuning.settledMove))||diagnostics;anySearch=anySearch||a.search;}
    // Every sampled vertex of every non-rigid actor in a reach-sized xy grid.
    const float reach=AttachRadius*KeepReach;
    auto cellOf=[&](float x,float y,int dx,int dy){
        const auto cx=std::uint32_t(std::int32_t(std::floor(x/reach))+dx),cy=std::uint32_t(std::int32_t(std::floor(y/reach))+dy);
        return std::uint64_t(cx)<<32|cy;};
    if(anySearch)for(std::size_t n=0;n<actors.size();++n){const auto& a=actors[n];if(a.rigid)continue;
        for(std::size_t i=a.first;i<a.first+a.count;++i){const auto& d=draws[i];
            if(d.known&&std::isfinite(d.distanceSquared)&&d.distanceSquared>=0)points.push_back({cellOf(d.at[0],d.at[1],0,0),d.at[0],d.at[1],d.at[2],n});}}
    std::sort(points.begin(),points.end(),[](const Point& a,const Point& b){return a.cell<b.cell||(a.cell==b.cell&&(a.x<b.x||(a.x==b.x&&a.actor<b.actor)));});
    for(auto& a:actors){if(!a.rigid)continue;float best=reach*reach;std::size_t inReach=SIZE_MAX,same=SIZE_MAX;
        auto* e=a.entry;const std::size_t linked=links.size();
        // Nearest body over all draws of the rigid actor; ties go to the earlier
        // actor. An attachment stays with last frame's body while it is in reach.
        // "same" is last frame's body: the first rigid draw that sees it wins, and
        // within that draw the point with the smallest (x, actor).
        if(a.search)for(std::size_t i=a.first;i<a.first+a.count;++i){const auto& d=draws[i];
            if(!(d.known&&std::isfinite(d.distanceSquared)&&d.distanceSquared>=0))continue;
            const Point* sameAt=nullptr;
            for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy){const auto cell=cellOf(d.at[0],d.at[1],dx,dy);
                auto it=std::lower_bound(points.begin(),points.end(),cell,[](const Point& p,std::uint64_t c){return p.cell<c;});
                for(;it!=points.end()&&it->cell==cell;++it){const float x=it->x-d.at[0],y=it->y-d.at[1],z=it->z-d.at[2],q=x*x+y*y+z*z;
                    if(q>reach*reach)continue;
                    if(q<=AttachRadius*AttachRadius&&(links.size()==linked||links.back().second!=it->actor))links.push_back({std::size_t(&a-actors.data()),it->actor});
                    if(e&&e->attachment&&same==SIZE_MAX&&actors[it->actor].key==e->bodyKey&&actors[it->actor].located&&
                       (!sameAt||it->x<sameAt->x||(it->x==sameAt->x&&it->actor<sameAt->actor))){float b=0;
                        for(unsigned k=0;k<3;++k){const float t=actors[it->actor].at[k]-e->bodyAt[k];b+=t*t;}
                        if(b<=IdentityTolerance*IdentityTolerance)sameAt=&*it;}
                    if(q<best||(q==best&&inReach!=SIZE_MAX&&it->actor<inReach)){best=q;inReach=it->actor;}}}
            if(sameAt)same=sameAt->actor;}
        a.rigidTracked=true;std::memcpy(a.anchor,a.at,sizeof a.anchor);
        const std::size_t tight=best<=AttachRadius*AttachRadius?inReach:SIZE_MAX; /* new attachments need the tight radius */
        auto moved=[&](const float* anchor,float limit){float m=0;for(unsigned k=0;k<3;++k){const float x=a.at[k]-anchor[k];m+=x*x;}return m>limit*limit;};
        bool follow=false;
        if(e&&e->attachment){ /* confirmed: keep the body; free only after FreeFrames without one */
            if(same!=SIZE_MAX)inReach=same;a.previous=e->kept;a.free=inReach!=SIZE_MAX?0:e->free+1;
            a.attachment=a.free<tuning.frames(FreeFrames);follow=a.attachment;if(!a.attachment)a.settled=true;}
        else if(e&&e->settled){ /* free-standing: only real movement beside a body re-attaches */
            a.settled=true;std::memcpy(a.anchor,e->anchor,sizeof a.anchor);
            if(!moved(e->anchor,tuning.settledMove))result.rigidStuck+=inReach!=SIZE_MAX;
            else if(tight!=SIZE_MAX){a.settled=false;a.attachment=follow=true;inReach=tight;}
            else std::memcpy(a.anchor,a.at,sizeof a.anchor);}
        else{ /* probation: attach on co-movement beside a body; follow it until still for ProbationStill frames */
            if(e)std::memcpy(a.anchor,e->anchor,sizeof a.anchor);a.free=e?e->free+1:1;inReach=tight;
            a.still=e?e->still+1:0;
            if(tight!=SIZE_MAX){a.attachment=e&&moved(e->anchor,CoMoveTolerance);follow=a.attachment||a.still<tuning.frames(ProbationStill)||tuning.probationFollow;if(follow)a.previous=e&&e->kept;}
            if(!a.attachment&&a.free>=tuning.frames(FreeFrames))a.settled=true;
            if(a.attachment)a.free=0;}
        if(!follow){links.resize(linked);continue;}
        a.rigid=false;
        if(inReach!=SIZE_MAX){a.body=inReach;a.bodyKey=actors[inReach].key;std::memcpy(a.bodyAt,actors[inReach].at,sizeof a.bodyAt);++result.attached;result.attachedBytes+=a.bytes;}
        else{a.orphan=true;++result.orphans;}}
    const bool allKept=history.allKept();const float scale=(1-tuning.margin)*(1-tuning.margin);
    std::vector<std::size_t>& candidates=scratch.candidates;candidates.clear();
    for(std::size_t n=0;n<actors.size();++n){auto& a=actors[n];
        if(a.rigid){++result.rigidActors;result.rigidDraws+=a.count;result.rigidBytes+=a.bytes;continue;}
        if(a.body!=SIZE_MAX||a.orphan)continue;
        ++result.actors;result.unknown+=!a.known;result.rooted+=a.rooted;result.rootFallback+=a.located&&!a.rooted;
        decltype(history.find(0,a.at)) e=nullptr;
        if(allKept)a.previous=true;
        else if(a.located)if((e=history.find(a.key,a.at))){e->used=true;a.matched=true;a.previous=e->kept;a.pending=e->pending;++result.matched;a.sinceChange=e->sinceChange;}
        a.locked=tuning.rateFrames&&a.matched&&a.sinceChange<tuning.rateFrames;result.locked+=a.locked;
        if(tuning.stationary&&a.located){
            // World samples: the sampled vertex of up to four draws, plus two
            // extra vertices of each of the first two draws when provided.
            // Rooted: the palette root is the one pose sample (waving parts ignored).
            // plus the vertex sample with a loose 2 yd tolerance: a root that stays put
            // while the object walks away (an unproven palette layout) cannot exempt it.
            if(a.rooted){std::memcpy(a.samples[0],a.at,sizeof a.at);std::memcpy(a.samples[1],a.vertexAt,sizeof a.vertexAt);a.mask=3;}
            // Stable identity orders the sampled draws by key (issue order varies).
            std::size_t slots[4];unsigned used=0;
            if(!a.rooted)for(std::size_t i=a.first;i<a.first+a.count;++i){
                if(!tuning.stableIdentity){if(used<4)slots[used++]=i;continue;}
                unsigned at=used<4?used:4;while(at&&draws[slots[at-1]].key>draws[i].key){if(at<4)slots[at]=slots[at-1];--at;}
                if(at<4){slots[at]=i;if(used<4)++used;}}
            if(!a.rooted)for(unsigned k=0;k<used;++k){const auto& d=draws[slots[k]];
                if(d.known&&std::isfinite(d.distanceSquared)&&d.distanceSquared>=0){std::memcpy(a.samples[k],d.at,sizeof d.at);a.mask|=1u<<k;}
                if(k<(tuning.stableIdentity?4u:2u))for(unsigned x=0;x<d.extras&&x<2;++x){std::memcpy(a.samples[4+2*k+x],d.extra[x],sizeof d.extra[x]);a.mask|=1u<<(4+2*k+x);}}
            if(e){const unsigned common=a.mask&e->mask;bool within=(common&15)!=0;
                for(unsigned k=0;k<Samples&&within;++k)if(common&(1u<<k)){float q=0;for(unsigned c=0;c<3;++c){const float t=a.samples[k][c]-e->samples[k][c];q+=t*t;}const float tolerance=a.rooted&&k==1?LooseTolerance:StationaryTolerance;within=q<=tolerance*tolerance;}
                if(within){a.moving=0;a.stillFrames=e->stillFrames+1;a.stillExtras=(common&0xff0)?e->stillExtras+1:0;
                    for(unsigned k=0;k<Samples;++k)if(common&(1u<<k))std::memcpy(a.samples[k],e->samples[k],sizeof a.samples[k]);
                    a.exempt=e->exempt;}
                else{a.moving=e->moving+1;
                    if(e->exempt&&a.moving<tuning.frames(ExemptMoveFrames)){std::memcpy(a.samples,e->samples,sizeof a.samples);a.mask=e->mask;a.stillFrames=e->stillFrames;a.stillExtras=e->stillExtras;a.exempt=true;}}}
            a.wasExempt=a.exempt;a.exemptCandidate=a.exempt||(a.stillFrames>=tuning.frames(StationaryFrames)&&(a.rooted||a.stillExtras>=tuning.frames(StationaryFrames)/2));
            result.stillRankedSmall+=a.stillFrames>=tuning.frames(StationaryFrames)&&!a.exemptCandidate&&a.count<=2;}
        a.exempt=false;
        if(a.exemptCandidate){candidates.push_back(n);continue;}
        a.frozen=tuning.stationary&&a.matched&&a.stillFrames>=tuning.frames(ProbationStill); /* still, not (yet) exempt */
        a.rank=a.previous?a.distanceSquared*(a.frozen?scale*scale:scale):a.distanceSquared;order.push_back(n);
    }
    if(!candidates.empty()){
        // Exempt bytes (free-standing rigid + stationary) stay under
        // exemptFraction of the budget: nearest stationary actors first, the rest
        // are ranked like any actor.
        // Already-exempt actors keep their state even if rigid bytes in view grew
        // (a moving cap boundary would flip them just like the quota cut did);
        // newly stationary ones fill the remaining room nearest first.
        std::sort(candidates.begin(),candidates.end(),[&](std::size_t x,std::size_t y){const auto& a=actors[x];const auto& b=actors[y];
            if(a.wasExempt!=b.wasExempt)return a.wasExempt;
            if(a.distanceSquared!=b.distanceSquared)return a.distanceSquared<b.distanceSquared;return a.first<b.first;});
        const double cap=double(budget)*tuning.exemptFraction;double exempt=(tuning.sharedWithRigid?double(result.rigidBytes):0.)+double(history.exemptReserved());
        for(auto n:candidates){auto& a=actors[n];
            if(a.wasExempt||exempt+double(a.bytes)<=cap){ /* the cap bounds new exemptions; granted ones stay */exempt+=double(a.bytes);a.exempt=true;a.keep=true;++result.exemptActors;result.exemptDraws+=a.count;result.exemptBytes+=a.bytes;result.exemptNpcLike+=a.count>=3;
                result.toggles+=a.matched&&!a.previous;}
            else{++result.cappedExempt;result.exemptCapBinds=1;a.frozen=a.matched;a.rank=a.previous?a.distanceSquared*scale*scale:a.distanceSquared;order.push_back(n);}}}
    if(!allKept)for(auto n:order)if(!actors[n].matched&&actors[n].located)history.claim(actors[n].key,actors[n].at);
    // Admission delay applies only near the cut: unlocated actors keep their first
    // claim and actors well inside the previous cut need no identity at all.
    const float inner=InnerCut*InnerCut*(tuning.rateFrames?history.steadyCut():history.lastCut());
    // Tiers: unknown distance (first claim), then every actor inside the inner
    // cut (near shadows are guaranteed, locked or not), then actors kept and
    // locked by the rate limit (nearest first), then the rest by rank. A full
    // budget therefore drops free actors before locked ones, but never a near
    // actor for a far locked one.
    const bool rateInner=tuning.rateFrames!=0;
    std::sort(order.begin(),order.end(),[&](std::size_t x,std::size_t y){const auto& a=actors[x];const auto& b=actors[y];
        if(a.known!=b.known)return !a.known;
        if(rateInner){const bool ia=a.distanceSquared<=inner,ib=b.distanceSquared<=inner;if(ia!=ib)return ia;if(ia&&a.rank!=b.rank)return a.rank<b.rank;}
        const bool la=a.locked&&a.previous,lb=b.locked&&b.previous;
        if(la!=lb)return la;
        if(la&&a.distanceSquared!=b.distanceSquared)return a.distanceSquared<b.distanceSquared;
        if(a.known&&a.rank!=b.rank)return a.rank<b.rank;
        return a.first<b.first;});
    const std::size_t reserve=allKept?0:std::min(budget,history.reserved(tuning.reserveAge));
    // With the rate limit, unknown and inner-cut actors are charged before the
    // reservation of absent actors (a turn into a new crowd must not lose its
    // near shadows to the crowd left behind); everyone else after it.
    bool reserving=!rateInner;std::size_t used=reserving?reserve:0;bool open=true;result.reserved=reserve;
    for(auto n:order){auto& a=actors[n];
        if(!reserving&&a.known&&!(a.distanceSquared<=inner)){reserving=true;used=std::min(budget,used+reserve);}
        const bool fits=open&&a.bytes<=budget-used;
        if(!fits){a.keep=false;a.pending=0;open=false;}
        else if(a.locked&&!a.previous&&!(a.distanceSquared<=inner)){a.keep=false;a.waiting=true;++result.waiting;} /* dropped recently: stays dropped for rateFrames unless inside the inner cut */
        else if(!a.previous&&a.located&&!(a.distanceSquared<=inner)&&a.pending+1<(a.frozen?3*tuning.admitFrames:tuning.admitFrames)){ /* a still dropped actor needs room three times as long */a.keep=false;a.waiting=true;++a.pending;++result.waiting;}
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
        if(a.exempt)continue; /* stationary: outside the quota, counted in exempt* */
        if(a.keep){++result.kept;result.keptBytes+=d.bytes;}else{++result.dropped;result.droppedBytes+=d.bytes;}}
    float cut=0;bool cutting=false;
    for(auto n:order){const auto& a=actors[n];cutting=cutting||!a.keep;if(a.keep&&a.known)cut=std::max(cut,a.distanceSquared);}
    history.publish(actors,cutting?cut:INFINITY,tuning);
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
