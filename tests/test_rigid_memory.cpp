// 0.3.172 rigid memory (rigid_memory.h): settling (time AND capture frames), sticky mobile/held/static,
// identity without the snapshot pointer, absence (off screen and shortfall frames draw, the in-view
// despawn test, range, teleport, clear), seen-again changes, caps with farthest eviction, the rebase
// round trip; the real client one-influence program (from the retired test_persistent_casters: the exact
// template, a Stormwind sign end to end, the static-doodad flood with real world-cache placements).
// Built by test_rigid_memory.py.
#include "rigid_memory.h"
#include "sampled_vertex_cache.h"
#include "replay_bounds.h"
#include "actor_client_programs.h" /* written by test_rigid_memory.py from the tester's client (client_fixtures.py) */
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <random>
using namespace NorthlightRigidMemory;
using NorthlightRigidGeometry::oneBoneTemplate;using NorthlightRigidGeometry::worldBone;using NorthlightRigidGeometry::staticPlacement;using NorthlightRigidGeometry::PlacementIndex;
using NorthlightActorDeformation::Program;
static std::vector<NorthlightActorDeformation::Word> load(const char* path){
    FILE* f=std::fopen(path,"rb");assert(f);std::fseek(f,0,SEEK_END);long size=std::ftell(f);std::rewind(f);
    std::vector<NorthlightActorDeformation::Word> w(size_t(size)/4);assert(std::fread(w.data(),1,size_t(size),f)==size_t(size));std::fclose(f);return w;}
// The renderer's copy, reduced: a token (released on clear) and the constant bank.
struct Payload {std::shared_ptr<int> token;std::vector<float> constants;unsigned bone=0;};
using Reg=Registry<Payload>;
struct Camera {float inverse[16]={};float projection[3]={1.5f,2.f,-1.f};};
// World camera: rows right, up, back (view z = -forward) and the eye (the renderer's inverse view).
static Camera looking(const double* eye,const double* at){double f[3]={at[0]-eye[0],at[1]-eye[1],at[2]-eye[2]};double n=std::sqrt(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);for(double& x:f)x/=n;
    double r[3]={f[1],-f[0],0};n=std::sqrt(r[0]*r[0]+r[1]*r[1]);for(double& x:r)x/=n;const double u[3]={r[1]*f[2]-r[2]*f[1],r[2]*f[0]-r[0]*f[2],r[0]*f[1]-r[1]*f[0]};
    Camera c;for(unsigned k=0;k<3;++k){c.inverse[k]=float(r[k]);c.inverse[4+k]=float(u[k]);c.inverse[8+k]=float(-f[k]);c.inverse[12+k]=float(eye[k]);}c.inverse[15]=1;return c;}
static Camera facing(double x,double y,double z,double dx,double dy){const double eye[3]={x,y,z},at[3]={x+dx,y+dy,z};return looking(eye,at);}
static void rotZ(float angle,float scale,float* M){const float c=std::cos(angle)*scale,s=std::sin(angle)*scale;const float m[9]={c,-s,0,s,c,0,0,0,scale};std::memcpy(M,m,36);}
// W of a model placed with row-major M (column vector) at t: axis a -> column a of M.
static void placed(const float* M,float x,float y,float z,float* W){for(unsigned a=0;a<3;++a)for(unsigned w=0;w<3;++w)W[a*3+w]=M[w*3+a];W[9]=x;W[10]=y;W[11]=z;}
static Observation sign(std::uint64_t shape,float x,float y,float z,float yaw=.3f,std::size_t bytes=4000,unsigned draws=1,unsigned triangles=150){
    Observation o;o.shape=shape;float M[9];rotZ(yaw,1,M);placed(M,x,y,z,o.world);o.draws=draws;o.triangles=triangles;o.bytes=bytes;return o;}
