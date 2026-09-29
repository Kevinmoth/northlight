// 0.3.177 (r83 a1-prepare): prepare_worker.h. prepareRecord() over random capture frames with the real
// SampledVertexCache/RigidBoneCache, synthetic one-influence palette programs (test_actor_deformation's
// generator) and the client's one-influence and four-bone programs: groups of 1-8 draws, shared and owned
// snapshots, rigid and multi-bone meshes, unknown programs, undeclared layouts, repeated shader and
// declaration, declaration copies taken through the real 128-slot declaration cache.
//   - prepareRecord equals a verbatim copy of the 0.3.176 selectStableActors loop (draws, stored rigid
//     bones, distance counters) over the same records and the live declaration cache.
//   - Handover at every index (worker prefix, inline tail from the last output's state, the same caches)
//     equals fully inline, bit for bit, distance counters included; cold and pre-warmed caches agree.
//   - The threaded Worker: random publish delays, stops before the first publish, mid-record and after
//     completion, records never published (prepared by the joiner), the context camera changed after
//     the frame opened: equal to inline. An exception on record j and a stalled record (the watchdog)
//     are taken over inline with equal outputs.
//   - Counterfactuals that must fail: the carried state reset at the handover, unfiltered records
//     published, declaration elements read through the live cache after an eviction, the camera taken
//     at the join.
// Built by test_prepare_worker.py (O2, ASan+UBSan, TSan). Native, no game or GPU.
#include "prepare_worker.h"
#include "vertex_declaration_cache.h"
#include "actor_client_programs.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
using namespace NorthlightActorDeformation;
/*SYNTHETIC_PROGRAMS*/
using NorthlightDrawSnapshot::Mesh;using NorthlightActorPrepare::Output;using NorthlightActorPrepare::State;using NorthlightActorPrepare::Caches;
struct IDirect3DVertexShader9;
static Program compiled(const std::uint32_t* words,std::size_t n){Program p;bool ok=compile(words,n,p);assert(ok);(void)ok;return p;}
#define CLIENT_PROGRAM(name) compiled(ClientShaders::name,sizeof ClientShaders::name/4)
// A captured record as the renderer's Replay holds it (the fields prepareRecord reads).
struct Rec {
    unsigned constantGroup=0;IDirect3DVertexShader9* originalShader=nullptr;IDirect3DVertexDeclaration9* decl=nullptr;
    std::shared_ptr<const Mesh> shared;Mesh snapshot;const float* constants=nullptr;std::vector<float> storage;
    std::shared_ptr<const Program> program;std::array<D3DVERTEXELEMENT9,MAXD3DDECLLENGTH+1> elements{};UINT elementCount=0;bool declared=false;
    bool shadowSelected=true,shadowSkinned=true,boneKnown=false;float bone=NAN;
    const Mesh& mesh()const{return shared?*shared:snapshot;}
};
struct FakeDecl final:IDirect3DVertexDeclaration9 {
    unsigned refs=1;std::vector<D3DVERTEXELEMENT9> layout;
    unsigned AddRef()override{return ++refs;}unsigned Release()override{return --refs;}
    HRESULT GetDeclaration(D3DVERTEXELEMENT9* out,UINT* n)override{if(*n<layout.size())return D3DERR_INVALIDCALL;std::copy(layout.begin(),layout.end(),out);*n=UINT(layout.size());return D3D_OK;}
};
static const std::vector<D3DVERTEXELEMENT9> gameLayout={{0,0,2,0,0,0},{0,12,8,0,1,0},{0,16,5,0,2,0},{0xff,0,17,0,0,0}};
static std::vector<Word> load(const char* path){FILE* f=std::fopen(path,"rb");assert(f);std::fseek(f,0,SEEK_END);long size=std::ftell(f);std::rewind(f);
    std::vector<Word> w(size_t(size)/4);size_t got=std::fread(w.data(),1,size_t(size),f);assert(got==size_t(size));(void)got;std::fclose(f);return w;}
