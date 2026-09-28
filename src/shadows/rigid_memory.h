#pragma once
// 0.3.172 rigid memory. A server-placed rigid prop (a shop sign, an event fence) is a per-frame
// replay: it casts only in frames in which the game draws it, so its shadow vanished when the
// camera turned away from it (or when the capture budget turned its draw away). This registry
// remembers the last captured replay of a settled, free-standing, small rigid group and has the
// renderer draw a copy of it (a replay draw only: no cache signature, no conversion) while the
// game does not draw it.
// Remembered: every draw of the group is driven by ONE palette bone (the renderer's audited
// programs and rigidBones), at most maxDraws/maxTriangles/maxMeshBytes, and its bone's world
// matrix W (worldBone) has stayed within stillTolerance/axisTolerance for settleMs AND
// settleFrames capture frames. Never remembered (sticky for the track): mobile (W travelled more
// than travelTolerance or turned more than turnTolerance since the track began: lifts, boats, a
// door opened while seen), held (a non-rigid group's palette root within attachRadius in any
// capture frame: weapons, shields), static (the static cache draws it: staticPlacement at W).
// Identity: a hash of the draws' shapes and W's origin within identityTolerance, never the
// snapshot pointer (a re-created snapshot keeps the entry; the renderer refreshes its copy on
// every capture frame in which it is seen).
// Absent (not captured this capture frame): dropped when W's origin is in view within viewRange
// on absentFrames complete frames (no capture shortfall) and absentMs (a despawn; a game cull
// fails toward no shadow, never toward a stale one on screen), beyond range from the pivot, or
// after unseenMs; otherwise drawn (off screen, and on shortfall frames). A pivot jump beyond
// teleportDistance clears everything; so do the renderer's map change, device loss and trim.
// Seen again with another W (moved, turned) or held: dropped, and the track is mobile or held.
// Caps: maxEntries and maxBytes of mesh (the farthest from the pivot goes), maxTracks; a track
// without an entry is forgotten after forgetMs unseen. Portable: the Payload (the renderer's
// replay copies) is opaque here; no D3D calls.
#include "rigid_geometry.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>
namespace NorthlightRigidMemory {
using NorthlightRigidGeometry::Vec3;
struct Tuning {
    unsigned settleMs=2000,settleFrames=4;          /* time AND capture frames: cadence and frame rate do not rescale it */
    float stillTolerance=.02f,axisTolerance=.01f;    /* W origin (yd) and basis (times its scale) */
    float travelTolerance=.5f,turnTolerance=.05f;    /* since the track began: mobile for good */
    float attachRadius=4.f;                          /* a non-rigid root this near: held for good */
    float identityTolerance=.25f;                    /* same shapes, W origin this near: the same object */
    unsigned maxDraws=4,maxTriangles=4096;std::size_t maxMeshBytes=256u<<10;
    float viewRange=60.f;unsigned absentFrames=2,absentMs=150;
    float range=80.f,teleportDistance=100.f;unsigned unseenMs=600000;
    std::size_t maxEntries=64,maxBytes=2u<<20,maxTracks=2048;unsigned forgetMs=60000;
};
// One captured rigid group of this capture frame, built by the renderer.
struct Observation {
    std::uint64_t shape=0;float world[12]={}; /* W: world images of the bone's model axes (9), then of its origin (3) (worldBone) */
    unsigned draws=0,triangles=0;std::size_t bytes=0; /* bytes: mesh bytes of its draws */
    enum Action : unsigned char {None,Refresh,Remember};
    Action action=None;std::uint32_t track=0;float distanceSquared=0; /* out: store() a fresh copy for Refresh/Remember */
};
// Identity of a group's shapes: each draw's original shader, declaration, vertex and primitive
// counts and mesh bytes, in draw order (start with ShapeSeed). Never the snapshot pointer.
constexpr std::uint64_t ShapeSeed=14695981039346656037ull;
inline void mixShape(std::uint64_t& h,const void* shader,const void* declaration,unsigned vertices,unsigned primitives,std::size_t bytes){
    for(std::uint64_t n:{std::uint64_t(reinterpret_cast<std::uintptr_t>(shader)),std::uint64_t(reinterpret_cast<std::uintptr_t>(declaration)),std::uint64_t(vertices),std::uint64_t(primitives),std::uint64_t(bytes)})h=(h^n)*1099511628211ull;}
struct Stats {std::size_t tracks=0,entries=0,seen=0,held=0,statics=0,mobile=0,bytes=0;
    std::uint64_t remembered=0,droppedInView=0,droppedRange=0,droppedTeleport=0,droppedMoved=0,droppedUnseen=0,evicted=0,cleared=0;};
// Palette rows (3 x 4 floats, the c31+3b layout) that put W in the view of `inverseView`
// (view -> world; its 3x3 part is inverted, not transposed): worldBone(rows, inverseView) == W.
inline bool rebase(const float* world,const float* inverseView,float* rows){
    double m[3][3],inv[3][3];for(unsigned r=0;r<3;++r)for(unsigned w=0;w<3;++w)m[r][w]=inverseView[4*r+w];
    const double det=m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1])-m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0])+m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0]);
    if(!std::isfinite(det)||std::fabs(det)<1e-12)return false;
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c){const unsigned r1=(c+1)%3,r2=(c+2)%3,c1=(r+1)%3,c2=(r+2)%3;
        inv[r][c]=(m[r1][c1]*m[r2][c2]-m[r1][c2]*m[r2][c1])/det;} /* inverse = adjugate / det */
    for(unsigned a=0;a<4;++a){double x[3];for(unsigned w=0;w<3;++w)x[w]=a<3?double(world[a*3+w]):double(world[9+w])-double(inverseView[12+w]);
        for(unsigned r=0;r<3;++r){double v=0;for(unsigned w=0;w<3;++w)v+=x[w]*inv[w][r];rows[4*r+a]=float(v);}}
    for(unsigned k=0;k<12;++k)if(!std::isfinite(rows[k]))return false;
    return true;
}
template<class Payload> class Registry {
public:
    struct Entry {std::uint32_t track=0;std::uint64_t shape=0;float world[12]={};Payload payload{};std::size_t bytes=0;
        unsigned lastSeenMs=0,absentFrames=0,absentSinceMs=0;bool matched=false;float distanceSquared=0;};
private:
    struct Track {std::uint64_t shape=0;std::uint32_t serial=0;float anchor[12]={},first[12]={},at[3]={};unsigned sinceMs=0,frames=0,lastSeenMs=0;
        bool used=false,mobile=false,held=false,statics=false,entry=false;signed char placed=0; /* static screen: -1 not static, 0 not asked */};
    Tuning t_;std::vector<Track> tracks_;std::vector<Entry> entries_;Stats stats_;
    float lastPivot_[3]={};bool hasPivot_=false,screening_=false;std::uint32_t nextSerial_=1;
    static float distance2(const float* a,const float* b){float q=0;for(unsigned k=0;k<3;++k){const float d=a[k]-b[k];q+=d*d;}return q;}
    static float axesDifference(const float* a,const float* b){float m=0;for(unsigned k=0;k<9;++k)m=std::max(m,std::fabs(a[k]-b[k]));return m;}
    static float axesScale(const float* a){float m=1;for(unsigned k=0;k<9;++k)m=std::max(m,std::fabs(a[k]));return m;}
    bool still(const float* a,const float* b)const{return distance2(a+9,b+9)<=t_.stillTolerance*t_.stillTolerance&&axesDifference(a,b)<=t_.axisTolerance*axesScale(b);}
    // Tracks are ordered by shape (then serial) up to `sorted`; later ones are this frame's new tracks.
    std::size_t find(std::uint64_t shape,const float* at,std::size_t sorted)const{
        std::size_t best=SIZE_MAX;float bestSquared=t_.identityTolerance*t_.identityTolerance;
        auto visit=[&](std::size_t i){const auto& t=tracks_[i];if(t.used||t.shape!=shape)return;const float d=distance2(t.at,at);if(d<=bestSquared){bestSquared=d;best=i;}};
        auto it=std::lower_bound(tracks_.begin(),tracks_.begin()+std::ptrdiff_t(sorted),shape,[](const Track& t,std::uint64_t s){return t.shape<s;});
        for(std::size_t i=std::size_t(it-tracks_.begin());i<sorted&&tracks_[i].shape==shape;++i)visit(i);
        for(std::size_t i=sorted;i<tracks_.size();++i)visit(i);
        return best;
    }
    Track* track(std::uint32_t serial){for(auto& t:tracks_)if(t.serial==serial)return &t;return nullptr;}
    std::size_t entryOf(std::uint32_t serial)const{for(std::size_t i=0;i<entries_.size();++i)if(entries_[i].track==serial)return i;return SIZE_MAX;}
    void drop(std::size_t i,std::uint64_t& counter){++counter;stats_.bytes-=std::min(stats_.bytes,entries_[i].bytes);
        if(auto* t=track(entries_[i].track))t->entry=false;entries_.erase(entries_.begin()+std::ptrdiff_t(i));}
    std::size_t farthest()const{std::size_t f=SIZE_MAX;for(std::size_t i=0;i<entries_.size();++i)if(f==SIZE_MAX||entries_[i].distanceSquared>entries_[f].distanceSquared)f=i;return f;}
    // Room for `o` within the caps, evicting only entries farther from the pivot than it.
    bool room(const Observation& o)const{
        if(o.bytes>t_.maxBytes||!t_.maxEntries)return false;
        std::vector<char> gone;std::size_t count=entries_.size(),bytes=stats_.bytes;
        while(count+1>t_.maxEntries||bytes+o.bytes>t_.maxBytes){std::size_t f=SIZE_MAX;
            for(std::size_t i=0;i<entries_.size();++i){if(gone.size()>i&&gone[i])continue;if(f==SIZE_MAX||entries_[i].distanceSquared>entries_[f].distanceSquared)f=i;}
            if(f==SIZE_MAX||!(entries_[f].distanceSquared>o.distanceSquared))return false;
            gone.resize(entries_.size(),0);gone[f]=1;--count;bytes-=std::min(bytes,entries_[f].bytes);}
        return true;
    }
public:
    explicit Registry(Tuning t=Tuning{}):t_(t){}
    const Tuning& tuning()const{return t_;}
    const Stats& stats()const{return stats_;}
    const std::vector<Entry>& entries()const{return entries_;}
    bool small(const Observation& o)const{return o.draws&&o.draws<=t_.maxDraws&&o.triangles<=t_.maxTriangles&&o.bytes<=t_.maxMeshBytes;}
    // A settled track waits for the static-cache screen (the renderer builds its index meanwhile).
    bool screening()const{return screening_;}
    // Map change, device loss, trim, shadows off: everything is forgotten (payloads released).
    void clear(){stats_.cleared+=entries_.size();entries_.clear();tracks_.clear();stats_.bytes=0;hasPivot_=false;screening_=false;}
    // One capture frame. bodies: palette roots (3 floats each) of this frame's non-rigid groups.
    // complete: no draw was lost to capture limits. covered(n): 1 the static cache draws obs[n],
    // 0 it does not, -1 not known yet (asked again next frame).
    template<class Covered> void frame(std::vector<Observation>& obs,const float* bodies,std::size_t bodyCount,unsigned now,const float* pivot,
                                       const float* inverseView,const float* projection,bool complete,Covered covered){
        screening_=false;
        if(pivot){if(hasPivot_&&distance2(pivot,lastPivot_)>t_.teleportDistance*t_.teleportDistance){stats_.droppedTeleport+=entries_.size();entries_.clear();tracks_.clear();stats_.bytes=0;}
            std::memcpy(lastPivot_,pivot,12);hasPivot_=true;}
        for(auto& t:tracks_)t.used=false;for(auto& e:entries_)e.matched=false;
        const std::size_t sorted=tracks_.size();const float attach2=t_.attachRadius*t_.attachRadius;
        for(std::size_t n=0;n<obs.size();++n){auto& o=obs[n];o.action=Observation::None;o.distanceSquared=pivot?distance2(o.world+9,pivot):0;
            std::size_t k=find(o.shape,o.world+9,sorted);
            if(k==SIZE_MAX){Track f;f.shape=o.shape;f.serial=nextSerial_++;if(!nextSerial_)nextSerial_=1;std::memcpy(f.anchor,o.world,48);std::memcpy(f.first,o.world,48);
                f.sinceMs=f.lastSeenMs=now;f.frames=1;tracks_.push_back(f);k=tracks_.size()-1;}
            else{auto& t=tracks_[k];t.lastSeenMs=now;
                if(still(o.world,t.anchor))++t.frames;else{std::memcpy(t.anchor,o.world,48);t.sinceMs=now;t.frames=1;t.placed=0;}
                if(distance2(o.world+9,t.first+9)>t_.travelTolerance*t_.travelTolerance||axesDifference(o.world,t.first)>t_.turnTolerance*axesScale(t.first))t.mobile=true;}
            auto& t=tracks_[k];t.used=true;std::memcpy(t.at,o.world+9,12);o.track=t.serial;
            for(std::size_t b=0;b<bodyCount&&!t.held;++b)if(distance2(bodies+3*b,o.world+9)<=attach2)t.held=true;
            if(t.entry){const std::size_t i=entryOf(t.serial);
                if(i==SIZE_MAX)t.entry=false;
                else{auto& e=entries_[i];e.matched=true;e.lastSeenMs=now;e.absentFrames=0;e.distanceSquared=o.distanceSquared;
                    if(!still(o.world,e.world)||t.mobile||t.held){if(!t.held)t.mobile=true;drop(i,stats_.droppedMoved);} /* a door opened, a weapon's body came back */
                    else{o.action=Observation::Refresh;continue;}}}
            if(t.mobile||t.held||t.statics||!small(o)||t.frames<t_.settleFrames||unsigned(now-t.sinceMs)<t_.settleMs)continue;
            if(!t.placed){const int c=covered(n);if(c>0)t.statics=true;else if(c==0)t.placed=-1;else screening_=true;}
            if(t.placed<0&&room(o))o.action=Observation::Remember;
        }
        // Absent entries: range, the safety timeout, the in-view despawn test (complete frames only).
        for(std::size_t i=0;i<entries_.size();){auto& e=entries_[i];e.distanceSquared=pivot?distance2(e.world+9,pivot):e.distanceSquared;
            if(e.distanceSquared>t_.range*t_.range){drop(i,stats_.droppedRange);continue;}
            if(!e.matched){
                if(unsigned(now-e.lastSeenMs)>=t_.unseenMs){drop(i,stats_.droppedUnseen);continue;}
                if(complete){const Vec3 at(e.world[9],e.world[10],e.world[11]);
                    if(NorthlightRigidGeometry::centreInView(inverseView,projection,at,at,t_.viewRange)){if(!e.absentFrames++)e.absentSinceMs=now;
                        if(e.absentFrames>=t_.absentFrames&&unsigned(now-e.absentSinceMs)>=t_.absentMs){drop(i,stats_.droppedInView);continue;}}
                    else e.absentFrames=0;}}
            ++i;}
        // Forget tracks (never one with an entry): unseen forgetMs, then the oldest above maxTracks.
        tracks_.erase(std::remove_if(tracks_.begin(),tracks_.end(),[&](const Track& t){return !t.entry&&unsigned(now-t.lastSeenMs)>=t_.forgetMs;}),tracks_.end());
        if(tracks_.size()>t_.maxTracks){std::vector<std::pair<unsigned,std::uint32_t>> age;for(const auto& t:tracks_)if(!t.entry)age.push_back({unsigned(now-t.lastSeenMs),t.serial});
            std::sort(age.begin(),age.end(),[](const auto& a,const auto& b){return a.first!=b.first?a.first>b.first:a.second<b.second;});
            std::vector<std::uint32_t> drop;for(std::size_t i=0;i<age.size()&&tracks_.size()-drop.size()>t_.maxTracks;++i)drop.push_back(age[i].second);std::sort(drop.begin(),drop.end());
            tracks_.erase(std::remove_if(tracks_.begin(),tracks_.end(),[&](const Track& t){return std::binary_search(drop.begin(),drop.end(),t.serial);}),tracks_.end());}
        std::sort(tracks_.begin(),tracks_.end(),[](const Track& a,const Track& b){return a.shape!=b.shape?a.shape<b.shape:a.serial<b.serial;});
        stats_.tracks=tracks_.size();stats_.held=stats_.statics=stats_.mobile=0;
        for(const auto& t:tracks_){stats_.held+=t.held;stats_.statics+=t.statics;stats_.mobile+=t.mobile;}
        stats_.seen=0;for(const auto& e:entries_)stats_.seen+=e.matched;stats_.entries=entries_.size();
    }
    // A fresh copy for an observation frame() marked Refresh (replaces the entry's) or Remember
    // (a new entry; the farthest entries beyond the caps go). False: nothing stored.
    bool store(const Observation& o,Payload&& payload,unsigned now){
        if(o.action==Observation::Refresh){const std::size_t i=entryOf(o.track);if(i==SIZE_MAX)return false;auto& e=entries_[i];
            stats_.bytes=stats_.bytes-std::min(stats_.bytes,e.bytes)+o.bytes;e.bytes=o.bytes;e.payload=std::move(payload);return true;}
        if(o.action!=Observation::Remember)return false;Track* t=track(o.track);if(!t||t->entry||!room(o))return false;
        while(entries_.size()+1>t_.maxEntries||stats_.bytes+o.bytes>t_.maxBytes){const std::size_t f=farthest();if(f==SIZE_MAX)return false;drop(f,stats_.evicted);}
        Entry e;e.track=o.track;e.shape=o.shape;std::memcpy(e.world,o.world,48);e.payload=std::move(payload);e.bytes=o.bytes;e.lastSeenMs=now;e.matched=true;e.distanceSquared=o.distanceSquared;
        entries_.push_back(std::move(e));if((t=track(o.track)))t->entry=true;stats_.bytes+=o.bytes;++stats_.remembered;stats_.entries=entries_.size();return true;
    }
    // Entries the game did not draw this capture frame: the renderer draws their copies.
    template<class F> void forAbsent(F f){for(auto& e:entries_)if(!e.matched)f(e);}
};
}