struct Scene {
    Reg reg;unsigned now=1000;std::vector<Observation> obs;std::vector<float> bodies;int covered=0;unsigned injected=0,stored=0,refreshed=0,screens=0;std::shared_ptr<int> token=std::make_shared<int>(0);
    float pivot[3]={0,0,0};Camera away=facing(0,0,2,-1,0),toward=facing(0,0,2,1,0);
    explicit Scene(Tuning t=Tuning{}):reg(t){}
    void step(std::vector<Observation> list,const Camera& c,bool complete=true,unsigned dt=16){now+=dt;obs=std::move(list);
        reg.frame(obs,bodies.data(),bodies.size()/3,now,pivot,c.inverse,c.projection,complete,[&](std::size_t){++screens;return covered;});
        for(const auto& o:obs)if(o.action!=Observation::None&&reg.store(o,Payload{token,{},0},now)){if(o.action==Observation::Remember)++stored;else ++refreshed;}
        injected=0;reg.forAbsent([&](Reg::Entry&){++injected;});}
    // Frames at dt until remembered (or `limit` frames): the count.
    unsigned settle(const Observation& o,const Camera& c,unsigned dt=16,unsigned limit=1000){for(unsigned k=1;k<=limit;++k){step({o},c,true,dt);if(reg.entries().size())return k;}return 0;}
};
// Frames at 16 ms until a new track is remembered: the first, then 125 more (2000 ms).
constexpr unsigned Settle=126;
static void settle(){
    // Time AND capture frames: 16 ms frames need the whole 2 s (125 frames after the first); 1 s frames need a 4th frame.
    {Scene s;const auto o=sign(1,20,0,3);assert(s.settle(o,s.away)==Settle&&s.stored==1&&s.reg.stats().remembered==1);}
    {Scene s;const auto o=sign(1,20,0,3);for(unsigned k=0;k<3;++k){s.step({o},s.away,true,1000);assert(s.reg.entries().empty());} /* 2 s, 3 frames: not yet */
     s.step({o},s.away,true,1000);assert(s.reg.entries().size()==1);}
    {Scene s;const auto o=sign(1,20,0,3);for(unsigned k=0;k<40;++k){s.step({o},s.away,true,16);}assert(s.reg.entries().empty());} /* 40 frames, 640 ms: not yet */
    // A creeping object (0.03 yd per frame, below the travel limit) restarts the settle every frame.
    {Scene s;for(unsigned k=0;k<16;++k){s.step({sign(1,20+.03f*k,0,3)},s.away);}for(unsigned k=0;k<16;++k)s.step({sign(1,20.45f,0,3)},s.away);
     assert(s.reg.entries().empty());assert(s.settle(sign(1,20.45f,0,3),s.away)==Settle-17);} /* settles from its stop: the anchor is the 16th frame */
    std::puts("PASS settle: 2000 ms AND 4 capture frames (either alone is not enough), motion restarts it");
}
static void sticky(){
    // Mobile: travelled 0.6 yd, then still for 10 s: never remembered. Turned 0.06: likewise.
    {Scene s;for(unsigned k=0;k<=12;++k)s.step({sign(1,20+.05f*k,0,3)},s.away);for(unsigned k=0;k<625;++k)s.step({sign(1,20.6f,0,3)},s.away);
     assert(s.reg.entries().empty()&&s.reg.stats().mobile==1);}
    {Scene s;for(unsigned k=0;k<=6;++k)s.step({sign(1,20,0,3,.3f+.01f*k)},s.away);for(unsigned k=0;k<625;++k)s.step({sign(1,20,0,3,.36f)},s.away);
     assert(s.reg.entries().empty()&&s.reg.stats().mobile==1);}
    // Held: a body root within 4 yd in a single frame, then gone for good: never remembered. 4.1 yd: remembered.
    {Scene s;s.bodies={22,0,0};s.step({sign(1,20,0,3)},s.away);s.bodies.clear();for(unsigned k=0;k<625;++k)s.step({sign(1,20,0,3)},s.away);
     assert(s.reg.entries().empty()&&s.reg.stats().held==1);}
    {Scene s;s.bodies={20,4.1f,3};assert(s.settle(sign(1,20,0,3),s.away)==Settle&&s.reg.stats().held==0);
     // A body coming within 4 yd of a remembered object (its owner back in the capture): dropped, held for good.
     s.bodies={20,3.9f,3};s.step({sign(1,20,0,3)},s.away);assert(s.reg.entries().empty()&&s.reg.stats().held==1);
     s.bodies.clear();for(unsigned k=0;k<300;++k)s.step({sign(1,20,0,3)},s.away);assert(s.reg.entries().empty());}
    // Static: covered once -> never; not covered -> remembered; unknown -> asked again every frame.
    {Scene s;s.covered=1;for(unsigned k=0;k<300;++k)s.step({sign(1,20,0,3)},s.away);assert(s.reg.entries().empty()&&s.reg.stats().statics==1&&s.screens==1);}
    {Scene s;s.covered=-1;for(unsigned k=0;k<200;++k)s.step({sign(1,20,0,3)},s.away);assert(s.reg.entries().empty()&&s.reg.screening()&&s.screens==200-Settle+1);
     s.covered=0;s.step({sign(1,20,0,3)},s.away);assert(s.reg.entries().size()==1&&!s.reg.screening());}
    // Too large: more than 4 draws, 4096 triangles or 256 KiB of mesh.
    for(const auto& o:{sign(1,20,0,3,.3f,4000,5),sign(1,20,0,3,.3f,4000,1,4097),sign(1,20,0,3,.3f,(256u<<10)+1)}){Scene s;assert(!s.settle(o,s.away,16,300));}
    std::puts("PASS sticky: mobile (travel 0.5 yd, turn 0.05), held (a body root within 4 yd in any frame, also after remembering), static screen once; small only");
}
static void identity(){
    // The renderer's identity: draw shapes, never the snapshot pointer. A snapshot re-created every 10 frames keeps the entry.
    const int shader=0,decl=0;std::vector<int> snapshots(400);
    auto shape=[&](unsigned frame,bool withPointer){std::uint64_t h=ShapeSeed;mixShape(h,&shader,&decl,268,150,6432);
        if(withPointer){const auto* p=&snapshots[frame/10];h=(h^std::uint64_t(reinterpret_cast<std::uintptr_t>(p)))*1099511628211ull;}return h;};
    {Scene s;unsigned frame=0;for(;frame<400&&s.reg.entries().empty();++frame)s.step({sign(shape(frame,false),20,0,3)},s.away);
     assert(frame==Settle);for(;frame<400;++frame){s.step({sign(shape(frame,false),20,0,3)},s.away);assert(s.reg.entries().size()==1&&s.injected==0);}
     assert(s.stored==1&&s.refreshed==400-Settle);}
    // Counterfactual (must fail): the snapshot pointer in the identity rekeys every 160 ms and never settles.
    {Scene s;for(unsigned frame=0;frame<400;++frame)s.step({sign(shape(frame,true),20,0,3)},s.away);assert(s.reg.entries().empty()&&s.reg.stats().tracks==40);}
    std::puts("PASS identity: shapes + origin; a re-created snapshot keeps the entry (counterfactual with the pointer: 40 tracks, never remembered)");
}
static void absence(){
    const auto o=sign(1,20,0,3);
    // Off screen (camera away): drawn for 120 s.
    {Scene s;s.settle(o,s.away);for(unsigned k=0;k<120;++k){s.step({},s.away,true,1000);assert(s.injected==1);}
     // In view on shortfall frames: still drawn (the draw may be one the budget turned away).
     unsigned drawn=0,gated=0;for(unsigned k=0;k<50;++k){const bool complete=false;s.step({},s.toward,complete);drawn+=s.injected;gated+=complete?s.injected:0;}
     assert(drawn==50&&gated==0); /* counterfactual (must fail): drawing only on complete frames draws nothing here */
     // In view on complete frames: dropped after 2 frames AND 150 ms; a shortfall frame in between holds.
     unsigned k=0;for(;k<40&&s.reg.entries().size();++k)s.step({},s.toward,k!=3);
     assert(s.reg.stats().droppedInView==1&&k==11);} /* frames at 0,16,32,(48 shortfall),64..160: dropped at 160 ms, the 10th complete one */
    // Not in view on a complete frame resets the count.
    {Scene s;s.settle(o,s.away);for(unsigned k=0;k<300;++k){s.step({},k%2?s.toward:s.away);assert(s.injected==1);}}
    // Range: walking away 1 yd per frame; dropped beyond 80 yd from the pivot.
    {Scene s;s.settle(o,s.away);unsigned k=0;for(;k<200&&s.reg.entries().size();++k){s.pivot[0]=-float(k);s.step({},s.away);}
     assert(s.reg.stats().droppedRange==1&&k==61);} /* at pivot x=-60: |(80,0,3)| = 80.06 > 80 */
    // Teleport: the pivot jumps 150 yd -> everything cleared. Map change: clear() releases the payloads.
    {Scene s;s.settle(o,s.away);assert(s.token.use_count()==2);s.pivot[0]=150;s.step({},s.away);assert(s.reg.entries().empty()&&s.reg.stats().droppedTeleport==1&&s.reg.stats().tracks==0&&s.token.use_count()==1);}
    {Scene s;s.settle(o,s.away);for(unsigned k=0;k<Settle;++k)s.step({sign(2,-20,5,3)},s.away);assert(s.reg.entries().size()==2&&s.token.use_count()==3);s.reg.clear();assert(s.reg.entries().empty()&&s.token.use_count()==1&&s.reg.stats().cleared==2);}
    // Safety timeout: 10 min unseen.
    {Scene s;s.settle(o,s.away);s.step({},s.away,true,599999);assert(s.reg.entries().size()==1);s.step({},s.away,true,2);assert(s.reg.stats().droppedUnseen==1);}
    // Seen again with other axes (a door that opened) or moved within the identity: dropped and mobile for good.
    for(const auto& moved:{sign(1,20,0,3,.3f+.02f),sign(1,20.1f,0,3)}){Scene s;s.settle(o,s.away);s.step({moved},s.away);
        assert(s.reg.entries().empty()&&s.reg.stats().droppedMoved==1&&s.reg.stats().mobile==1);for(unsigned k=0;k<300;++k)s.step({moved},s.away);assert(s.reg.entries().empty());}
    std::puts("PASS absence: off screen and shortfall frames draw, in-view despawn after 2 complete frames and 150 ms, range 80, teleport and clear release everything, 10 min timeout, seen changed -> dropped and mobile");
}
static void caps(){
    // 70 signs at 5..74 yd (both orders): the 64 nearest stay, the farthest go.
    for(bool reversed:{false,true}){Scene s;std::vector<Observation> all;for(unsigned i=0;i<70;++i){const unsigned k=reversed?69-i:i;all.push_back(sign(100+k,-5.f-k,0,3));}
        for(unsigned f=0;f<130;++f)s.step(all,s.toward); /* in view but seen: kept */
        assert(s.reg.entries().size()==64);float far=0;for(const auto& e:s.reg.entries())far=std::max(far,-e.world[9]);assert(far==68);
        assert(s.reg.stats().evicted==(reversed?6u:0u));for(unsigned f=0;f<30;++f)s.step(all,s.toward);assert(s.reg.stats().remembered==(reversed?70u:64u));}
    // 2 MiB of mesh: eight 256 KiB entries; a nearer ninth evicts the farthest.
    {Scene s;std::vector<Observation> all;for(unsigned i=0;i<8;++i)all.push_back(sign(200+i,-10.f-i,0,3,.3f,256u<<10));for(unsigned f=0;f<130;++f)s.step(all,s.toward);
     assert(s.reg.entries().size()==8&&s.reg.stats().bytes==(2u<<20));all.push_back(sign(300,-5,0,3,.3f,256u<<10));for(unsigned f=0;f<130;++f)s.step(all,s.toward);
     assert(s.reg.entries().size()==8&&s.reg.stats().evicted==1&&s.reg.stats().bytes==(2u<<20));for(const auto& e:s.reg.entries())assert(e.world[9]!=-17);}
    // Tracks: at most 2048 (the oldest without an entry go); unseen 60 s forgotten.
    {Scene s;std::vector<Observation> crowd;for(unsigned i=0;i<2100;++i)crowd.push_back(sign(1000+i,float(i%50),float(i/50),0));s.step(crowd,s.away);assert(s.reg.stats().tracks==2048);
     s.step({},s.away,true,60000);assert(s.reg.stats().tracks==0);}
    std::puts("PASS caps: 64 entries and 2 MiB with farthest eviction (either arrival order), 2048 tracks, forgotten after 60 s unseen");
}
static void rebaseRoundTrip(){
    std::mt19937 rng(172);std::uniform_real_distribution<float> angle(-3.1f,3.1f),pos(-9000,9000),near(-60,60),scale(.4f,2.2f);double worst=0;
    for(unsigned i=0;i<1000;++i){float yaw=angle(rng),pitch=angle(rng)*.45f,roll=angle(rng)*.1f;
        const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch),cr=std::cos(roll),sr=std::sin(roll);
        float iv[16]={};const float f[3]={cy*cp,sy*cp,sp},r0[3]={sy,-cy,0},u0[3]={r0[1]*f[2]-r0[2]*f[1],r0[2]*f[0]-r0[0]*f[2],r0[0]*f[1]-r0[1]*f[0]};
        for(unsigned k=0;k<3;++k){iv[k]=cr*r0[k]+sr*u0[k];iv[4+k]=-sr*r0[k]+cr*u0[k];iv[8+k]=-f[k];}const float eye[3]={pos(rng),pos(rng),pos(rng)*.02f};
        for(unsigned k=0;k<3;++k)iv[12+k]=eye[k];iv[15]=1;
        float M[9],W[12];rotZ(angle(rng),scale(rng),M);placed(M,eye[0]+near(rng),eye[1]+near(rng),eye[2]+near(rng),W);
        float rows[12],back[12];assert(rebase(W,iv,rows)&&worldBone(rows,iv,back));
        for(unsigned k=0;k<12;++k){const double tolerance=1e-5*(k<9?1.0:std::max(1.0,std::fabs(double(W[k]))));const double d=std::fabs(double(back[k])-W[k]);worst=std::max(worst,d/tolerance);assert(d<=tolerance);}}
    float singular[16]={};assert(!rebase(singular+0,singular,singular+4));
    std::printf("PASS rebase round trip: worldBone(rebase(W, view), view) == W for 1000 random views and placements up to 9000 yd (worst %.2f of 1e-5 relative)\n",worst);
}
// ---- real client one-influence program (from the retired test_persistent_casters) ----------
static Program compiled(const std::uint32_t* words,std::size_t n){Program p;assert(NorthlightActorDeformation::compile(words,n,p));return p;}
#define CLIENT_PROGRAM(name) compiled(ClientShaders::name,sizeof ClientShaders::name/4)
static void oneBoneProgram(const char* fourBone){
    const Program variants[]={CLIENT_PROGRAM(OneBoneVs3),CLIENT_PROGRAM(OneBoneVs3WLast),CLIENT_PROGRAM(OneBoneVs3WMid),CLIENT_PROGRAM(OneBoneVs2)};
    for(const auto& p:variants)assert(oneBoneTemplate(p)&&!NorthlightReplayBounds::SkinEnvelope::supports(p)&&p.skinned&&p.paletteBase==31);
    const Program none=CLIENT_PROGRAM(NoBoneVs3);assert(!oneBoneTemplate(none)&&!none.skinned);
    {auto words=load(fourBone);Program four;assert(NorthlightActorDeformation::compile(words.data(),words.size(),four)&&!oneBoneTemplate(four)&&NorthlightReplayBounds::SkinEnvelope::supports(four));}
    // Every altered program is refused: another palette row, register, constant, input, order or an extra move.
    const Program& base=variants[0];unsigned refused=0;
    auto refuse=[&](auto change){Program p=base;change(p);assert(!oneBoneTemplate(p));++refused;};
    refuse([](Program& p){p.operations[5].source[0].token+=1;}); /* dp4 z reads c34 */
    refuse([](Program& p){std::swap(p.operations[4],p.operations[5]);}); /* z before y */
    refuse([](Program& p){p.operations[3].source[0].address=0;}); /* no a0 addressing */
    refuse([](Program& p){p.operations[2].source[0].token=0x80000000u;}); /* mova from r0, mul wrote r1 */
    refuse([](Program& p){p.operations[1].source[0].token=0xa0550000u;}); /* mul by c0.y */
    refuse([](Program& p){for(auto& d:p.definitions)if(d.reg==0)d.value[0]=4;}); /* stride 4 */
    refuse([](Program& p){p.operations.erase(p.operations.begin());}); /* no w */
    refuse([](Program& p){p.operations.push_back(p.operations[0]);}); /* two w moves */
    refuse([](Program& p){p.inputs.push_back({3,1,0});}); /* a BLENDWEIGHT input */
    refuse([](Program& p){p.paletteBase=34;});refuse([](Program& p){p.positionRegister=1;});refuse([](Program& p){p.major=1;});
    std::printf("PASS one-influence program: 4 real client variants (w move first/last/middle, t=r0/r1, vs_2_0 swapped mul) are the exact template, four-weight/zero-influence are not, %u altered programs refused\n",refused);
}
// The game's palette slot: view * world (model->world M row-major column vector, translation t), as float rows.
static void slot(const Camera& c,const double* eye,const float* M,const float* t,float* rows){
    for(unsigned row=0;row<3;++row){const float* v=c.inverse+4*row;double w=0;
        for(unsigned j=0;j<3;++j){double s=0;for(unsigned k=0;k<3;++k)s+=double(v[k])*M[k*3+j];rows[4*row+j]=float(s);}
        for(unsigned k=0;k<3;++k)w+=double(v[k])*(double(t[k])-eye[k]);rows[4*row+3]=float(w);}}