// Game layout mesh: float3 position, UBYTE4N weights, UBYTE4 indices; every vertex on `bone`, or (mixed)
// alternating bones with split weights.
static std::shared_ptr<Mesh> mesh(std::mt19937& rng,unsigned vertices,unsigned bone,bool mixed){
    auto m=std::make_shared<Mesh>();m->vertexCount=vertices;m->streams[0].stride=20;m->streams[0].bytes.resize(size_t(vertices)*20);
    for(unsigned v=0;v<vertices;++v){auto* out=m->streams[0].bytes.data()+size_t(v)*20;const float p[3]={float(int(rng()%2000)-1000)*.01f,float(int(rng()%2000)-1000)*.01f,float(rng()%300)*.01f};
        std::memcpy(out,p,12);const bool split=mixed&&v%2;out[12]=split?128:255;out[13]=split?127:0;out[14]=out[15]=0;
        out[16]=std::uint8_t(bone);out[17]=std::uint8_t(split?bone+1:bone);out[18]=out[19]=0;}
    m->indexed=true;m->topology=D3DPT_TRIANGLELIST;for(unsigned t=0;t+2<vertices;++t)for(unsigned k=0;k<3;++k)m->indices.push_back(t+k);
    m->primitiveCount=unsigned(m->indices.size()/3);return m;
}
struct World {
    std::vector<std::shared_ptr<const Program>> programs; /* per shader slot; null: not an actor program */
    std::vector<FakeDecl> decls=std::vector<FakeDecl>(2);
    NorthlightVertexDeclarations::Cache declarationCache;std::vector<std::shared_ptr<Mesh>> pool;
    static IDirect3DVertexShader9* shader(unsigned k){return reinterpret_cast<IDirect3DVertexShader9*>(std::uintptr_t(0x1000+32*k));}
    explicit World(const char* fourBone){
        for(unsigned major:{3u,2u,1u}){auto w=code(major);programs.push_back(std::make_shared<const Program>(compiled(w.data(),w.size())));}
        {auto w=load(fourBone);programs.push_back(std::make_shared<const Program>(compiled(w.data(),w.size())));}
        programs.push_back(std::make_shared<const Program>(CLIENT_PROGRAM(OneBoneVs3)));programs.push_back(nullptr);
        assert(NorthlightReplayBounds::SkinEnvelope::supports(*programs[3])&&programs[3]->paletteBase==31);
        decls[0].layout=gameLayout;decls[1].layout={{0,0,2,0,0,0},{0xff,0,17,0,0,0}}; /* no BLENDINDICES */
        std::mt19937 rng(177);for(unsigned i=0;i<48;++i)pool.push_back(mesh(rng,3+rng()%24,rng()%6,rng()%4==0));
    }
    // The capture-side fill (the renderer's prepareFill): the program handle and a copy of the declaration.
    void fill(Rec& r){
        r.program=programs[(reinterpret_cast<std::uintptr_t>(r.originalShader)-0x1000)/32];
        const D3DVERTEXELEMENT9* e=nullptr;UINT n=0;r.declared=r.program&&declarationCache.get(r.decl,e,n);
        if(r.declared){std::copy(e,e+n,r.elements.begin());r.elementCount=n;}else r.elementCount=0;
    }
    // A capture frame: constant groups of 1-8 draws in replays order; some neither selected nor skinned.
    std::vector<std::unique_ptr<Rec>> frame(std::mt19937& rng,unsigned groups){
        std::vector<std::unique_ptr<Rec>> out;
        for(unsigned g=0;g<groups;++g){const unsigned draws=1+rng()%8;const unsigned groupShader=rng()%10<6?rng()%2:rng()%6;
            std::vector<float>* bank=nullptr;
            for(unsigned k=0;k<draws;++k){auto r=std::make_unique<Rec>();r->constantGroup=g+1;
                r->originalShader=shader(rng()%5?groupShader:rng()%6);r->decl=&decls[rng()%10?0:1];
                if(rng()%4){r->shared=pool[rng()%pool.size()];}else r->snapshot=*mesh(rng,3+rng()%24,rng()%6,rng()%3==0);
                if(!bank||rng()%6==0){r->storage.resize(1024);for(auto& v:r->storage)v=float(int(rng()%2001)-1000)*.001f;
                    r->storage[0]=3;r->storage[1]=1;for(unsigned b=0;b<8;++b){const unsigned row=31+3*b;r->storage[4*row]=1;r->storage[4*(row+1)+1]=1;r->storage[4*(row+2)+2]=1;
                        r->storage[4*row+3]=float(int(rng()%200)-100);r->storage[4*(row+1)+3]=float(int(rng()%200)-100);r->storage[4*(row+2)+3]=float(rng()%50);}
                    bank=&r->storage;}
                r->constants=bank->data(); /* a group shares its bank: earlier records' storage */
                r->shadowSelected=rng()%8!=0;r->shadowSkinned=rng()%10!=0;
                if(r->shadowSelected&&r->shadowSkinned)fill(*r);
                out.push_back(std::move(r));}}
        return out;
    }
};
// ---- the 0.3.176 selectStableActors loop, verbatim (after the program-handle commit): the identity
// reference of prepareRecord. Its renderer members, with the same caches, programs and declaration cache.
struct Reference {
    using Replay=Rec;
    std::vector<std::unique_ptr<Rec>>& replays;std::unordered_map<IDirect3DVertexShader9*,std::shared_ptr<const Program>> actorPrograms;
    NorthlightVertexDeclarations::Cache& declarationCache;struct {float inverseView[16],camera[3];} context;
    NorthlightActorDeformation::SampledVertexCache& sampledVertices;NorthlightActorDeformation::RigidBoneCache& rigidBones;
    std::vector<NorthlightActorShadowSelection::Draw> actorShadowDraws;
    struct {bool stationaryHint(std::uint64_t,const float*){return false;}} actorShadowHistory;
    static NorthlightActorShadowSelection::Tuning selectionTuning(){return NorthlightActorShadowSelection::active();}
    Reference(std::vector<std::unique_ptr<Rec>>& r,NorthlightVertexDeclarations::Cache& d,Caches& c):replays(r),declarationCache(d),sampledVertices(c.sampled),rigidBones(c.bones){}
    void run(std::size_t& distanceTests,std::size_t& distanceReused){actorShadowDraws.clear();
        unsigned previousGroup=UINT_MAX;IDirect3DVertexShader9* previousShader=nullptr;
        IDirect3DVertexDeclaration9* previousDecl=nullptr;float previousDistance=0,previousAt[3]={};bool previousKnown=false,groupRigid=true,groupStationary=false;unsigned groupDraw=0;
        const auto tuning=selectionTuning();
        for(size_t index=0;index<replays.size();++index){const auto& p=*replays[index];
            if(!p.shadowSelected||!p.shadowSkinned)continue;
            NorthlightActorShadowSelection::Draw item;item.index=index;item.bytes=p.mesh().byteSize();item.group=p.constantGroup;
            // Program and declaration lookups only for draws that test a distance
            // or a palette (reused-distance draws of multi-bone groups need none).
            auto program=actorPrograms.end();const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;int declaredState=-1;
            auto declared=[&]{if(declaredState<0){program=actorPrograms.find(p.originalShader);declaredState=program!=actorPrograms.end()&&declarationCache.get(p.decl,elements,count);}return declaredState==1;};
            if(p.constantGroup==previousGroup&&p.originalShader==previousShader&&p.decl==previousDecl){
                item.known=previousKnown;item.distanceSquared=previousDistance;std::memcpy(item.at,previousAt,sizeof item.at);++distanceReused;
            }else{
                ++distanceTests;
                item.known=declared()&&sampledVertices.distance(*program->second,p.mesh(),p.shared,p.decl,elements,count,
                    p.constants,context.inverseView,context.camera,item.distanceSquared,item.at);
            }
            // A group is rigid only if every draw is: after its first multi-bone
            // draw the remaining draws need no palette test. Only a group's first
            // draw supplies the actor identity key.
            const bool first=p.constantGroup!=previousGroup;if(first)groupRigid=true;
            item.bone=groupRigid&&declared()?rigidBones.bone(*program->second,p.mesh(),p.shared,p.decl,elements,count):NAN;item.rigid=!std::isnan(item.bone);
            {Replay& stored=*replays[index];stored.boneKnown=groupRigid&&declared();stored.bone=item.bone;} /* 0.3.176 (S2): rigidObserveGroup reuses it */
            groupRigid=item.rigid;
            if(tuning.stableIdentity){
                // Stable per-draw identity: the snapshot-cache entry (VB/IB identity,
                // range, base, declaration) with the shader; a shape hash only for
                // uncached draws. The actor key is the smallest key of its draws.
                std::uint64_t key=14695981039346656037ull;auto mix=[&](uint64_t n){key=(key^n)*1099511628211ull;};
                mix(reinterpret_cast<uintptr_t>(p.originalShader));mix(reinterpret_cast<uintptr_t>(p.decl));
                if(p.shared)mix(reinterpret_cast<uintptr_t>(p.shared.get()));else{mix(p.mesh().vertexCount);mix(p.mesh().primitiveCount);mix(item.bytes);}
                item.key=key;
                // One palette root per constant group (its draws share the pose).
                if(first){if(program==actorPrograms.end())program=actorPrograms.find(p.originalShader);
                    // Only the audited palette template (c31.. row-major 3x4 bones, translation
                    // in w: the skin-envelope specialization) has a provable root.
                    item.hasRoot=program!=actorPrograms.end()&&NorthlightReplayBounds::SkinEnvelope::supports(*program->second)&&program->second->paletteBase==31&&
                        NorthlightActorDeformation::rootWorld(*program->second,p.constants,context.inverseView,item.root);}}
            else if(first){std::uint64_t key=14695981039346656037ull;auto mix=[&](uint64_t n){key=(key^n)*1099511628211ull;};
                mix(reinterpret_cast<uintptr_t>(p.originalShader));mix(reinterpret_cast<uintptr_t>(p.decl));mix(p.mesh().vertexCount);mix(p.mesh().primitiveCount);mix(item.bytes);
                item.key=key;groupDraw=0;
                // Two extra world samples per draw (first two draws) only for actors
                // the history marks as possibly stationary: the idle-pose check.
                groupStationary=tuning.stationary&&!item.rigid&&item.known&&actorShadowHistory.stationaryHint(key,item.at);}
            if(!tuning.stableIdentity){
                if(groupStationary&&groupDraw<2&&declared())for(unsigned x=0;x<2;++x)
                    if(NorthlightActorDeformation::sampledExtraWorld(*program->second,p.mesh(),elements,count,x,p.constants,context.inverseView,item.extra[item.extras]))++item.extras;
                ++groupDraw;}
            previousGroup=p.constantGroup;previousShader=p.originalShader;previousDecl=p.decl;
            previousKnown=item.known;previousDistance=item.distanceSquared;std::memcpy(previousAt,item.at,sizeof previousAt);actorShadowDraws.push_back(item);
        }
    }
};
struct Camera {float inverseView[16],camera[3];};
static Camera camera(std::mt19937& rng){Camera c{};const float yaw=float(rng()%628)*.01f;
    c.inverseView[0]=std::cos(yaw);c.inverseView[1]=std::sin(yaw);c.inverseView[4]=-std::sin(yaw);c.inverseView[5]=std::cos(yaw);c.inverseView[10]=1;c.inverseView[15]=1;
    for(unsigned k=0;k<3;++k)c.inverseView[12+k]=c.camera[k]=float(int(rng()%400)-200);return c;}
