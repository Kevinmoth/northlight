#pragma once
#include "actor_scene_job.h"
#include "actor_shadow_selection.h"
#include "shadow_bounds.h"
#include "rigid_geometry.h"
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <mutex>
#include <thread>

// 0.3.141 persistent casters. A captured replay actor (constant group) that has
// stood still for stillMs (and stillFrames captured frames) is converted ONCE into a world-
// space mesh: its captured draws (snapshot, bone palette, inverse view) are
// evaluated through the same CPU emulation of the game's vertex program that
// GI actor packets use (NorthlightActorDeformation), on a background thread. The
// mesh is drawn into the cached directional shadow map like a static caster
// (dirty-rect partial redraw on add/remove), so it keeps casting while the game
// does not draw the object (camera turned away) and it leaves the per-frame
// replay path and the actor budget (no double draw, no budget toggling).
// Identity: the stable actor key (smallest per-draw snapshot key) plus the
// palette root (bone 0 origin) in world space, exactly as 0.3.140 selection.
// Removed (and returned to the replay path) when its root/orientation/sampled
// vertex moves, it reappears with more draws or a new key, it is unseen for
// unseenMs, leaves range, stays absent while fully on screen and near (a
// despawn), is evicted for a nearer one, or the map changes. Thresholds are in
// time as well as observed frames, so frames without model capture (Near/Far
// ShadowInterval capture skipping) or a low frame rate do not rescale them.
// Stillness is judged on the FULL world-space pose: every palette bone the
// group's vertices reference (weight > 0) must keep its world matrix (origin
// within poseTolerance, basis within axisTolerance) for stillMs, as well as the
// root. Idle animation (breathing, /dance, /wave), waving cloth and turning
// therefore never convert, and any later bone motion removes the caster at once.
// Also excluded: >= characterBones distinct bone MATRICES in the group, any
// actor that ever travelled or turned while tracked, and single-bone (rigid)
// actors within attachRadius of a non-rigid actor that is not itself a
// persistent structure (a weapon beside its owner). v1 is opaque only
// (AlphaCutouts=false): a group with an alpha-tested draw stays on the replays.
// Audited palette programs: the client's four-weight blend (SkinEnvelope) and
// its one-influence program (oneBoneTemplate, rigid_geometry.h). Rigid props that
// stand still are remembered by rigid_memory.h (0.3.172; PersistentRigidProps retired).
// Portable (no D3D calls): the renderer owns device resources.
namespace NorthlightPersistentCasters {
constexpr bool Enabled=true;
// v1: alpha-tested groups stay on the replay path (the converter supports
// cutouts from the <=128 px mip copy, but the cutout shape would differ slightly).
constexpr bool AlphaCutouts=false;
using Vec3=NorthlightGI::Vec3;
using NorthlightRigidGeometry::oneBoneTemplate;using NorthlightRigidGeometry::worldBone; /* rigid_geometry.h */
struct Tuning {
    std::size_t budgetBytes=8u<<20;unsigned maxCasters=256,maxVertices=32768,maxDraws=32,maxPending=1;
    // Time AND observed-frame thresholds: independent of frame rate and of frames
    // without model capture (still = 2 s like the 120 frames of 0.3.140 at 60 FPS).
    unsigned stillMs=2000,stillFrames=30;
    float stillTolerance=NorthlightActorShadowSelection::StationaryTolerance,axisTolerance=.01f,poseTolerance=NorthlightActorShadowSelection::StationaryTolerance;
    unsigned poseAfterFrames=5; /* root still this long: the renderer starts supplying the pose */
    unsigned publishMs=500; /* new ready casters join the cache as one batch at most this often */
    float identityTolerance=NorthlightActorShadowSelection::IdentityTolerance,moveTolerance=.10f,travelTolerance=.5f,turnTolerance=.05f;
    float vertexTolerance=NorthlightActorShadowSelection::LooseTolerance; /* waving parts stay within it */
    unsigned unseenMs=60000,cooldownMs=10000,deferMs=2000,evictIntervalMs=1000;float evictMargin=.8f;
    float range=320.f,viewRange=60.f;unsigned absentInView=8,absentInViewMs=500;
    unsigned characterBones=24;float attachRadius=NorthlightActorShadowSelection::AttachRadius;
    std::size_t trackCapacity=8192;
    bool general=true; /* the still multi-bone class (PersistentCasters) */
};
// One captured actor (constant group) of this frame, built by the renderer.
struct Observation {
    std::uint64_t key=0;float root[3]={},axes[9]={},distanceSquared=0; /* axes: palette bone 0 basis in world space */
    std::size_t first=0,end=0,count=0; /* renderer's draw range [first,end) holding `count` eligible draws */
    const float* pose=nullptr;unsigned poseBones=0; /* world matrix (basis 9 + origin 3) of every referenced bone, when requested */
    std::size_t bytes=0;unsigned vertices=0,triangles=0;
    std::uint32_t caster=0; /* out: ready caster id; its draws leave the replay path */
    bool candidate=false;   /* out: still long enough to convert */
    std::size_t track=SIZE_MAX;
};
enum Removal : unsigned {Moved,Unseen,Range,InView,Rekey,Shape,Evicted,Failed,Cleared,RemovalCount};
inline const char* removalName(unsigned r){static const char* n[RemovalCount]={"moved","unseen","range","inView","rekey","shape","evicted","failed","cleared"};return r<RemovalCount?n[r]:"?";}
enum class Failure : unsigned {None,Empty,Evaluate,Alpha,Vertices,Bones,Allocation,Budget,Oversize,Upload}; /* Oversize, Upload: diagnostics only (the renderer's size gate, a failed upload) */
inline const char* failureName(Failure f){switch(f){case Failure::None:return "none";case Failure::Empty:return "empty";case Failure::Evaluate:return "evaluate";case Failure::Alpha:return "alpha";case Failure::Vertices:return "vertices";case Failure::Bones:return "bones";case Failure::Allocation:return "allocation";case Failure::Budget:return "budget";case Failure::Oversize:return "oversize(draws>maxDraws or vertices>4*maxVertices)";case Failure::Upload:return "upload";}return "?";}
struct Batch {std::uint32_t firstIndex=0,indexCount=0,material=0;};
// World-space caster mesh. Material 0 is always the opaque one (cutoff < 0).
struct Mesh {
    std::vector<NorthlightGI::WorldVertex> vertices;std::vector<std::uint32_t> indices;std::vector<Batch> batches;
    std::vector<NorthlightGI::WorldMaterial> materials;Vec3 low,high;unsigned bones=0,draws=0;
    std::size_t gpuBytes()const{std::size_t n=vertices.size()*sizeof(NorthlightGI::WorldVertex)+indices.size()*(vertices.size()<=65535?2:4);
        for(const auto& m:materials)if(m.alphaCutoff>=0)n+=std::size_t(std::max(1u,m.width))*std::max(1u,m.height)*4;return n;}
};
struct Job {std::uint32_t id=0;std::vector<NorthlightActorGeometry::Packet> packets;unsigned maxVertices=32768,bones=0;};
struct Result {std::uint32_t id=0;std::shared_ptr<const Mesh> mesh;Failure failure=Failure::None;double ms=0;unsigned bones=0;};

// Worker-side conversion: exact per-vertex CPU evaluation of the game's vertex
// program (same evaluator as GI actors and replay bounds), then view->world by
// the captured inverse view. Alpha-tested draws need exact UVs and a decoded
// texture, else the whole actor stays dynamic (a partial caster would drop
// the remainder's shadow). No D3D calls, no game pointers.
inline Result convert(const Job& job){
    const auto start=std::chrono::steady_clock::now();Result out;out.id=job.id;
    auto finish=[&](Failure f){out.failure=f;if(f!=Failure::None)out.mesh.reset();out.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return out;};
    try{
        auto mesh=std::make_shared<Mesh>();mesh->materials.emplace_back();mesh->materials[0].alphaCutoff=-1;mesh->materials[0].width=mesh->materials[0].height=0;
        mesh->low=Vec3(INFINITY,INFINITY,INFINITY);mesh->high=Vec3(-INFINITY,-INFINITY,-INFINITY);
        std::vector<std::uint32_t> opaque;std::vector<std::pair<std::uint32_t,std::vector<std::uint32_t>>> cutout; /* material, indices */
        std::vector<std::uint32_t> remap;
        thread_local NorthlightActorDeformation::PreparedBindingCache bindingCache;
        for(const auto& packet:job.packets){
            const auto& m=packet.meshView();if(!m.primitiveCount||m.primitiveCount>87380||!m.vertexCount||m.vertexCount>262144)return finish(Failure::Empty);
            if(m.topology!=D3DPT_TRIANGLELIST&&m.topology!=D3DPT_TRIANGLESTRIP)return finish(Failure::Empty);
            const unsigned required=m.topology==D3DPT_TRIANGLELIST?m.primitiveCount*3:m.primitiveCount+2;
            if(m.indexed?m.indices.size()<required:m.vertexCount<required)return finish(Failure::Empty);
            auto index=[&](UINT i){return m.indexed?m.indices[i]:i;};
            std::vector<NorthlightActorDeformation::Position> positions;
            auto bindings=bindingCache.acquire(packet.position,m,packet.elements.data(),packet.elements.size());
            if(!bindings||!NorthlightActorDeformation::worldPositionsPrepared(packet.position,m,*bindings,packet.constants.data(),packet.inverseView.data(),positions))return finish(Failure::Evaluate);
            std::vector<std::array<float,2>> uvs;std::uint32_t material=0;
            if(packet.alphaTest){
                auto uvBindings=packet.hasUV?bindingCache.acquire(packet.uv,m,packet.elements.data(),packet.elements.size()):nullptr;
                if(!uvBindings||!NorthlightActorDeformation::textureUVsPrepared(packet.uv,m,*uvBindings,packet.constants.data(),uvs))return finish(Failure::Alpha);
                NorthlightGI::WorldMaterial mat;mat.alphaCutoff=std::max(0.f,packet.material.alphaCutoff);mat.addressU=packet.material.addressU;mat.addressV=packet.material.addressV;
                const auto& t=packet.texture;
                if(!NorthlightActorTexture::decode(t.bytes.data(),t.bytes.size(),t.width,t.height,t.pitch,t.format,mat.rgba))return finish(Failure::Alpha);
                mat.width=t.width;mat.height=t.height;
                for(std::size_t k=1;k<mesh->materials.size()&&!material;++k){const auto& o=mesh->materials[k];
                    if(o.width==mat.width&&o.height==mat.height&&o.alphaCutoff==mat.alphaCutoff&&o.addressU==mat.addressU&&o.addressV==mat.addressV&&o.rgba==mat.rgba)material=std::uint32_t(k);}
                if(!material){material=std::uint32_t(mesh->materials.size());mesh->materials.push_back(std::move(mat));cutout.push_back({material,{}});}
            }
            // Referenced vertices only, in first-reference order.
            remap.assign(m.vertexCount,UINT32_MAX);
            auto emit=[&](std::uint32_t v)->std::uint32_t{auto& r=remap[v];if(r!=UINT32_MAX)return r;
                if(mesh->vertices.size()>=job.maxVertices)return UINT32_MAX;
                NorthlightGI::WorldVertex w;const auto& p=positions[v];w.position=Vec3(p.x,p.y,p.z);if(!uvs.empty()){w.u=uvs[v][0];w.v=uvs[v][1];}
                mesh->low=Vec3(std::min(mesh->low.x,p.x),std::min(mesh->low.y,p.y),std::min(mesh->low.z,p.z));mesh->high=Vec3(std::max(mesh->high.x,p.x),std::max(mesh->high.y,p.y),std::max(mesh->high.z,p.z));
                r=std::uint32_t(mesh->vertices.size());mesh->vertices.push_back(w);return r;};
            std::vector<std::uint32_t>* target=&opaque;if(material)for(auto& c:cutout)if(c.first==material){target=&c.second;break;}
            for(UINT t=0;t<m.primitiveCount;++t){UINT a,b,c;
                if(m.topology==D3DPT_TRIANGLELIST){a=index(t*3);b=index(t*3+1);c=index(t*3+2);}else{a=index(t);b=index(t+1);c=index(t+2);if(t&1)std::swap(a,b);}
                if(a>=positions.size()||b>=positions.size()||c>=positions.size()||a==b||b==c||c==a)continue;
                const std::uint32_t x=emit(a),y=emit(b),z=emit(c);if(x==UINT32_MAX||y==UINT32_MAX||z==UINT32_MAX)return finish(Failure::Vertices);
                target->push_back(x);target->push_back(y);target->push_back(z);}
        }
        if(opaque.empty()&&cutout.empty())return finish(Failure::Empty);
        mesh->bones=job.bones;mesh->draws=unsigned(job.packets.size());out.bones=job.bones;
        mesh->indices.reserve(opaque.size()+[&]{std::size_t n=0;for(auto& c:cutout)n+=c.second.size();return n;}());
        auto batch=[&](std::uint32_t material,const std::vector<std::uint32_t>& list){if(list.empty())return;Batch b;b.firstIndex=std::uint32_t(mesh->indices.size());b.indexCount=std::uint32_t(list.size());b.material=material;
            mesh->indices.insert(mesh->indices.end(),list.begin(),list.end());mesh->batches.push_back(b);};
        batch(0,opaque);for(const auto& c:cutout)batch(c.first,c.second);
        out.mesh=std::move(mesh);return finish(Failure::None);
    }catch(const std::bad_alloc&){return finish(Failure::Allocation);}
}
// Palette slots one vertex references: BLENDINDICES lanes [0,lanes) with weight
// > 0 (lanes 4: the four-weight blend), or lane x alone whatever the weights
// (lanes 1: the one-influence program reads no weight). False: an index the
// audited layout cannot address (not 0..74 integral).
template<class Use> bool referencedSlots(const NorthlightActorDeformation::Four& index,const NorthlightActorDeformation::Four& weight,unsigned lanes,Use use){
    for(unsigned l=0;l<lanes&&l<4;++l){if(lanes>1&&!(weight[l]>0))continue;const float i=index[l];if(!(i>=0&&i<=74&&std::floor(i)==i))return false;use(unsigned(i));}
    return true;
}
// Frustum test of the main camera (inverse view rows = camera axes/eye in world;
// projection = xScale, yScale, sign of view z). True only if every box corner is
// in front and inside 90% of the screen, and the center is within `range`.
inline bool fullyOnScreen(const float* inverseView,const float* projection,Vec3 low,Vec3 high,float range){
    if(!inverseView||!projection)return false;
    for(unsigned i=0;i<16;++i)if(!std::isfinite(inverseView[i]))return false;
    for(unsigned i=0;i<3;++i)if(!std::isfinite(projection[i])||std::fabs(projection[i])<1e-4f)return false;
    const float* eye=inverseView+12;const float c[3]={(low.x+high.x)*.5f-eye[0],(low.y+high.y)*.5f-eye[1],(low.z+high.z)*.5f-eye[2]};
    if(!(c[0]*c[0]+c[1]*c[1]+c[2]*c[2]<=range*range))return false;
    for(unsigned k=0;k<8;++k){const float p[3]={(k&1?high.x:low.x)-eye[0],(k&2?high.y:low.y)-eye[1],(k&4?high.z:low.z)-eye[2]};
        const float x=p[0]*inverseView[0]+p[1]*inverseView[1]+p[2]*inverseView[2],y=p[0]*inverseView[4]+p[1]*inverseView[5]+p[2]*inverseView[6];
        const float f=(p[0]*inverseView[8]+p[1]*inverseView[9]+p[2]*inverseView[10])*(projection[2]<0?-1.f:1.f);
        if(!(f>1.f)||!(std::fabs(x*projection[0])<=.9f*f)||!(std::fabs(y*projection[1])<=.9f*f))return false;}
    return true;
}
// Recorded content of one rendered cache slot: ids and bounds of the casters drawn.
struct Content {struct Slice {std::uint32_t id=0;Vec3 low,high;};bool valid=false;std::uint64_t epoch=0;float matrix[16]={};std::vector<Slice> slices;};
inline bool committed(const Content& c,std::uint32_t id){if(!c.valid||!id)return false;auto it=std::lower_bound(c.slices.begin(),c.slices.end(),id,[](const Content::Slice& s,std::uint32_t k){return s.id<k;});return it!=c.slices.end()&&it->id==id;}
// Registry::frame()'s track order (0.3.150): items [0,old) plus new ones at the end
// are ordered by (key, at[0]) (the order nearest() searches), and above `capacity` the
// first `capacity` by rank (casters, then the most recently seen) are kept, in that
// order. orderTracksInPlace() is the 0.3.149 code: std::sort of the new items,
// inplace_merge, a full std::sort when a moved root broke the order, stable_sort by
// rank, resize, std::sort. orderTracks() runs the same steps on compact slots (key,
// at[0], rank, index) over the same sequences with the same comparator results and
// moves the items once. It is identical, ties included (nearest() takes the last of
// equal distances): std::sort's choices depend only on comparator results and
// positions (libc++'s type-dependent branchless path needs an arithmetic type and
// std::less; these are lambdas), stable_sort's result is unique, and so is
// inplace_merge's while [0,old) is ordered. With a moved root in [0,old) the merge's
// output depends on its temporary buffer (sized in elements), so that merge runs on
// the items as before; so does everything if the slots cannot be allocated.
struct TrackSlot {std::uint64_t key=0,rank=0;float x=0;std::uint32_t index=0;};
template<class T> bool trackBefore(const T& a,const T& b){return a.key<b.key||(a.key==b.key&&a.at[0]<b.at[0]);}
template<class T,class Rank> void orderTracksInPlace(std::vector<T>& items,std::size_t old,std::size_t capacity,Rank rank){
    std::sort(items.begin()+std::ptrdiff_t(old),items.end(),trackBefore<T>);std::inplace_merge(items.begin(),items.begin()+std::ptrdiff_t(old),items.end(),trackBefore<T>);
    for(std::size_t i=1;i<items.size();++i)if(trackBefore(items[i],items[i-1])){std::sort(items.begin(),items.end(),trackBefore<T>);break;} /* roots moved: restore order */
    if(items.size()>capacity){std::stable_sort(items.begin(),items.end(),[&](const T& a,const T& b){return rank(a)<rank(b);});items.resize(capacity);std::sort(items.begin(),items.end(),trackBefore<T>);}
}
template<class T,class Rank> void orderTracks(std::vector<T>& items,std::size_t old,std::size_t capacity,Rank rank,std::vector<TrackSlot>& slots){
    const std::size_t n=items.size();
    bool ordered=true;for(std::size_t i=1;i<old&&ordered;++i)ordered=!trackBefore(items[i],items[i-1]);
    if(old==n&&n<=capacity&&ordered)return; /* nothing new, no moved root, room: every step is a no-op */
    try{slots.resize(n);}catch(const std::bad_alloc&){orderTracksInPlace(items,old,capacity,rank);return;}
    const bool merged=!ordered&&old<n; /* the merge on the items (above) */
    if(merged){std::sort(items.begin()+std::ptrdiff_t(old),items.end(),trackBefore<T>);std::inplace_merge(items.begin(),items.begin()+std::ptrdiff_t(old),items.end(),trackBefore<T>);}
    for(std::size_t i=0;i<n;++i){auto& s=slots[i];s.key=items[i].key;s.x=items[i].at[0];s.index=std::uint32_t(i);}
    auto before=[](const TrackSlot& a,const TrackSlot& b){return a.key<b.key||(a.key==b.key&&a.x<b.x);};
    if(!merged){std::sort(slots.begin()+std::ptrdiff_t(old),slots.end(),before);std::inplace_merge(slots.begin(),slots.begin()+std::ptrdiff_t(old),slots.end(),before);}
    for(std::size_t i=1;i<n;++i)if(before(slots[i],slots[i-1])){std::sort(slots.begin(),slots.end(),before);break;} /* roots moved: restore order */
    std::size_t size=n;
    if(n>capacity){for(auto& s:slots)s.rank=rank(items[s.index]);
        std::stable_sort(slots.begin(),slots.end(),[](const TrackSlot& a,const TrackSlot& b){return a.rank<b.rank;});
        size=capacity;std::sort(slots.begin(),slots.begin()+std::ptrdiff_t(size),before);} /* the dropped ones stay behind, in any order */
    // Position k receives items[slots[k].index]: one move per displaced item along each cycle.
    for(std::size_t i=0;i<n;++i){if(slots[i].index==i)continue;T held=std::move(items[i]);std::size_t j=i;
        for(;;){const std::size_t k=slots[j].index;slots[j].index=std::uint32_t(j);if(k==i){items[j]=std::move(held);break;}items[j]=std::move(items[k]);j=k;}}
    items.resize(size);
}
struct Statistics {
    std::uint64_t submitted=0,converted=0,rejectedBones=0,rejectedAlpha=0,rejectedOther=0,deferredAttach=0,deferredBudget=0,uploads=0,publications=0;
    std::uint64_t removed[RemovalCount]={};double workerMs=0,workerPeakMs=0;unsigned tracks=0,attachBlocked=0,poseStill=0;
};
class Registry {
public:
    enum class State : unsigned char {Pending,Converted,Ready};
    struct Entry {std::uint32_t id=0;State state=State::Pending;std::uint64_t key=0;float root[3]={},axes[9]={},vertex[3]={};bool hasVertex=false;
        Vec3 low,high;std::size_t bytes=0;unsigned draws=0,bones=0,absentInView=0,absentSinceMs=0,lastSeenMs=0,triangles=0;bool matched=false,published=false;float distanceSquared=0;
        std::vector<float> pose; /* world bone matrices at conversion */
        std::shared_ptr<const Mesh> mesh;};
private:
    struct Track {std::uint64_t key=0;float at[3]={},seen[3]={},axes[9]={},anchor[3]={},anchorAxes[9]={},origin[3]={},originAxes[9]={};
        unsigned stillFrames=0,stillSinceMs=0,lastSeenMs=0,notBeforeMs=0,poseFrames=0,poseSinceMs=0;std::size_t maxCount=0;std::uint32_t entry=0;bool used=false,mobile=false,rejected=false,attachBlocked=false;
        std::uint32_t serial=0; /* track id (tracksNear) */
        std::vector<float> pose;};
public:
    struct Event {std::uint32_t id=0;unsigned reason=0;float root[3]={};bool ready=false,rejected=false;unsigned bones=0;}; /* a removal, or (rejected) a character-like candidate */
private:
    std::vector<Event> events_;
    Tuning tuning_;std::vector<Track> tracks_,fresh_;std::vector<std::size_t> unmatched_;std::vector<TrackSlot> slots_;std::vector<Entry> entries_;std::vector<std::uint32_t> removedIds_;
    std::uint32_t nextId_=1;std::uint64_t epoch_=1;unsigned lastEvictMs_=0,lastPublishMs_=0;bool evicted_=false;std::size_t used_=0;
    Statistics stats_;
    std::uint32_t nextSerial_=1;
    static float distance2(const float* a,const float* b){float q=0;for(unsigned k=0;k<3;++k){const float t=a[k]-b[k];q+=t*t;}return q;}
    static float axesDifference(const float* a,const float* b){float m=0;for(unsigned k=0;k<9;++k)m=std::max(m,std::fabs(a[k]-b[k]));return m;}
    static float axesScale(const float* a){float m=1;for(unsigned k=0;k<9;++k)m=std::max(m,std::fabs(a[k]));return m;}
    // Every bone's world origin within poseTolerance and basis within axisTolerance.
    bool poseStill(const float* a,const float* b,unsigned bones)const{
        for(unsigned n=0;n<bones;++n){const float* x=a+12*n;const float* y=b+12*n;float scale=1;for(unsigned k=0;k<9;++k)scale=std::max(scale,std::fabs(y[k]));
            for(unsigned k=0;k<9;++k)if(!(std::fabs(x[k]-y[k])<=tuning_.axisTolerance*scale))return false;
            if(!(distance2(x+9,y+9)<=tuning_.poseTolerance*tuning_.poseTolerance))return false;}
        return true;
    }
    static bool expired(unsigned now,unsigned then,unsigned ms){return unsigned(now-then)>=ms;}
    Track* nearest(std::uint64_t key,const float* at){
        auto it=std::lower_bound(tracks_.begin(),tracks_.end(),std::make_pair(key,at[0]-tuning_.identityTolerance),[](const Track& t,const std::pair<std::uint64_t,float>& k){return t.key<k.first||(t.key==k.first&&t.at[0]<k.second);});
        Track* best=nullptr;float bestSquared=tuning_.identityTolerance*tuning_.identityTolerance;
        for(;it!=tracks_.end()&&it->key==key&&it->at[0]<=at[0]+tuning_.identityTolerance;++it){if(it->used)continue;const float d=distance2(it->at,at);if(d<=bestSquared){bestSquared=d;best=&*it;}}
        return best;
    }
    Entry* entry(std::uint32_t id){auto it=std::lower_bound(entries_.begin(),entries_.end(),id,[](const Entry& e,std::uint32_t k){return e.id<k;});return it!=entries_.end()&&it->id==id?&*it:nullptr;}
    const Entry* entry(std::uint32_t id)const{return const_cast<Registry*>(this)->entry(id);}
    void unlink(std::uint32_t id,unsigned notBefore,bool mobile){for(auto& t:tracks_)if(t.entry==id){t.entry=0;t.notBeforeMs=notBefore;t.mobile=t.mobile||mobile;t.stillFrames=0;t.stillSinceMs=notBefore;std::memcpy(t.anchor,t.at,sizeof t.anchor);std::memcpy(t.anchorAxes,t.axes,sizeof t.anchorAxes);}}
    void erase(std::size_t i,Removal why,unsigned now){
        const auto id=entries_[i].id;used_-=std::min(used_,entries_[i].bytes);if(entries_[i].state==State::Ready)removedIds_.push_back(id);
        Event v;v.id=id;v.reason=why;std::memcpy(v.root,entries_[i].root,12);v.ready=entries_[i].state==State::Ready;if(events_.size()<256)events_.push_back(v);
        ++stats_.removed[why];unlink(id,now+(why==Moved||why==Shape||why==Rekey?tuning_.deferMs:tuning_.cooldownMs),why==Moved);
        entries_.erase(entries_.begin()+std::ptrdiff_t(i));}
public:
    explicit Registry(Tuning t=Tuning{}):tuning_(t){}
    // Distinct bone matrices of a pose (identical matrices at several palette
    // slots count once): the character-size measure.
    static unsigned distinctMatrices(const float* pose,unsigned bones){unsigned n=0;
        for(unsigned i=0;i<bones;++i){bool seen=false;for(unsigned j=0;j<i&&!seen;++j)seen=std::memcmp(pose+12*i,pose+12*j,48)==0;n+=!seen;}return n;}
    // Every referenced vertex driven by one matrix (distinctMatrices()==1, O(bones)).
    static bool singleMatrix(const float* pose,unsigned bones){if(!pose||!bones)return false;for(unsigned i=1;i<bones;++i)if(std::memcmp(pose,pose+12*i,48)!=0)return false;return true;}
    // Class (quality settings). A change returns everything to the replay path.
    void classes(bool general){if(general==tuning_.general)return;clear();tuning_.general=general;}
    // Read-only: should the renderer compute this actor's pose this frame?
    // (root still for poseAfterFrames, or a caster: its pose is watched).
    bool needsPose(std::uint64_t key,const float* root)const{
        auto it=std::lower_bound(tracks_.begin(),tracks_.end(),std::make_pair(key,root[0]-tuning_.identityTolerance),[](const Track& t,const std::pair<std::uint64_t,float>& k){return t.key<k.first||(t.key==k.first&&t.at[0]<k.second);});
        const Track* t=nullptr;float bestSquared=tuning_.identityTolerance*tuning_.identityTolerance; /* the nearest, as frame() matches */
        for(;it!=tracks_.end()&&it->key==key&&it->at[0]<=root[0]+tuning_.identityTolerance;++it){const float d=distance2(it->at,root);if(d<=bestSquared){bestSquared=d;t=&*it;}}
        return t&&(t->entry||(!t->mobile&&!t->rejected&&tuning_.general&&t->stillFrames+1>=tuning_.poseAfterFrames));
    }
    const Tuning& tuning()const{return tuning_;}
    const Statistics& stats()const{return stats_;}
    // Every track within `radius` of `at` (a full scan, for tests): f(key,root,serial).
    template<class F> void tracksNear(const float* at,float radius,F f)const{for(const auto& t:tracks_)if(distance2(t.at,at)<=radius*radius)f(t.key,t.at,t.serial);}
    std::size_t usedBytes()const{return used_;}
    const std::vector<Entry>& entries()const{return entries_;}
    std::uint64_t epoch()const{return epoch_;}
    // Ready ids removed since the last call (GPU resources to release).
    void takeRemoved(std::vector<std::uint32_t>& out){out.swap(removedIds_);removedIds_.clear();}
    // Removals since the last call (diagnostics; at most 256 kept).
    void takeEvents(std::vector<Event>& out){out.swap(events_);events_.clear();}
    std::size_t count(State s)const{std::size_t n=0;for(const auto& e:entries_)n+=e.state==s;return n;}
    // Map change, device loss, disable: everything returns to the replay path.
    void clear(){for(const auto& e:entries_)if(e.state==State::Ready)removedIds_.push_back(e.id);stats_.removed[Cleared]+=entries_.size();entries_.clear();tracks_.clear();used_=0;++epoch_;}
    // One frame. `complete`: no draw was lost to capture limits this frame (only
    // then is an on-screen absence real).
    // Fills obs[].caster (ready casters whose draws leave the replay path) and
    // obs[].candidate; returns candidate indices nearest (to the pivot) first.
    void frame(std::vector<Observation>& obs,unsigned now,const float* pivot,const float* inverseView,const float* projection,bool complete,std::vector<std::size_t>& candidates){
        candidates.clear();evicted_=false;
        for(auto& t:tracks_)t.used=false;for(auto& e:entries_)e.matched=false;fresh_.clear();
        const float still2=tuning_.stillTolerance*tuning_.stillTolerance,move2=tuning_.moveTolerance*tuning_.moveTolerance,travel2=tuning_.travelTolerance*tuning_.travelTolerance;
        auto& unmatched=unmatched_;unmatched.clear();
        for(std::size_t n=0;n<obs.size();++n){auto& o=obs[n];o.caster=0;o.candidate=false;o.track=SIZE_MAX;
            Track* t=nearest(o.key,o.root);
            if(!t){Track f;f.key=o.key;std::memcpy(f.at,o.root,12);std::memcpy(f.anchor,o.root,12);std::memcpy(f.origin,o.root,12);std::memcpy(f.axes,o.axes,36);std::memcpy(f.anchorAxes,o.axes,36);std::memcpy(f.originAxes,o.axes,36);
                f.lastSeenMs=now;f.stillSinceMs=f.poseSinceMs=now;f.notBeforeMs=now;f.maxCount=o.count;f.used=true;f.serial=nextSerial_++;if(!nextSerial_)nextSerial_=1;
                if(o.pose&&o.poseBones)f.pose.assign(o.pose,o.pose+std::size_t(12)*o.poseBones);fresh_.push_back(f);unmatched.push_back(n);continue;}
            t->used=true;o.track=std::size_t(t-tracks_.data());std::memcpy(t->seen,o.root,12); /* `at` keeps the search order until the loop ends */std::memcpy(t->axes,o.axes,36);t->lastSeenMs=now;
            const float scale=axesScale(o.axes);
            if(distance2(o.root,t->anchor)<=still2&&axesDifference(o.axes,t->anchorAxes)<=tuning_.axisTolerance*scale){++t->stillFrames;t->maxCount=std::max(t->maxCount,o.count);}
            else{t->stillFrames=0;t->stillSinceMs=now;t->maxCount=o.count;std::memcpy(t->anchor,o.root,12);std::memcpy(t->anchorAxes,o.axes,36);}
            if(distance2(o.root,t->origin)>travel2||axesDifference(o.axes,t->originAxes)>tuning_.turnTolerance*scale)t->mobile=true;
            if(o.pose&&o.poseBones){if(t->pose.size()==std::size_t(12)*o.poseBones&&poseStill(o.pose,t->pose.data(),o.poseBones))++t->poseFrames;
                else{t->pose.assign(o.pose,o.pose+std::size_t(12)*o.poseBones);t->poseFrames=0;t->poseSinceMs=now;}}
            else{t->pose.clear();t->poseFrames=0;}
            if(!t->entry)continue;
            auto* e=entry(t->entry);if(!e){t->entry=0;continue;}
            e->matched=true;e->lastSeenMs=now;e->absentInView=0;
            const std::size_t index=std::size_t(e-entries_.data());
            if(distance2(o.root,e->root)>move2||axesDifference(o.axes,e->axes)>tuning_.axisTolerance*scale){erase(index,Moved,now);continue;}
            if(o.count>e->draws&&e->state!=State::Pending){erase(index,Shape,now);continue;} /* more parts than converted: reconvert whole */
            // Any bone moving (animation starting, a door, a turn) returns it to the replays now.
            if(o.pose&&o.poseBones){if(e->pose.size()!=std::size_t(12)*o.poseBones){erase(index,Shape,now);continue;}if(!poseStill(o.pose,e->pose.data(),o.poseBones)){erase(index,Moved,now);continue;}}
            if(e->state==State::Ready&&e->published)o.caster=e->id;
        }
        // A NEW identity at an unmatched caster's root is that object under a new
        // key (LOD or snapshot change): the caster no longer describes it. Known
        // identities there (a co-located separate model) never remove it.
        for(std::size_t k=0;k<unmatched.size();++k){const auto n=unmatched[k];for(std::size_t i=0;i<entries_.size();++i){auto& e=entries_[i];
            if(!e.matched&&distance2(obs[n].root,e.root)<=move2&&e.key!=obs[n].key){erase(i,Rekey,now);break;}}}
        for(std::size_t i=0;i<entries_.size();){auto& e=entries_[i];
            e.distanceSquared=pivot?distance2(e.root,pivot):0;
            if(e.distanceSquared>tuning_.range*tuning_.range){erase(i,Range,now);continue;}
            if(!e.matched){
                if(expired(now,e.lastSeenMs,tuning_.unseenMs)){erase(i,Unseen,now);continue;}
                // Absent while fully on screen and near (a despawn). A frame with a capture
                // shortfall HOLDS both the count and its start time (the object may be one
                // of the dropped draws); only complete frames count or reset it.
                if(complete){
                    if(e.state!=State::Pending&&fullyOnScreen(inverseView,projection,e.low,e.high,tuning_.viewRange)){if(!e.absentInView++)e.absentSinceMs=now;
                        if(e.absentInView>=tuning_.absentInView&&expired(now,e.absentSinceMs,tuning_.absentInViewMs)){erase(i,InView,now);continue;}}
                    else e.absentInView=0;}}
            ++i;}
        // Batched publication: new ready casters change cache signatures together.
        bool unpublished=false;for(const auto& e:entries_)unpublished=unpublished||(e.state==State::Ready&&!e.published);
        if(unpublished&&expired(now,lastPublishMs_,tuning_.publishMs)){for(auto& e:entries_)if(e.state==State::Ready)e.published=true;lastPublishMs_=now;++stats_.publications;}
        for(auto& t:tracks_)if(t.used)std::memcpy(t.at,t.seen,12);
        // Merge new tracks; forget tracks unseen for long (never one with a caster).
        std::size_t write=0;for(std::size_t i=0;i<tracks_.size();++i){auto& t=tracks_[i];if(!t.entry&&expired(now,t.lastSeenMs,tuning_.unseenMs))continue;if(write!=i)tracks_[write]=std::move(t);++write;}
        tracks_.resize(write);
        const std::size_t old=tracks_.size();tracks_.insert(tracks_.end(),fresh_.begin(),fresh_.end());
        // (key, x) order after new tracks and moved roots; above trackCapacity keep casters and the most recently seen.
        orderTracks(tracks_,old,tuning_.trackCapacity,[&](const Track& t){return (t.entry?std::uint64_t(0):std::uint64_t(1)<<32)|unsigned(now-t.lastSeenMs);},slots_);
        stats_.tracks=unsigned(tracks_.size());stats_.attachBlocked=0;stats_.poseStill=0;
        for(const auto& t:tracks_){stats_.attachBlocked+=t.attachBlocked&&!t.entry&&t.lastSeenMs==now;stats_.poseStill+=t.poseFrames>=tuning_.stillFrames&&t.lastSeenMs==now;}
        // Track pointers moved: relink observations, then collect candidates.
        for(auto& o:obs){o.track=SIZE_MAX;
            auto it=std::lower_bound(tracks_.begin(),tracks_.end(),std::make_pair(o.key,o.root[0]),[](const Track& t,const std::pair<std::uint64_t,float>& k){return t.key<k.first||(t.key==k.first&&t.at[0]<k.second);});
            for(;it!=tracks_.end()&&it->key==o.key&&it->at[0]==o.root[0];++it)if(distance2(it->at,o.root)==0&&it->lastSeenMs==now){o.track=std::size_t(it-tracks_.begin());break;}
            if(o.track==SIZE_MAX)continue;const auto& t=tracks_[o.track];
            // Convert on a frame that holds every part seen while still (a capture
            // shortfall drops the late draws of a crowded frame); a larger later
            // group is caught by the Shape rule and reconverted whole.
            if(t.entry||t.mobile||t.rejected||t.stillFrames<tuning_.stillFrames||!expired(now,t.stillSinceMs,tuning_.stillMs)||o.count<t.maxCount||unsigned(now-t.notBeforeMs)>0x7fffffffu)continue;
            if(!o.pose||t.pose.size()!=std::size_t(12)*o.poseBones||t.poseFrames<tuning_.stillFrames||!expired(now,t.poseSinceMs,tuning_.stillMs))continue; /* the full pose too */
            if(pivot&&o.distanceSquared>tuning_.range*tuning_.range)continue;
            if(!tuning_.general)continue;
            const unsigned bones=distinctMatrices(t.pose.data(),o.poseBones);
            if(bones>=tuning_.characterBones){tracks_[o.track].rejected=true;++stats_.rejectedBones;
                if(events_.size()<256){Event v;v.rejected=true;v.bones=bones;std::memcpy(v.root,o.root,12);events_.push_back(v);}continue;}
            o.candidate=true;candidates.push_back(std::size_t(&o-obs.data()));}
        std::sort(candidates.begin(),candidates.end(),[&](std::size_t a,std::size_t b){return obs[a].distanceSquared!=obs[b].distanceSquared?obs[a].distanceSquared<obs[b].distanceSquared:a<b;});
    }
    // A rigid candidate beside a non-rigid actor that is not a persistent
    // structure is presumably held/worn: wait (deferMs) instead of converting.
    // The track remembers the answer (logged: attachBlocked = tracks kept out now).
    template<class Rigid> bool attachedToActor(const std::vector<Observation>& obs,std::size_t n,Rigid rigid){
        bool blocked=false;
        if(rigid(n)){const float r2=tuning_.attachRadius*tuning_.attachRadius;
            for(std::size_t k=0;k<obs.size()&&!blocked;++k)blocked=k!=n&&!obs[k].caster&&distance2(obs[k].root,obs[n].root)<=r2&&!rigid(k);}
        if(obs[n].track<tracks_.size())tracks_[obs[n].track].attachBlocked=blocked;
        return blocked;
    }
    void defer(const Observation& o,unsigned now,bool budget){if(o.track<tracks_.size())tracks_[o.track].notBeforeMs=now+tuning_.deferMs;if(budget)++stats_.deferredBudget;else ++stats_.deferredAttach;}
    void reject(const Observation& o,unsigned now,bool permanent,Failure why=Failure::Empty){if(o.track<tracks_.size()){auto& t=tracks_[o.track];t.notBeforeMs=now+tuning_.cooldownMs;t.rejected=t.rejected||permanent;}
        if(why==Failure::Alpha)++stats_.rejectedAlpha;else ++stats_.rejectedOther;}
    // Room for `estimate` bytes: evict the farthest ready caster when this one is
    // clearly nearer (margin) and the eviction rate allows it. 0: no room.
    // begin() would succeed (possibly by evicting): checked before copying a job.
    // skip: an entry counted as already evicted.
    bool fits(std::size_t estimate,std::size_t skip=SIZE_MAX)const{
        std::size_t bytes=used_,count=entries_.size();
        if(skip<entries_.size()){bytes-=std::min(bytes,entries_[skip].bytes);--count;}
        return bytes+estimate<=tuning_.budgetBytes&&count<tuning_.maxCasters;
    }
    std::size_t victim(const Observation& o,unsigned now)const{
        std::size_t farthest=SIZE_MAX;for(std::size_t i=0;i<entries_.size();++i)if(entries_[i].state==State::Ready&&(farthest==SIZE_MAX||entries_[i].distanceSquared>entries_[farthest].distanceSquared))farthest=i;
        if(farthest==SIZE_MAX||evicted_||!expired(now,lastEvictMs_,tuning_.evictIntervalMs)||!(o.distanceSquared<entries_[farthest].distanceSquared*tuning_.evictMargin*tuning_.evictMargin))return SIZE_MAX;
        return farthest;
    }
    bool room(const Observation& o,std::size_t estimate,unsigned now)const{
        if(o.track>=tracks_.size()||estimate>tuning_.budgetBytes)return false;
        if(fits(estimate))return true;
        const std::size_t v=victim(o,now);return v!=SIZE_MAX&&fits(estimate,v);
    }
    std::uint32_t begin(const Observation& o,std::size_t estimate,unsigned now,const float* vertex){
        if(o.track>=tracks_.size()||estimate>tuning_.budgetBytes)return 0;
        while(!fits(estimate)){const std::size_t v=victim(o,now);if(v==SIZE_MAX)return 0;
            erase(v,Evicted,now);lastEvictMs_=now;evicted_=true;}
        Entry e;e.id=nextId_++;if(!nextId_)nextId_=1;e.key=o.key;std::memcpy(e.root,o.root,12);std::memcpy(e.axes,o.axes,36);
        if(vertex){std::memcpy(e.vertex,vertex,12);e.hasVertex=true;}
        const auto& t=tracks_[o.track];e.pose=t.pose;e.bones=o.poseBones?distinctMatrices(t.pose.data(),unsigned(t.pose.size()/12)):0;
        e.bytes=estimate;e.draws=unsigned(o.count);e.lastSeenMs=now;e.matched=true;e.distanceSquared=o.distanceSquared;
        e.triangles=o.triangles;used_+=estimate;tracks_[o.track].entry=e.id;entries_.push_back(e);++stats_.submitted;return e.id;
    }
    // Worker result. False: dropped (entry gone or conversion failed).
    bool accept(Result&& r,unsigned now){
        stats_.workerMs+=r.ms;stats_.workerPeakMs=std::max(stats_.workerPeakMs,r.ms);
        auto* e=entry(r.id);if(!e||e->state!=State::Pending)return false;const std::size_t i=std::size_t(e-entries_.data());
        if(r.failure!=Failure::None||!r.mesh){
            const bool permanent=r.failure==Failure::Alpha||r.failure==Failure::Vertices;
            if(r.failure==Failure::Alpha)++stats_.rejectedAlpha;else ++stats_.rejectedOther;
            for(auto& t:tracks_)if(t.entry==r.id)t.rejected=t.rejected||permanent;
            erase(i,Failed,now);return false;}
        const std::size_t bytes=r.mesh->gpuBytes();
        if(used_-std::min(used_,e->bytes)+bytes>tuning_.budgetBytes){++stats_.deferredBudget;erase(i,Failed,now);return false;}
        used_=used_-std::min(used_,e->bytes)+bytes;e->bytes=bytes;e->low=r.mesh->low;e->high=r.mesh->high;e->mesh=std::move(r.mesh);e->state=State::Converted;++stats_.converted;return true;
    }
    // Next converted mesh awaiting upload, nearest first (null: none).
    const Entry* nextUpload()const{const Entry* best=nullptr;for(const auto& e:entries_)if(e.state==State::Converted&&(!best||e.distanceSquared<best->distanceSquared))best=&e;return best;}
    void ready(std::uint32_t id,std::size_t gpuBytes){auto* e=entry(id);if(!e||e->state!=State::Converted)return;used_=used_-std::min(used_,e->bytes)+gpuBytes;e->bytes=gpuBytes;e->mesh.reset();e->state=State::Ready;++stats_.uploads;}
    void uploadFailed(std::uint32_t id,unsigned now){auto* e=entry(id);if(e){for(auto& t:tracks_)if(t.entry==id)t.rejected=true;erase(std::size_t(e-entries_.data()),Failed,now);}}
    // Sampled vertex check for a matched ready caster (renderer, round robin).
    void vertex(std::uint32_t id,const float* at,unsigned now){auto* e=entry(id);if(!e||!e->hasVertex||!at)return;
        if(distance2(e->vertex,at)>tuning_.vertexTolerance*tuning_.vertexTolerance)erase(std::size_t(e-entries_.data()),Moved,now);}
    // Published ready casters a directional light matrix can see, in id order.
    template<class F> void forVisible(const float* matrix,F f)const{const bool affine=NorthlightShadowBounds::affineLightMatrix(matrix);
        for(const auto& e:entries_)if(e.state==State::Ready&&e.published&&!(affine&&NorthlightShadowBounds::directionalClipRejectAffine(e.low,e.high,matrix)))f(e);}
    // 0 while nothing is visible (an empty registry never forces a cache redraw).
    std::uint64_t signature(const float* matrix)const{std::uint64_t h=1469598103934665603ull;bool any=false;auto mix=[&](std::uint64_t v){h=(h^v)*1099511628211ull;};mix(epoch_);
        forVisible(matrix,[&](const Entry& e){mix(e.id);any=true;});return any?h|1:0;}
    void record(const float* matrix,Content& out)const{out.slices.clear();out.epoch=epoch_;std::memcpy(out.matrix,matrix,sizeof out.matrix);forVisible(matrix,[&](const Entry& e){out.slices.push_back({e.id,e.low,e.high});});out.valid=true;}
    // Boxes of casters added or removed since `old` (appended). False: unknown, draw all.
    template<class Out> bool changedBounds(const float* matrix,const Content& old,Out out)const{
        if(!old.valid||old.epoch!=epoch_||std::memcmp(old.matrix,matrix,sizeof old.matrix)!=0)return false;auto a=old.slices.begin();
        forVisible(matrix,[&](const Entry& e){while(a!=old.slices.end()&&a->id<e.id){out(a->low,a->high);++a;}if(a!=old.slices.end()&&a->id==e.id){++a;return;}out(e.low,e.high);});
        for(;a!=old.slices.end();++a)out(a->low,a->high);
        return true;
    }
};
// One background thread; at most `capacity` queued jobs; results polled by the
// render thread. Never touches D3D or game memory.
class Worker {
    std::thread thread_;std::mutex mutex_;std::condition_variable wake_;std::deque<std::shared_ptr<const Job>> queue_;std::vector<Result> done_;bool stopping_=false;unsigned busy_=0;
    void loop(){for(;;){std::shared_ptr<const Job> job;
        {std::unique_lock<std::mutex> lock(mutex_);wake_.wait(lock,[&]{return stopping_||!queue_.empty();});if(stopping_)return;job=std::move(queue_.front());queue_.pop_front();++busy_;}
        Result r;try{r=convert(*job);}catch(...){r.id=job->id;r.failure=Failure::Allocation;}
        job.reset(); /* drop snapshot/constant copies before publishing */
        std::lock_guard<std::mutex> lock(mutex_);--busy_;try{done_.push_back(std::move(r));}catch(...){}}}
public:
    explicit Worker(std::function<void()> init=nullptr){thread_=std::thread([this,init]{if(init)init();loop();});}
    ~Worker(){{std::lock_guard<std::mutex> lock(mutex_);stopping_=true;}wake_.notify_one();if(thread_.joinable())thread_.join();}
    Worker(const Worker&)=delete;Worker& operator=(const Worker&)=delete;
    std::size_t inFlight(){std::lock_guard<std::mutex> lock(mutex_);return queue_.size()+busy_+done_.size();}
    void submit(std::shared_ptr<const Job> job){{std::lock_guard<std::mutex> lock(mutex_);queue_.push_back(std::move(job));}wake_.notify_one();}
    void collect(std::vector<Result>& out){out.clear();std::lock_guard<std::mutex> lock(mutex_);out.swap(done_);}
};
} // namespace NorthlightPersistentCasters
