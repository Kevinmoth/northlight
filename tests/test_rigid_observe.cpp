// 0.3.176 (S2): rigidObserveGroup reusing selection's rigid bone. The production rigidProgram() and
// rigidObserveGroup() (world_rigid_memory.inl, pasted below by test_rigid_memory.py) observe random
// capture frames: groups of shared and owned draws, the audited one-influence and four-bone client
// programs, an unaudited program that reads BLENDINDICES, a program without them and an unknown one,
// small unselected draws (first in a group too), selection drops, rigid and mixed-bone meshes, bones
// outside 0..74, unknown declarations, and frames without the stable selection (radius 0, no ranking).
// Observations, bodies, groups and copy sources must equal the 0.3.175 path (every stored flag clear:
// the unchanged fallback). Counterfactual: the stored bone used without the audit gate must differ.
// Built by test_rigid_memory.py. Native, no game or GPU.
#include "rigid_memory.h"
#include "sampled_vertex_cache.h"
#include "replay_bounds.h"
#include "actor_client_programs.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <random>
#include <unordered_map>
struct IDirect3DVertexShader9;
using NorthlightActorDeformation::Program;
static const D3DVERTEXELEMENT9 gameLayout[]={{0,0,2,0,0,0},{0,12,8,0,1,0},{0,16,5,0,2,0},{0xff,0,17,0,0,0}};
struct Replay {
    bool shadowSkinned=true,shadowSelected=true,shadowSmall=false,boneKnown=false;float bone=NAN;unsigned constantGroup=0;UINT count=0;
    IDirect3DVertexShader9* originalShader=nullptr;IDirect3DVertexDeclaration9* decl=nullptr;
    std::shared_ptr<const NorthlightDrawSnapshot::Mesh> shared;NorthlightDrawSnapshot::Mesh snapshot;float constantStorage[1024]={};const float* constants=constantStorage;
    const NorthlightDrawSnapshot::Mesh& mesh()const{return shared?*shared:snapshot;}
};
struct Payload {int unused=0;};
struct Declarations {std::unordered_map<const void*,int> known;
    bool get(IDirect3DVertexDeclaration9* d,const D3DVERTEXELEMENT9*& elements,UINT& count){if(!known.count(d))return false;elements=gameLayout;count=4;return true;}};
struct Base {
    std::vector<std::unique_ptr<Replay>>& replays;std::unordered_map<IDirect3DVertexShader9*,Program>& actorPrograms;Declarations& declarationCache;
    struct {float inverseView[16]={};} context;
    NorthlightRigidMemory::Registry<Payload> rigidMemory;NorthlightActorDeformation::RigidBoneCache rigidBones;
    std::vector<NorthlightRigidMemory::Observation> rigidObservations;std::vector<float> rigidBodies;
    std::vector<const Replay*> rigidGroupDraws;std::vector<std::pair<size_t,unsigned>> rigidGroups;
    std::unordered_map<IDirect3DVertexShader9*,bool> rigidAudited;
    Base(std::vector<std::unique_ptr<Replay>>& r,std::unordered_map<IDirect3DVertexShader9*,Program>& p,Declarations& d,const float* inverse):replays(r),actorPrograms(p),declarationCache(d){std::memcpy(context.inverseView,inverse,64);}
};
// test_rigid_memory.py writes each harness as struct Name:Base{using Base::Base; <the two methods> OBSERVE_LOOP};
#define OBSERVE_LOOP \
    void observe(){rigidObservations.clear();rigidBodies.clear();rigidGroupDraws.clear();rigidGroups.clear(); \
        for(size_t i=0;i<replays.size();){const unsigned group=replays[i]->constantGroup;size_t j=i;while(j<replays.size()&&replays[j]->constantGroup==group)++j;rigidObserveGroup(i,j);i=j;}}