// A board: one row of `quads` quads, game layout: float3 POSITION, UBYTE4N BLENDWEIGHT (255,0,0,0),
// UBYTE4 BLENDINDICES (0 and junk in the unread lanes).
static std::shared_ptr<NorthlightDrawSnapshot::Mesh> board(unsigned quads,float width,float height){auto m=std::make_shared<NorthlightDrawSnapshot::Mesh>();
    const unsigned nx=quads+1,ny=2;m->vertexCount=nx*ny;m->streams[0].stride=20;m->streams[0].bytes.resize(size_t(m->vertexCount)*20);
    for(unsigned y=0;y<ny;++y)for(unsigned x=0;x<nx;++x){auto* out=m->streams[0].bytes.data()+size_t(y*nx+x)*20;const float p[3]={width*(float(x)/quads-.5f),.05f*(y&1),-height*float(y)/(ny-1)};
        std::memcpy(out,p,12);out[12]=255;out[13]=out[14]=out[15]=0;out[16]=0;out[17]=7;out[18]=3;out[19]=9;}
    m->indexed=true;m->topology=D3DPT_TRIANGLELIST;
    for(unsigned y=0;y+1<ny;++y)for(unsigned x=0;x<quads;++x){const std::uint32_t a=y*nx+x,b=a+1,c=a+nx,d=c+1;for(auto i:{a,b,c,b,d,c})m->indices.push_back(i);}
    m->primitiveCount=unsigned(m->indices.size()/3);return m;}