// The records the renderer publishes: selected and skinned at capture, in replays order, with their index.
struct Published {std::vector<const Rec*> records;std::vector<std::size_t> index;};
static Published published(const std::vector<std::unique_ptr<Rec>>& replays,bool filtered=true){
    Published p;for(std::size_t i=0;i<replays.size();++i)if(!filtered||(replays[i]->shadowSelected&&replays[i]->shadowSkinned)){p.records.push_back(replays[i].get());p.index.push_back(i);}return p;}
// prepareRecord over [from, to) resuming from out[from-1] (from 0: a fresh state).
static void prepare(const Published& p,std::size_t from,std::size_t to,Caches& c,const float* inverse,const float* eye,std::vector<Output>& out,bool resetState=false){
    State s=from&&!resetState?out[from-1].after:State{};
    for(std::size_t k=from;k<to;++k)NorthlightActorPrepare::prepareRecord(s,*p.records[k],p.index[k],c,inverse,eye,out[k]);
}
static bool equal(const std::vector<Output>& a,const std::vector<Output>& b,std::size_t n){for(std::size_t k=0;k<n;++k)if(!NorthlightActorPrepare::same(a[k],b[k]))return false;return true;}
static std::size_t hasRoot=0,rigid=0,known=0,reused=0;
static void stats(const std::vector<Output>& out,std::size_t n){for(std::size_t k=0;k<n;++k){hasRoot+=out[k].item.hasRoot;rigid+=out[k].item.rigid;known+=out[k].item.known;}if(n)reused+=out[n-1].after.distanceReused;}
// Handover at every index; cache independence; the carried-state and filter counterfactuals.
static void handover(World& world,unsigned sequences){
    std::mt19937 rng(1771);std::size_t handovers=0,resetDiffer=0,unfilteredDiffer=0;auto warm=std::make_unique<Caches>();
    for(unsigned q=0;q<sequences;++q){
        auto replays=world.frame(rng,6+rng()%14);const auto p=published(replays);const std::size_t n=p.records.size();const Camera c=camera(rng);
        std::vector<Output> reference(n),out(n),check(n);auto cold=std::make_unique<Caches>();
        prepare(p,0,n,*cold,c.inverseView,c.camera,reference);stats(reference,n);
        prepare(p,0,n,*warm,c.inverseView,c.camera,check);assert(equal(check,reference,n)); /* pre-warmed caches */
        prepare(p,0,n,*warm,c.inverseView,c.camera,check);assert(equal(check,reference,n));
        for(std::size_t k=0;k<=n;++k){auto caches=std::make_unique<Caches>();
            prepare(p,0,k,*caches,c.inverseView,c.camera,out);prepare(p,k,n,*caches,c.inverseView,c.camera,out);
            assert(equal(out,reference,n));++handovers;
            if(k&&k<n){prepare(p,k,n,*caches,c.inverseView,c.camera,out,true);resetDiffer+=!equal(out,reference,n);}}
        // Counterfactual: every record published (small or unskinned too): the selected ones' outputs differ.
        const auto all=published(replays,false);std::vector<Output> every(all.records.size());auto caches=std::make_unique<Caches>();
        prepare(all,0,all.records.size(),*caches,c.inverseView,c.camera,every);
        std::vector<Output> projected;for(std::size_t k=0;k<all.records.size();++k)if(all.records[k]->shadowSelected&&all.records[k]->shadowSkinned)projected.push_back(every[k]);
        unfilteredDiffer+=!equal(projected,reference,n);
    }
    assert(resetDiffer>sequences&&unfilteredDiffer>sequences/4&&hasRoot&&rigid&&known&&reused);
    std::printf("handover: %u sequences, %zu handovers equal to inline (hasRoot=%zu rigid=%zu known=%zu reused=%zu); counterfactuals differ: state reset %zu, unfiltered %zu\n",
        sequences,handovers,hasRoot,rigid,known,reused,resetDiffer,unfilteredDiffer);
}
// Counterfactual: the declaration read through the live cache after 128 other layouts evicted its slot.
static void evictedDeclaration(World& world){
    std::mt19937 rng(1772);NorthlightVertexDeclarations::Cache cache;std::vector<FakeDecl> others(200);
    for(unsigned i=0;i<others.size();++i)others[i].layout={{0,std::uint16_t(4+4*(i%3)),2,0,0,0},{0,16,5,0,2,0},{0xff,0,17,0,0,0}};
    std::size_t differ=0,frames=0;
    for(unsigned q=0;q<40;++q){auto replays=world.frame(rng,4+rng()%6);auto p=published(replays);const std::size_t n=p.records.size();const Camera c=camera(rng);
        std::vector<const D3DVERTEXELEMENT9*> live(n);std::vector<UINT> counts(n);
        for(std::size_t k=0;k<n;++k)if(p.records[k]->declared){bool ok=cache.get(p.records[k]->decl,live[k],counts[k]);assert(ok);(void)ok;}
        for(auto& d:others){const D3DVERTEXELEMENT9* e=nullptr;UINT m=0;cache.get(&d,e,m);} /* 200 > 128 slots: every slot turned over */
        std::vector<Rec> stale(n);for(std::size_t k=0;k<n;++k){stale[k].constantGroup=p.records[k]->constantGroup;stale[k].originalShader=p.records[k]->originalShader;
            stale[k].decl=p.records[k]->decl;stale[k].shared=p.records[k]->shared;stale[k].snapshot=p.records[k]->snapshot;stale[k].constants=p.records[k]->constants;
            stale[k].program=p.records[k]->program;stale[k].declared=p.records[k]->declared;stale[k].elementCount=counts[k];
            if(live[k])std::copy(live[k],live[k]+counts[k],stale[k].elements.begin());}
        Published q2;for(std::size_t k=0;k<n;++k){q2.records.push_back(&stale[k]);q2.index.push_back(p.index[k]);}
        std::vector<Output> reference(n),read(n);auto a=std::make_unique<Caches>(),b=std::make_unique<Caches>();
        prepare(p,0,n,*a,c.inverseView,c.camera,reference);prepare(q2,0,n,*b,c.inverseView,c.camera,read);
        differ+=!equal(read,reference,n);++frames;
        cache.clear();}
    assert(differ>frames/2);std::printf("declaration copy: the live cache read after eviction differs in %zu of %zu frames\n",differ,frames);
}
// prepareRecord == the 0.3.176 loop: draws, the stored rigid bones and the distance counters, bit for bit.
static void loopIdentity(World& world,unsigned frames){
    std::mt19937 rng(1775);std::size_t draws=0,tested=0;auto caches=std::make_unique<Caches>();
    std::unordered_map<IDirect3DVertexShader9*,std::shared_ptr<const Program>> programs;
    for(unsigned k=0;k<world.programs.size();++k)if(world.programs[k])programs[World::shader(k)]=world.programs[k];
    auto referenceCaches=std::make_unique<Caches>(); /* the reference's caches persist across frames, as the renderer's */
    for(unsigned f=0;f<frames;++f){
        auto replays=world.frame(rng,1+rng()%24);const auto p=published(replays);const std::size_t n=p.records.size();const Camera c=camera(rng);
        Reference r(replays,world.declarationCache,*referenceCaches);r.actorPrograms=programs;std::memcpy(r.context.inverseView,c.inverseView,64);std::memcpy(r.context.camera,c.camera,12);
        std::size_t tests=0,reusedDistances=0;r.run(tests,reusedDistances);
        std::vector<Output> out(n);prepare(p,0,n,*caches,c.inverseView,c.camera,out);
        assert(r.actorShadowDraws.size()==n);
        for(std::size_t k=0;k<n;++k){Output expected;expected.item=r.actorShadowDraws[k];expected.tested=p.records[k]->boneKnown;expected.after=out[k].after;
            assert(NorthlightActorPrepare::same(out[k],expected));assert(NorthlightActorPrepare::sameBits(&p.records[k]->bone,&out[k].item.bone,4));tested+=out[k].tested;}
        assert(!n||(out[n-1].after.distanceTests==tests&&out[n-1].after.distanceReused==reusedDistances));draws+=n;
    }
    std::printf("loop identity: prepareRecord == the 0.3.176 loop over %u frames, %zu draws (%zu rigid bones tested)\n",frames,draws,tested);
}
// Test processes: a throw on one index, or a stall (the watchdog).
static std::atomic<std::size_t> throwAt{SIZE_MAX},stallAt{SIZE_MAX};
struct Faulty {template<class R> void operator()(State& s,const R& p,std::size_t index,Caches& c,const NorthlightActorPrepare::Frame& f,Output& out)const{
    if(index==throwAt.load())throw std::bad_alloc();
    if(index==stallAt.load())std::this_thread::sleep_for(std::chrono::milliseconds(20));
    NorthlightActorPrepare::prepareRecord(s,p,index,c,f.inverseView,f.camera,out);}};