/*OBSERVE_METHODS*/
static bool same(const Base& a,const Base& b){
    if(a.rigidObservations.size()!=b.rigidObservations.size()||a.rigidBodies!=b.rigidBodies||a.rigidGroups!=b.rigidGroups||a.rigidGroupDraws!=b.rigidGroupDraws)return false;
    for(size_t i=0;i<a.rigidObservations.size();++i){const auto& x=a.rigidObservations[i];const auto& y=b.rigidObservations[i];
        if(x.shape!=y.shape||std::memcmp(x.world,y.world,sizeof x.world)||x.hasWorld0!=y.hasWorld0||(x.hasWorld0&&std::memcmp(x.world0,y.world0,sizeof x.world0))||
           x.selected!=y.selected||x.draws!=y.draws||x.triangles!=y.triangles||x.bytes!=y.bytes)return false;}
    return true;
}
// A mesh in the game layout: every vertex on bone `bone` (weight 255 on lane 0), or (mixed) two bones.
static std::shared_ptr<NorthlightDrawSnapshot::Mesh> mesh(unsigned vertices,unsigned bone,bool mixed,std::mt19937& rng){
    auto m=std::make_shared<NorthlightDrawSnapshot::Mesh>();m->vertexCount=vertices;m->streams[0].stride=20;m->streams[0].bytes.resize(size_t(vertices)*20);
    for(unsigned v=0;v<vertices;++v){auto* out=m->streams[0].bytes.data()+size_t(v)*20;const float p[3]={float(rng()%100)*.01f,float(rng()%100)*.01f,float(rng()%100)*.01f};
        std::memcpy(out,p,12);out[12]=255;out[13]=out[14]=out[15]=0;out[16]=std::uint8_t(mixed&&v%2?bone+1:bone);out[17]=7;out[18]=3;out[19]=9;}
    m->indexed=true;m->topology=D3DPT_TRIANGLELIST;for(unsigned t=0;t+2<vertices;++t)for(unsigned k=0;k<3;++k)m->indices.push_back(t+k);
    m->primitiveCount=unsigned(m->indices.size()/3);return m;
}
static Program compiled(const std::uint32_t* words,std::size_t n){Program p;assert(NorthlightActorDeformation::compile(words,n,p));return p;}
#define CLIENT_PROGRAM(name) compiled(ClientShaders::name,sizeof ClientShaders::name/4)
static std::vector<NorthlightActorDeformation::Word> load(const char* path){
    FILE* f=std::fopen(path,"rb");assert(f);std::fseek(f,0,SEEK_END);long size=std::ftell(f);std::rewind(f);
    std::vector<NorthlightActorDeformation::Word> w(size_t(size)/4);assert(std::fread(w.data(),1,size_t(size),f)==size_t(size));std::fclose(f);return w;}