static const std::vector<D3DVERTEXELEMENT9> gameLayout={{0,0,2,0,0,0},{0,12,8,0,1,0},{0,16,5,0,2,0},{0xff,0,17,0,0,0}};
struct Prop {std::uint64_t key=0;float M[9]={},t[3]={};std::shared_ptr<NorthlightDrawSnapshot::Mesh> mesh;bool placed=false;unsigned appear=0,screens=0;bool remembered=false;};
// Palette slot 0 = view * world, every other slot stale data (another model's bones).
static void propConstants(const Prop& p,const Camera& c,const double* eye,std::mt19937& rng,std::vector<float>& constants){
    constants.assign(1024,0.f);std::uniform_real_distribution<float> junk(-50,50);for(unsigned r=34;r<256;++r)for(unsigned k=0;k<4;++k)constants[4*r+k]=junk(rng);
    constants[0]=3;constants[1]=1;slot(c,eye,p.M,p.t,constants.data()+4*31);}
// rigidMemoryObserve for props: the bone (RigidBoneCache, the renderer's rigidBones), W (worldBone), the
// shape (mixShape), the static screen (the index), then store() of a copy (the constant bank).
struct PropScene {
    const Program& program;Reg reg;const PlacementIndex& index;const std::vector<std::array<float,12>>& placements;std::vector<Prop>& props;
    std::vector<Observation> obs;std::vector<size_t> who;std::mt19937 rng{172};NorthlightActorDeformation::RigidBoneCache bones;unsigned now=1000,screens=0;
    PropScene(const Program& p,const PlacementIndex& x,const std::vector<std::array<float,12>>& pl,std::vector<Prop>& v):program(p),index(x),placements(pl),props(v){}
    int covered(const Observation& o){if(!index.complete)return -1;return index.find(o.world+9,.05f,[&](std::uint32_t i){return staticPlacement(o.world+9,o.world,placements[i].data()+9,placements[i].data());})?1:0;}
    void frame(const double* eye,const Camera& c,const float* pivot,bool visible=true,unsigned serial=0,unsigned dt=16){
        now+=dt;obs.clear();who.clear();std::vector<std::vector<float>> banks;
        if(visible)for(size_t n=0;n<props.size();++n){auto& p=props[n];if(p.appear>serial)continue;banks.emplace_back();propConstants(p,c,eye,rng,banks.back());
            const float bone=bones.scanMesh(*p.mesh,gameLayout.data(),gameLayout.size());assert(bone==0);
            Observation o;o.shape=ShapeSeed;mixShape(o.shape,reinterpret_cast<const void*>(p.key),&gameLayout,p.mesh->vertexCount,p.mesh->primitiveCount,p.mesh->byteSize());
            o.draws=1;o.triangles=p.mesh->primitiveCount;o.bytes=p.mesh->byteSize();assert(worldBone(banks.back().data()+4*31,c.inverse,o.world));obs.push_back(o);who.push_back(n);}
        reg.frame(obs,nullptr,0,now,pivot,c.inverse,c.projection,true,[&](size_t k){++screens;++props[who[k]].screens;return covered(obs[k]);});
        for(size_t k=0;k<obs.size();++k)if(obs[k].action!=Observation::None){Payload x;x.constants=banks[k];x.bone=0;if(reg.store(obs[k],std::move(x),now))props[who[k]].remembered=true;}
    }
};
// A Stormwind shop sign drawn with the real one-influence program, the camera orbiting the player the whole
// time (every palette row changes each frame): remembered after 2 s, then drawn 120 s while the camera looks
// away; the copy rebased to each new view places every vertex exactly (the program evaluated on the CPU).
static void realSign(){
    const Program program=CLIENT_PROGRAM(OneBoneVs3);PlacementIndex index;std::vector<std::array<float,12>> placements;int token=0;index.reset(&token,1);index.complete=true;
    std::vector<Prop> props(1);auto& s=props[0];s.key=0x5167;rotZ(.7f,1,s.M);s.t[0]=-8850.31f;s.t[1]=612.74f;s.t[2]=104.2f;s.mesh=board(75,1.6f,1.1f);
    assert(s.mesh->primitiveCount==150); /* the HD sign board: 150 triangles */
    PropScene scene{program,index,placements,props};const float pivot[3]={-8856.f,610.f,98.5f};const double player[3]={-8856,610,99.5};unsigned frames=0;
    auto orbit=[&](double yaw,double* eye){eye[0]=player[0]-12*std::cos(yaw);eye[1]=player[1]-12*std::sin(yaw);eye[2]=player[2]+5;return looking(eye,player);};
    for(double yaw=0;frames<400&&!s.remembered;++frames,yaw+=.02){double eye[3];const Camera c=orbit(yaw,eye);scene.frame(eye,c,pivot);}
    assert(s.remembered&&frames==Settle&&s.screens==1&&scene.reg.stats().remembered==1);
    double worst=0;unsigned drawn=0;
    for(unsigned i=0;i<120;++i){double eye[3];orbit(.5*i,eye);const double back[3]={eye[0]+(eye[0]-player[0]),eye[1]+(eye[1]-player[1]),eye[2]};const Camera c=looking(eye,back);
        scene.frame(eye,c,pivot,false,0,1000); /* 120 s turned away: the game draws nothing */
        scene.reg.forAbsent([&](Reg::Entry& e){++drawn;auto constants=e.payload.constants;float rows[12];assert(rebase(e.world,c.inverse,rows));std::memcpy(constants.data()+4*(31+3*e.payload.bone),rows,48);
            std::vector<NorthlightActorDeformation::Position> world;assert(NorthlightActorDeformation::worldPositions(program,*s.mesh,gameLayout.data(),gameLayout.size(),constants.data(),c.inverse,world));
            for(size_t v=0;v<s.mesh->vertexCount;++v){float x[3];std::memcpy(x,s.mesh->streams[0].bytes.data()+v*20,12);
                const double e3[3]={s.M[0]*double(x[0])+s.M[1]*double(x[1])+s.M[2]*double(x[2])+s.t[0],s.M[3]*double(x[0])+s.M[4]*double(x[1])+s.M[5]*double(x[2])+s.t[1],s.M[6]*double(x[0])+s.M[7]*double(x[1])+s.M[8]*double(x[2])+s.t[2]};
                worst=std::max({worst,std::fabs(world[v].x-e3[0]),std::fabs(world[v].y-e3[1]),std::fabs(world[v].z-e3[2])});}});}
    assert(drawn==120&&worst<5e-3&&scene.reg.entries().size()==1);
    std::printf("PASS real sign: one-influence program, camera orbiting, remembered after %u frames (%u ms), drawn 120 s turned away, rebased vertices within %.4f yd of its placement\n",frames,frames*16,worst);
}
// Static-doodad flood: real Stormwind trade-district / Goldshire placements from the world cache (WMO
// doodads scaled .4-2.2 and ADT doodads) plus rotated/scaled synthetic doodads, all drawn at rest with the
// one-influence program (palette = view * placement): each is screened once and never remembered; the
// server-spawned signs among them, and a spawned crate beside its static twin, are all remembered.
struct RealPlacement {unsigned category;const char* model;float m[9],t[3];};
static const RealPlacement tradeDistrict[]={
#include "stormwind_rigid_placements.inc" /* written by test_rigid_memory.py from the tester's world cache */
};
static void staticFlood(){
    const Program program=CLIENT_PROGRAM(OneBoneVs3);PlacementIndex index;std::vector<std::array<float,12>> placements;int token=0;
    std::vector<Prop> props;std::mt19937 rng(1461);std::uniform_real_distribution<float> spread(-90,90),angle(-3.1f,3.1f),scale(.4f,2.2f),height(90,110);
    const float centre[2]={-8850,620};auto base=board(20,1,1);
    auto add=[&](const float* M,const float* t,bool placed){Prop p;p.key=0x1000+props.size()%37;std::memcpy(p.M,M,36);std::memcpy(p.t,t,12);p.mesh=base;p.placed=placed;props.push_back(p);
        if(placed){std::array<float,12> x;std::memcpy(x.data(),M,36);std::memcpy(x.data()+9,t,12);placements.push_back(x);}};
    for(const auto& r:tradeDistrict){const bool goldshire=r.t[0]<-9000;const float t[3]={goldshire?r.t[0]+610.f:r.t[0],goldshire?r.t[1]+560.f:r.t[1],r.t[2]};add(r.m,t,true);}
    auto flood=[&](unsigned doodads,unsigned signs,unsigned appear){
        for(unsigned i=0;i<doodads;++i){float M[9];rotZ(angle(rng),scale(rng),M);const float t[3]={centre[0]+spread(rng),centre[1]+spread(rng),height(rng)};add(M,t,true);props.back().appear=appear;}
        for(unsigned i=0;i<signs;++i){float M[9];rotZ(angle(rng),1,M);const float t[3]={centre[0]+spread(rng)*.3f,centre[1]+spread(rng)*.3f,104};add(M,t,false);props.back().key=0x9000+props.size();props.back().appear=appear;}};
    flood(180,6,0);flood(200,5,300);
    {float M[9];rotZ(.3f,1.1f,M);const float a[3]={centre[0]+4,centre[1]+3,97},b[3]={a[0]+.8f,a[1]+.6f,97};add(M,a,true);props.back().key=0x7777;props.back().appear=300;
        add(M,b,false);props.back().key=0x7777;props.back().appear=300;}
    index.reset(&token,1);for(size_t i=0;i<placements.size();++i)index.add(placements[i][9],placements[i][10],placements[i][11],std::uint32_t(i));
    PropScene scene{program,index,placements,props};const float pivot[3]={centre[0],centre[1],98};const double player[3]={centre[0],centre[1],99};
    auto orbit=[&](double yaw,double* eye){eye[0]=player[0]-12*std::cos(yaw);eye[1]=player[1]-12*std::sin(yaw);eye[2]=player[2]+5;return looking(eye,player);};
    unsigned frame=0;for(;frame<1500;++frame){index.complete=frame>=150;double eye[3];const Camera c=orbit(.01*frame,eye);scene.frame(eye,c,pivot,true,frame);
        bool all=frame>=300;for(const auto& p:props)all=all&&(p.placed||p.remembered);if(all)break;}
    size_t statics=0,signs=0;
    for(const auto& p:props){if(p.placed){++statics;assert(!p.remembered&&p.screens>=1);}else{assert(p.remembered);++signs;}}
    const auto& st=scene.reg.stats();assert(signs==12&&st.statics==statics&&st.remembered==12&&scene.reg.entries().size()==12);
    const unsigned screens=scene.screens;
    for(unsigned i=0;i<600;++i){double eye[3];const Camera c=orbit(.01*(frame+i),eye);scene.frame(eye,c,pivot,true,frame+i);}
    assert(scene.screens==screens&&scene.reg.entries().size()==12&&scene.reg.stats().statics==statics); /* screened once: never asked again */
    std::printf("PASS static-doodad flood: %zu doodads (%zu real world-cache placements, scale .4-2.2) screened and never remembered, 11 signs and a spawned crate beside its static twin remembered (%u frames), %u screen calls\n",statics,sizeof tradeDistrict/sizeof*tradeDistrict,frame,screens);
}
int main(int argc,char** argv){
    assert(argc>1);settle();sticky();identity();absence();caps();rebaseRoundTrip();oneBoneProgram(argv[1]);realSign();staticFlood();
    std::puts("rigid memory: all passed");
}