static void delay(std::mt19937& rng){switch(rng()%6){case 0:std::this_thread::sleep_for(std::chrono::microseconds(rng()%80));break;
    case 1:for(unsigned i=rng()%2000;i;--i)NorthlightActorPrepare::pause();break;case 2:std::this_thread::yield();break;default:break;}}
// The threaded worker against inline, with the renderer's join: stop, then the tail over [done, n) with
// the frame's frozen camera and the same caches (ownership returned).
template<class Process> static void threaded(World& world,unsigned frames,unsigned maxGroups,bool faults){
    std::mt19937 rng(1773+frames);NorthlightActorPrepare::Worker<Rec,Process> worker;worker.setWatchdogMs(2000); /* a test-host scheduler stall is not a hang */auto caches=std::make_unique<Caches>();auto referenceCaches=std::make_unique<Caches>();
    std::size_t published_=0,byWorker=0,byJoin=0,beforeFirst=0,unpublished=0,failed=0,cameraDiffer=0,withTail=0;
    for(unsigned f=0;f<frames;++f){
        auto replays=world.frame(rng,1+rng()%maxGroups);const auto p=published(replays);const std::size_t n=p.records.size();Camera context=camera(rng);const Camera captured=context;
        std::vector<Output> reference(n);prepare(p,0,n,*referenceCaches,captured.inverseView,captured.camera,reference);
        if(faults){throwAt=rng()%3==0&&n?p.index[rng()%n]:SIZE_MAX;}
        const unsigned mode=rng()%8;const std::size_t cut=mode==0?0:mode==1?rng()%(n+1):n; /* publish all but a tail the joiner prepares */
        NorthlightActorPrepare::Frame frame;std::memcpy(frame.inverseView,context.inverseView,64);std::memcpy(frame.camera,context.camera,12);frame.timed=rng()%2;
        std::size_t k=0;
        if(mode!=0||rng()%2){bool ok=worker.begin(frame,*caches);assert(ok);(void)ok;
            for(;k<cut;++k){delay(rng);bool ok=worker.publish(p.records[k],p.index[k]);assert(ok);(void)ok;}}
        context.camera[0]+=1;context.inverseView[12]+=1; /* the context changes after the frame opened */
        if(rng()%3==0)std::this_thread::sleep_for(std::chrono::microseconds(rng()%300)); /* stop after completion */
        const auto r=worker.stop();if(r.timedOut||r.published!=k||r.done>k)std::printf("STOP unexpected: timedOut=%d published=%u k=%zu done=%u failed=%d\n",r.timedOut,r.published,k,r.done,r.failed);assert(!r.timedOut&&r.published==k&&r.done<=k);beforeFirst+=k==0;unpublished+=n-k;failed+=r.failed;
        std::vector<Output> out(n);
        for(std::size_t j=0;j<r.done;++j)out[j]=worker.outputs()[j];
        prepare(p,r.done,n,*caches,frame.inverseView,frame.camera,out);
        assert(equal(out,reference,n));
        published_+=k;byWorker+=r.done;byJoin+=n-r.done;
        if(r.done<n){++withTail;std::vector<Output> wrong=out;prepare(p,r.done,n,*caches,context.inverseView,context.camera,wrong);cameraDiffer+=!equal(wrong,reference,n);}
    }
    throwAt=SIZE_MAX;
    assert(byWorker>0&&byJoin>0&&beforeFirst>0&&unpublished>0&&cameraDiffer>withTail/2&&(!faults||failed>0));
    std::printf("threaded%s: %u frames equal to inline: %zu published, %zu by the worker, %zu by the join (%zu never published), %zu stops before the first publish, %zu worker exceptions taken over; camera at the join differs in %zu of %zu frames with a tail\n",
        faults?" with exceptions":"",frames,published_,byWorker,byJoin,unpublished,beforeFirst,failed,cameraDiffer,withTail);
}
// A record that stalls past the watchdog: stop() returns within about WatchdogMs, the worker is
// abandoned for the session and the frame is prepared inline with fresh caches; the records stay alive
// until the worker settles (it finishes the record in flight and acknowledges).
static void watchdog(World& world){
    std::mt19937 rng(1774);NorthlightActorPrepare::Worker<Rec,Faulty> worker;auto caches=std::make_unique<Caches>();
    auto replays=world.frame(rng,12);const auto p=published(replays);const std::size_t n=p.records.size();assert(n>4);const Camera c=camera(rng);
    std::vector<Output> reference(n);{auto r=std::make_unique<Caches>();prepare(p,0,n,*r,c.inverseView,c.camera,reference);}
    stallAt=p.index[2];NorthlightActorPrepare::Frame frame;std::memcpy(frame.inverseView,c.inverseView,64);std::memcpy(frame.camera,c.camera,12);
    bool ok=worker.begin(frame,*caches);assert(ok);for(std::size_t k=0;k<n;++k)worker.publish(p.records[k],p.index[k]);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    const auto start=std::chrono::steady_clock::now();const auto r=worker.stop();const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    assert(r.timedOut&&r.done==0&&worker.abandoned()&&!worker.ready()&&!worker.begin(frame,*caches)&&ms<15);
    assert(!worker.settled()); /* the record in flight is still being read: the caller keeps the records */
    std::vector<Output> out(n);auto fresh=std::make_unique<Caches>();prepare(p,0,n,*fresh,c.inverseView,c.camera,out);assert(equal(out,reference,n));
    for(unsigned i=0;i<2000&&!worker.settled();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(worker.settled()); /* it finished that record, saw the stop and acknowledged: records and caches are free again */
    stallAt=SIZE_MAX;(void)ok;
    std::printf("watchdog: a 20 ms record returned stop() after %.2f ms, worker abandoned, frame prepared inline equal\n",ms);
}
int main(int argc,char** argv){
    assert(argc>2);World world(argv[1]);const bool tsan=std::string(argv[2])=="tsan";
    loopIdentity(world,tsan?40:600);handover(world,tsan?4:24);evictedDeclaration(world);
    threaded<NorthlightActorPrepare::Prepare>(world,tsan?10000:3000,tsan?3:12,false);
    threaded<Faulty>(world,tsan?2000:1500,tsan?3:12,true);
    watchdog(world);
    std::puts("PASS prepare worker: handover at every index, threaded worker and join, exception and watchdog takeover equal to inline; counterfactuals fail");
}