int main(int argc,char** argv){
    assert(argc>1);
    auto shader=[](unsigned k){return reinterpret_cast<IDirect3DVertexShader9*>(std::uintptr_t(0x1000+16*k));};
    auto declaration=[](unsigned k){return reinterpret_cast<IDirect3DVertexDeclaration9*>(std::uintptr_t(0x9000+16*k));};
    std::unordered_map<IDirect3DVertexShader9*,Program> programs;
    programs[shader(0)]=CLIENT_PROGRAM(OneBoneVs3);                              /* audited: the one-influence template */
    {auto words=load(argv[1]);Program four;assert(NorthlightActorDeformation::compile(words.data(),words.size(),four));programs[shader(1)]=four;} /* audited: skin envelope */
    {Program p=CLIENT_PROGRAM(OneBoneVs3);p.paletteBase=34;programs[shader(2)]=p;} /* reads BLENDINDICES, not audited */
    programs[shader(3)]=CLIENT_PROGRAM(NoBoneVs3);                               /* no BLENDINDICES */
    /* shader(4): not an actor program */
    assert(NorthlightRigidGeometry::oneBoneTemplate(programs[shader(0)])&&NorthlightReplayBounds::SkinEnvelope::supports(programs[shader(1)])&&programs[shader(1)].paletteBase==31);
    assert(!NorthlightRigidGeometry::oneBoneTemplate(programs[shader(2)])&&!NorthlightReplayBounds::SkinEnvelope::supports(programs[shader(2)]));
    Declarations declarations;declarations.known[declaration(0)]=1; /* declaration(1): unknown */
    std::mt19937 rng(1762);float inverse[16]={1,0,0,0, 0,1,0,0, 0,0,1,0, -8850,620,100,1};
    NorthlightActorDeformation::RigidBoneCache selectionBones; /* the renderer's rigidBones, used by selection first */
    size_t frames=0,stableFrames=0,stored=0,observations=0,bodies=0,differ=0;
    for(unsigned frame=0;frame<2500;++frame,++frames){
        std::vector<std::unique_ptr<Replay>> replays;std::vector<std::shared_ptr<NorthlightDrawSnapshot::Mesh>> keep;
        const unsigned groups=1+rng()%6;
        for(unsigned g=0;g<groups;++g){const unsigned draws=1+rng()%5;const unsigned groupShader=rng()%10<6?0:rng()%5,groupBone=rng()%8?rng()%6:80+rng()%4;
            for(unsigned k=0;k<draws;++k){auto p=std::make_unique<Replay>();p->constantGroup=g+1;
                p->originalShader=shader(rng()%4?groupShader:rng()%5);p->decl=declaration(rng()%12?0:1);
                const unsigned bone=rng()%5?groupBone:rng()%6;auto m=mesh(3+rng()%12,bone,rng()%6==0,rng);keep.push_back(m);
                if(rng()%5)p->shared=m;else p->snapshot=*m;p->count=m->primitiveCount;
                p->shadowSkinned=rng()%10!=0;
                if(rng()%(k==0?3:6)==0){p->shadowSelected=false;p->shadowSmall=true;} /* small at capture */
                for(unsigned r=0;r<256;++r)for(unsigned c=0;c<4;++c)p->constantStorage[4*r+c]=float(int(rng()%2001)-1000)*.001f;
                p->constantStorage[0]=3;p->constantStorage[1]=1;
                replays.push_back(std::move(p));}}
        // Selection (selectStableActors' rule): selected skinned draws in order; a group's palette test stops
        // after its first non-rigid draw; the stored flag marks exactly the draws it tested.
        const bool stable=rng()%10<7;std::vector<std::pair<bool,float>> flags(replays.size(),{false,NAN});
        if(stable){++stableFrames;unsigned previousGroup=UINT_MAX;bool groupRigid=true;selectionBones.beginFrame();
            for(size_t i=0;i<replays.size();++i){const Replay& p=*replays[i];if(!p.shadowSelected||!p.shadowSkinned)continue;
                const bool first=p.constantGroup!=previousGroup;if(first)groupRigid=true;
                auto program=programs.find(p.originalShader);const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;
                const bool declared=program!=programs.end()&&declarations.get(p.decl,elements,count);
                const float bone=groupRigid&&declared?selectionBones.bone(program->second,p.mesh(),p.shared,p.decl,elements,count):NAN;
                flags[i]={groupRigid&&declared,bone};stored+=flags[i].first;groupRigid=!std::isnan(bone);previousGroup=p.constantGroup;}
            for(auto& p:replays)if(p->shadowSelected&&rng()%7==0)p->shadowSelected=false;} /* dropped by the quota or the radius */
        Gated reference(replays,programs,declarations,inverse);reference.observe(); /* every flag clear: the 0.3.175 path */
        for(size_t i=0;i<replays.size();++i){replays[i]->boneKnown=flags[i].first;replays[i]->bone=flags[i].second;}
        Gated gated(replays,programs,declarations,inverse);gated.observe();
        Ungated ungated(replays,programs,declarations,inverse);ungated.observe();
        assert(same(gated,reference));differ+=!same(ungated,reference);
        observations+=reference.rigidObservations.size();bodies+=reference.rigidBodies.size()/3;
    }
    std::printf("rigid observe with selection's bone == 0.3.175: %zu frames (%zu with the stable selection), %zu stored bones, %zu observations, %zu bodies; without the audit gate %zu frames differ\n",
        frames,stableFrames,stored,observations,bodies,differ);
    assert(stored>1000&&observations>500&&bodies>1000&&differ>50);
    std::puts("PASS rigid observe: identical to 0.3.175, the audit gate is needed");
}
