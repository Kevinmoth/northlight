// 0.3.141 persistent casters: CPU transform accuracy against the replay VS
// emulation (and an independent double-precision skin), conversion gates, and
// the registry lifecycle (still -> caster, off-screen persistence, movement,
// memory cap, cache-slot invalidation). Built by test_persistent_casters.py.
#include "persistent_casters.h"
#include "persistent_casters_0149.h" /* the harness: reference/persistent_casters-0.3.149.h in namespace NorthlightPersistentCasters0149 */
#include "replay_bounds.h"
#include "world_math.h"
#include "actor_client_programs.h" /* written by test_persistent_casters.py from the tester's client (client_fixtures.py) */
#include <cassert>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <deque>
#include <random>
#include <string>
#include <type_traits>
using namespace NorthlightPersistentCasters;
using NorthlightActorDeformation::Program;
static std::vector<NorthlightActorDeformation::Word> load(const char* path){
    FILE* f=std::fopen(path,"rb");assert(f);std::fseek(f,0,SEEK_END);long size=std::ftell(f);std::rewind(f);
    std::vector<NorthlightActorDeformation::Word> w(size_t(size)/4);assert(std::fread(w.data(),1,size_t(size),f)==size_t(size));std::fclose(f);return w;}
struct Pose {float constants[1024]={};float inverse[16]={};};
// Rotation (yaw/pitch) * scale plus translation per bone; the camera likewise.
static void rotation(float yaw,float pitch,float* m){const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
    const float r[9]={cy,-sy*cp,sy*sp, sy,cy*cp,-cy*sp, 0,sp,cp};std::memcpy(m,r,sizeof r);}
static Pose pose(std::mt19937& rng,unsigned bones,float camYaw,float camPitch,const float* eye){
    Pose p;std::uniform_real_distribution<float> a(-3.1f,3.1f),t(-20.f,20.f),s(.5f,1.5f);
    for(unsigned b=0;b<bones;++b){float r[9];rotation(a(rng),a(rng)*.5f,r);const float scale=s(rng);
        for(unsigned row=0;row<3;++row){float* c=p.constants+4*(31+3*b+row);for(unsigned k=0;k<3;++k)c[k]=r[row*3+k]*scale;c[3]=t(rng);}}
    float c[9];rotation(camYaw,camPitch,c); /* inverse view rows: camera axes in world, then the eye */
    for(unsigned row=0;row<3;++row){for(unsigned k=0;k<3;++k)p.inverse[row*4+k]=c[k*3+row];p.inverse[row*4+3]=0;}
    p.inverse[12]=eye[0];p.inverse[13]=eye[1];p.inverse[14]=eye[2];p.inverse[15]=1;return p;
}
// Mesh: float3 position, UBYTE4N weights, UBYTE4 indices (the audited layout).
static NorthlightActorGeometry::Packet packet(const Program& program,const Pose& p,unsigned vertices,unsigned bones,std::mt19937& rng,bool strip=false){
    NorthlightActorGeometry::Packet k;auto& m=k.mesh;m.vertexCount=vertices;m.streams[0].stride=20;m.streams[0].bytes.resize(size_t(vertices)*20);
    std::uniform_real_distribution<float> x(-3.f,3.f);std::uniform_int_distribution<unsigned> bone(0,bones-1),w(0,255);
    for(unsigned v=0;v<vertices;++v){auto* out=m.streams[0].bytes.data()+size_t(v)*20;const float xyz[3]={x(rng),x(rng),x(rng)};std::memcpy(out,xyz,12);
        unsigned a=w(rng),b=w(rng)%(256-a),c=255-a-b;out[12]=uint8_t(a);out[13]=uint8_t(b);out[14]=uint8_t(c);out[15]=0;
        for(unsigned l=0;l<4;++l)out[16+l]=uint8_t(bone(rng));}
    m.indexed=true;m.topology=strip?D3DPT_TRIANGLESTRIP:D3DPT_TRIANGLELIST;m.primitiveCount=strip?vertices-2:vertices/3;
    for(unsigned i=0;i<(strip?vertices:m.primitiveCount*3);++i)m.indices.push_back(i);
    k.elements={{0,0,2,0,0,0},{0,12,8,0,1,0},{0,16,5,0,2,0},{0xff,0,17,0,0,0}};k.position=program;
    std::memcpy(k.constants.data(),p.constants,sizeof p.constants);std::memcpy(k.inverseView.data(),p.inverse,64);k.material.alphaCutoff=-1;return k;
}
static void transform(const char* shader){
    auto words=load(shader);Program program;assert(NorthlightActorDeformation::compile(words.data(),words.size(),program));
    assert(NorthlightReplayBounds::SkinEnvelope::supports(program)&&program.paletteBase==31); /* the audited client template */
    std::mt19937 rng(141);double worst=0,worstLight=0;unsigned checked=0;
    for(unsigned trial=0;trial<24;++trial){const float eye[3]={-8767.f+trial,602.f,96.f};const Pose p=pose(rng,6,.3f*trial,.2f,eye);
        Job job;job.id=trial+1;job.packets.push_back(packet(program,p,300,6,rng,trial&1));job.packets.push_back(packet(program,p,90,6,rng));
        job.bones=6;const auto r=convert(job);assert(r.failure==Failure::None&&r.mesh&&r.bones==6);
        const auto& mesh=*r.mesh;assert(mesh.batches.size()==1&&mesh.batches[0].material==0&&mesh.materials[0].alphaCutoff<0);
        // Every referenced vertex, exactly once, in first-reference order.
        assert(mesh.vertices.size()==300+90);assert(mesh.indices.size()==size_t(trial&1?298*3:300)+90);
        // (1) Independent double skin (sum_l w_l * M_idx_l * p, then view->world).
        // (2) The replay bounds emulation (all vertices, exact error model) encloses it.
        size_t out=0;for(const auto& k:job.packets){const auto& m=k.mesh;
            NorthlightReplayBounds::Budget budget;NorthlightReplayBounds::Bounds bounds;const D3DVERTEXELEMENT9* e=k.elements.data();
            assert(NorthlightReplayBounds::calculate(program,m,e,k.elements.size(),p.constants,p.inverse,budget,bounds)==NorthlightReplayBounds::Status::Valid&&bounds.valid);
            std::vector<unsigned char> firstUse(m.vertexCount,0);
            for(unsigned idx:m.indices){if(firstUse[idx])continue;firstUse[idx]=1;
                const auto* v=m.streams[0].bytes.data()+size_t(idx)*20;float xyz[3];std::memcpy(xyz,v,12);double view[3]={};
                for(unsigned l=0;l<4;++l){const double weight=(l<3?v[12+l]:v[15])/255.0;const float* M=p.constants+4*(31+3*v[16+l]);
                    for(unsigned row=0;row<3;++row)view[row]+=weight*(double(M[row*4])*xyz[0]+double(M[row*4+1])*xyz[1]+double(M[row*4+2])*xyz[2]+M[row*4+3]);}
                double world[3];for(unsigned a=0;a<3;++a)world[a]=view[0]*p.inverse[a]+view[1]*p.inverse[4+a]+view[2]*p.inverse[8+a]+p.inverse[12+a];
                const auto& w=mesh.vertices[out++].position;const float got[3]={w.x,w.y,w.z};
                for(unsigned a=0;a<3;++a){worst=std::max(worst,std::fabs(got[a]-world[a]));assert(got[a]>=bounds.low[a]&&got[a]<=bounds.high[a]);}
                // (3) The cached-map projection of the world vertex equals the replay
                // path's projection (view position times replayProjection rows).
                float light[16];NorthlightWorldMath::shadowMatrixFrom(NorthlightWorldMath::shadowFrame(NorthlightGI::Vec3(eye[0],eye[1],eye[2]),NorthlightGI::normalized(NorthlightGI::Vec3(.3f,.2f,-.9f)),48.f),48.f,light);
                float rows[16];NorthlightWorldMath::replayProjection(p.inverse,light,rows);
                for(unsigned c=0;c<4;++c){const double a=double(got[0])*light[c]+double(got[1])*light[4+c]+double(got[2])*light[8+c]+light[12+c];
                    const double b=view[0]*rows[c*4]+view[1]*rows[c*4+1]+view[2]*rows[c*4+2]+rows[c*4+3]; /* transposed: the shader's dp4 rows */worstLight=std::max(worstLight,std::fabs(a-b));}
                ++checked;}}
        assert(out==mesh.vertices.size());
        for(const auto& v:mesh.vertices){assert(v.position.x>=mesh.low.x&&v.position.x<=mesh.high.x&&v.position.z>=mesh.low.z&&v.position.z<=mesh.high.z);}
    }
    // World coordinates near 9000 yd: float32 spacing is ~1e-3.
    assert(worst<5e-3&&worstLight<5e-4);
    std::printf("PASS transform: %u vertices vs double skin max %.2e yd, light-clip max %.2e, all inside replay-bounds emulation\n",checked,worst,worstLight);
    // World-vs-view invariance: the same world pose captured from a second camera
    // (palette rows re-expressed in that camera's view) converts to the same mesh.
    {const float eye[3]={100,-40,20};Pose a=pose(rng,6,.4f,.1f,eye);Job ja;ja.packets.push_back(packet(program,a,200,6,rng));
        const float eye2[3]={-300,700,55};Pose b=pose(rng,6,2.1f,-.3f,eye2);std::memcpy(b.constants,a.constants,sizeof a.constants);
        // view2 = world * inverse(inverseView2) = view1 * inverseView1 * inverse(inverseView2); bone rows map model->view.
        for(unsigned bone=0;bone<6;++bone){float world[12];assert(worldBone(a.constants+4*(31+3*bone),a.inverse,world));
            for(unsigned row=0;row<3;++row){float* c=b.constants+4*(31+3*bone+row);
                for(unsigned col=0;col<3;++col)c[col]=world[col*3+0]*b.inverse[row*4+0]+world[col*3+1]*b.inverse[row*4+1]+world[col*3+2]*b.inverse[row*4+2];
                c[3]=(world[9]-b.inverse[12])*b.inverse[row*4+0]+(world[10]-b.inverse[13])*b.inverse[row*4+1]+(world[11]-b.inverse[14])*b.inverse[row*4+2];}}
        Job jb=ja;std::memcpy(jb.packets[0].constants.data(),b.constants,sizeof b.constants);std::memcpy(jb.packets[0].inverseView.data(),b.inverse,64);
        const auto ra=convert(ja),rb=convert(jb);assert(ra.mesh&&rb.mesh&&ra.mesh->vertices.size()==rb.mesh->vertices.size());double d=0;
        for(size_t i=0;i<ra.mesh->vertices.size();++i){const auto& x=ra.mesh->vertices[i].position;const auto& y=rb.mesh->vertices[i].position;d=std::max({d,double(std::fabs(x.x-y.x)),double(std::fabs(x.y-y.y)),double(std::fabs(x.z-y.z))});}
        assert(d<2e-3);std::printf("PASS world-vs-view invariance: two cameras, same world mesh (max %.2e yd)\n",d);}
    // Gates: alpha without texture, vertex cap, bad topology.
    const float eye[3]={0,0,0};const Pose p=pose(rng,40,0,0,eye);Result r;
    Job alpha;alpha.packets.push_back(packet(program,p,30,4,rng));alpha.packets[0].alphaTest=true;alpha.packets[0].material.alphaCutoff=.5f;assert(convert(alpha).failure==Failure::Alpha);
    Job big;big.packets.push_back(packet(program,p,300,4,rng));big.maxVertices=100;assert(convert(big).failure==Failure::Vertices);
    Job empty;assert(convert(empty).failure==Failure::Empty);
    // Alpha-tested draw with UVs and a decoded texture: its own material and batch.
    Program uv;assert(NorthlightActorDeformation::compile(words.data(),words.size(),uv,true));
    Job cut;cut.packets.push_back(packet(program,p,30,4,rng));auto k=packet(program,p,30,4,rng);k.alphaTest=k.hasUV=true;k.uv=uv;k.material.alphaCutoff=.5f;k.material.addressU=3;
    k.elements={{0,0,2,0,0,0},{0,12,8,0,1,0},{0,16,5,0,2,0},{0xff,0,17,0,0,0}};
    // The client shader reads its UV from TEXCOORD0; give the vertices one.
    bool needsUV=false;for(const auto& in:uv.inputs)needsUV=needsUV||in.usage==5;
    if(needsUV){auto& m=k.mesh;std::vector<uint8_t> bytes(size_t(m.vertexCount)*28);for(unsigned v=0;v<m.vertexCount;++v){std::memcpy(bytes.data()+v*28,m.streams[0].bytes.data()+v*20,20);const float t[2]={.25f,.75f};std::memcpy(bytes.data()+v*28+20,t,8);}
        m.streams[0].bytes=bytes;m.streams[0].stride=28;k.elements.insert(k.elements.end()-1,D3DVERTEXELEMENT9{0,20,1,0,5,0});}
    k.texture.width=k.texture.height=2;k.texture.pitch=8;k.texture.format=NorthlightActorTexture::Format::BGRA8;k.texture.bytes={0,0,0,255,0,0,0,0,0,0,0,255,0,0,0,0};
    cut.packets.push_back(k);r=convert(cut);
    assert(r.failure==Failure::None&&r.mesh->batches.size()==2&&r.mesh->materials.size()==2&&r.mesh->materials[1].alphaCutoff==.5f&&r.mesh->materials[1].rgba[3]==255&&r.mesh->materials[1].rgba[7]==0&&r.mesh->materials[1].addressU==3);
    assert(r.mesh->gpuBytes()==r.mesh->vertices.size()*32+r.mesh->indices.size()*2+16);
    std::puts("PASS conversion gates: alpha needs exact UV+texture, vertex cap, empty, cutout material batch");
}
// ---- registry lifecycle -------------------------------------------------
// Observed frames at 16 ms until a new identity is still long enough (time and frame thresholds).
static unsigned stillSteps(const Tuning& t){return std::max(t.stillFrames,(t.stillMs+15)/16)+1;}
// Poses live in a pool for the whole test (observations point into it).
static std::deque<std::vector<float>> poses;
static const float* stillPose(float x,float y,float z,unsigned bones){poses.emplace_back(12*bones,0.f);auto& m=poses.back();
    for(unsigned b=0;b<bones;++b){float* q=m.data()+12*b;q[0]=q[4]=q[8]=1;q[9]=x+.3f*b;q[10]=y;q[11]=z+1;}return m.data();}
static Observation actor(std::uint64_t key,float x,float y,float z,std::size_t draws=2,float distance=10,unsigned bones=3){
    Observation o;o.key=key;o.root[0]=x;o.root[1]=y;o.root[2]=z;o.axes[0]=o.axes[4]=o.axes[8]=1;o.count=draws;o.first=0;o.end=draws;o.distanceSquared=distance*distance;o.vertices=500;o.triangles=400;
    o.pose=stillPose(x,y,z,bones);o.poseBones=bones;return o;}
static std::shared_ptr<Mesh> box(float x,float y,float z,std::size_t bytes=0){auto m=std::make_shared<Mesh>();m->materials.emplace_back();m->materials[0].alphaCutoff=-1;
    m->low=Vec3(x-1,y-1,z);m->high=Vec3(x+1,y+1,z+3);m->vertices.resize(bytes?bytes/32:100);m->indices.resize(300);m->batches.push_back({0,300,0});return m;}
struct Camera {float inverse[16]={};float projection[3]={1.5f,2.f,-1.f};};
// Looking along +x from `eye` (view z = -forward, projection[2]=-1 as the client's).
static Camera camera(float ex,float ey,float ez,bool towardPlusX){Camera c;const float s=towardPlusX?1.f:-1.f;
    c.inverse[0]=0;c.inverse[1]=-s;c.inverse[2]=0; /* right */c.inverse[4]=0;c.inverse[5]=0;c.inverse[6]=1; /* up */
    c.inverse[8]=-s;c.inverse[9]=0;c.inverse[10]=0; /* back */c.inverse[12]=ex;c.inverse[13]=ey;c.inverse[14]=ez;c.inverse[15]=1;return c;}
static void light(float cx,float cy,float* m){NorthlightWorldMath::shadowMatrixFrom(NorthlightWorldMath::shadowFrame(NorthlightGI::Vec3(cx,cy,0),NorthlightGI::normalized(NorthlightGI::Vec3(.3f,.2f,-.9f)),192.f),240.f,m);}
static void lifecycle(){
    Registry reg;const auto& t=reg.tuning();std::vector<size_t> cand;unsigned now=1000;const float pivot[3]={0,0,0};
    Camera away=camera(0,0,2,false),toward=camera(0,0,2,true);
    auto step=[&](std::vector<Observation>& obs,const Camera& c,bool complete=true){now+=16;reg.frame(obs,now,pivot,c.inverse,c.projection,complete,cand);};
    // Still for StillFrames -> candidate exactly then.
    std::vector<Observation> obs={actor(7,30,0,0)};unsigned frames=0;
    for(;frames<400&&cand.empty();++frames)step(obs,away);
    assert(frames==stillSteps(t)&&obs[0].candidate);
    // Not on a frame missing parts seen while still (capture shortfall), whatever `complete` says.
    obs[0].count=1;step(obs,away,false);assert(cand.empty());obs[0].count=2;step(obs,away,false);assert(cand.size()==1);step(obs,away);assert(cand.size()==1);
    const float vertex[3]={30,0,1};const auto id=reg.begin(obs[0],100000,now,vertex);assert(id&&reg.usedBytes()==100000);
    step(obs,away);assert(cand.empty()&&!obs[0].caster); /* pending: not a caster yet, no second job */
    Result r;r.id=id;r.mesh=box(30,0,0);assert(reg.accept(std::move(r),now));assert(reg.nextUpload()&&reg.nextUpload()->id==id);
    reg.ready(id,5000);assert(reg.usedBytes()==5000&&!reg.nextUpload());
    float m[16];light(0,0,m);assert(reg.signature(m)==0); /* ready, not yet published: not drawn, replay stays */
    step(obs,away);assert(!obs[0].caster&&reg.signature(m)); /* published this frame: in the cache, replay still drawn */
    step(obs,away);assert(obs[0].caster==id);const auto signature=reg.signature(m);assert(signature);Content slot;reg.record(m,slot);assert(committed(slot,id));
    // (a) The game stops drawing it (camera turned): it keeps casting.
    std::vector<Observation> none;for(unsigned i=0;i<600;++i)step(none,away);
    assert(reg.count(Registry::State::Ready)==1&&reg.signature(m)==signature);
    // Back in view and unchanged: same caster, no redraw.
    step(obs,away);assert(obs[0].caster==id&&reg.signature(m)==signature);
    // Waving parts: a sampled vertex within LooseTolerance keeps it; the root decides.
    const float waving[3]={30,1.5f,1};reg.vertex(id,waving,now);assert(reg.count(Registry::State::Ready)==1);
    // (b) It moves: removed at once, returns to the replay path, box reported for the dirty rect, never converted again.
    obs[0].root[0]+=.2f;step(obs,away);assert(!obs[0].caster&&reg.count(Registry::State::Ready)==0);
    std::vector<std::uint32_t> removed;reg.takeRemoved(removed);assert(removed.size()==1&&removed[0]==id);
    std::vector<std::pair<Vec3,Vec3>> dirty;assert(reg.changedBounds(m,slot,[&](Vec3 a,Vec3 b){dirty.push_back({a,b});}));assert(dirty.size()==1&&dirty[0].first.x==29&&reg.signature(m)==0);
    for(unsigned i=0;i<400;++i)step(obs,away);assert(cand.empty()); /* moved once: mobile for good */
    std::puts("PASS lifecycle: still 2 s (root + full pose) -> candidate, all-parts gate, pending/ready, absent 600 frames keeps casting, waving vertex kept, move removes + dirty box + stays dynamic");
    // Sampled vertex beyond LooseTolerance (a door swung, a part detached): removed.
    std::vector<Observation> fence={actor(11,50,5,0)};for(unsigned i=0;i<stillSteps(t);++i)step(fence,away);assert(cand.size()==1);
    auto fid=reg.begin(fence[0],1000,now,vertex);Result fr;fr.id=fid;fr.mesh=box(50,5,0);assert(reg.accept(std::move(fr),now));reg.ready(fid,1000);
    const float swung[3]={33,0,1};reg.vertex(fid,swung,now);assert(reg.count(Registry::State::Ready)==0);
    // Unseen for UnseenMs: removed; in-view absence (a despawn): removed after AbsentInView frames.
    Registry r2;now=5000;std::vector<Observation> a={actor(3,20,0,0)};
    auto step2=[&](Registry& g,std::vector<Observation>& o,const Camera& c,unsigned dt=16,bool complete=true){now+=dt;g.frame(o,now,pivot,c.inverse,c.projection,complete,cand);};
    for(unsigned i=0;i<stillSteps(t);++i)step2(r2,a,away);auto aid=r2.begin(a[0],1000,now,nullptr);Result ar;ar.id=aid;ar.mesh=box(20,0,0);r2.accept(std::move(ar),now);r2.ready(aid,1000);
    std::vector<Observation> gone;for(unsigned i=0;i<200;++i)step2(r2,gone,toward,16,false);assert(r2.count(Registry::State::Ready)==1); /* incomplete capture: never judged absent */
    unsigned absent=0;while(r2.count(Registry::State::Ready)==1&&absent<200){step2(r2,gone,toward);++absent;}
    assert(absent==std::max(t.absentInView,(t.absentInViewMs+15)/16+1)&&r2.stats().removed[InView]==1);
    Registry r3;std::vector<Observation> b={actor(4,20,0,0)};for(unsigned i=0;i<stillSteps(t);++i)step2(r3,b,away);auto bid=r3.begin(b[0],1000,now,nullptr);Result br;br.id=bid;br.mesh=box(20,0,0);r3.accept(std::move(br),now);r3.ready(bid,1000);
    step2(r3,gone,away,t.unseenMs-1);assert(r3.count(Registry::State::Ready)==1);step2(r3,gone,away,2);assert(r3.stats().removed[Unseen]==1);
    // New key at the same root (LOD/snapshot change) and more parts than converted.
    Registry r4;std::vector<Observation> c={actor(5,20,0,0)};for(unsigned i=0;i<stillSteps(t);++i)step2(r4,c,away);auto cid=r4.begin(c[0],1000,now,nullptr);Result cr;cr.id=cid;cr.mesh=box(20,0,0);r4.accept(std::move(cr),now);r4.ready(cid,1000);
    std::vector<Observation> rekey={actor(6,20,0,0)};step2(r4,rekey,away);assert(r4.stats().removed[Rekey]==1&&!rekey[0].caster);
    // A separate model already known at the same root (tent + banner) never removes it.
    Registry r4b;std::vector<Observation> pairAt={actor(14,20,0,0),actor(15,20,0,0)};for(unsigned i=0;i<stillSteps(t);++i)step2(r4b,pairAt,away);
    auto pid=r4b.begin(pairAt[0],1000,now,nullptr);Result pr;pr.id=pid;pr.mesh=box(20,0,0);r4b.accept(std::move(pr),now);r4b.ready(pid,1000);
    std::vector<Observation> banner={actor(15,20,0,0)};for(unsigned i=0;i<10;++i)step2(r4b,banner,away);assert(r4b.count(Registry::State::Ready)==1&&r4b.stats().removed[Rekey]==0);
    Registry r5;std::vector<Observation> d5={actor(8,20,0,0,2)};for(unsigned i=0;i<stillSteps(t);++i)step2(r5,d5,away);auto did=r5.begin(d5[0],1000,now,nullptr);Result dr;dr.id=did;dr.mesh=box(20,0,0);r5.accept(std::move(dr),now);r5.ready(did,1000);
    std::vector<Observation> more={actor(8,20,0,0,3)};step2(r5,more,away);assert(r5.stats().removed[Shape]==1);
    // Range and map change (clear): everything returns to the replay path, content epoch changes.
    Registry r6;std::vector<Observation> e6={actor(9,20,0,0)};for(unsigned i=0;i<stillSteps(t);++i)step2(r6,e6,away);auto eid=r6.begin(e6[0],1000,now,nullptr);Result er;er.id=eid;er.mesh=box(20,0,0);r6.accept(std::move(er),now);r6.ready(eid,1000);
    Content before;r6.record(m,before);r6.clear();std::vector<std::uint32_t> ids;r6.takeRemoved(ids);assert(ids.size()==1&&!r6.changedBounds(m,before,[](Vec3,Vec3){}));
    // 20 FPS (or captures every 3rd frame at 60): still after 2 s, not after 120 observed frames.
    Registry slow;std::vector<Observation> s1={actor(16,20,0,0)};unsigned slowFrames=0;
    for(;slowFrames<200&&(cand.empty()||slowFrames==0);++slowFrames)step2(slow,s1,away,50);assert(slowFrames==41);
    // Rejections: bones (permanent), and the attachment rule for rigid actors.
    // Character size: >= characterBones distinct bone MATRICES -> rejected for good (event logged);
    // identical matrices at many palette slots count once.
    Registry r7;std::vector<Observation> npc={actor(10,20,0,0,2,10,30)};for(unsigned i=0;i<stillSteps(t)+400;++i)step2(r7,npc,away);
    assert(cand.empty()&&r7.stats().rejectedBones==1);std::vector<Registry::Event> ev;r7.takeEvents(ev);assert(ev.size()==1&&ev[0].rejected&&ev[0].bones==30);
    {poses.emplace_back(12*30,0.f);auto& same=poses.back();for(unsigned b=0;b<30;++b){same[12*b]=same[12*b+4]=same[12*b+8]=1;same[12*b+9]=40;}
        assert(Registry::distinctMatrices(same.data(),30)==1);Registry r7b;std::vector<Observation> one={actor(17,40,0,0)};one[0].pose=same.data();one[0].poseBones=30;
        unsigned k=0;for(;k<400&&cand.empty();++k)step2(r7b,one,away);assert(cand.size()==1&&r7b.stats().rejectedBones==0);}
    // Full pose: root still but a limb moving (idle breathing, /wave, /dance, waving cloth) never converts;
    // pose missing (renderer could not tell) never converts either.
    {Registry r8;poses.emplace_back(12*3,0.f);auto& limb=poses.back();std::memcpy(limb.data(),stillPose(60,0,0,3),12*3*4);
        std::vector<Observation> dancer={actor(18,60,0,0)};dancer[0].pose=limb.data();
        for(unsigned i=0;i<600;++i){limb[12*2+10]=.05f*std::sin(i*.2f);step2(r8,dancer,away);assert(cand.empty());}
        std::vector<Observation> blind={actor(19,70,0,0)};blind[0].pose=nullptr;blind[0].poseBones=0;for(unsigned i=0;i<600;++i){step2(r8,blind,away);assert(cand.empty());}}
    // Camera moving, pose static: the world matrices (worldBone of the view-space rows) stay put -> converts.
    {Registry r9;std::mt19937 rng(9);std::uniform_real_distribution<float> a(-3.f,3.f);float world[3][12];
        for(unsigned b=0;b<3;++b){float r[9];rotation(a(rng),a(rng)*.3f,r);for(unsigned c=0;c<3;++c)for(unsigned w=0;w<3;++w)world[b][c*3+w]=r[w*3+c];world[b][9]=150.f+float(b);world[b][10]=60;world[b][11]=10;}
        poses.emplace_back(36,0.f);auto& live=poses.back();std::vector<Observation> fence={actor(20,150,60,10,2,160)};fence[0].pose=live.data();unsigned k=0;
        for(;k<400&&cand.empty();++k){const float eyeK[3]={140.f+10*std::sin(k*.05f),40.f+k*.1f,25.f};const Pose cam=pose(rng,1,k*.03f,.2f,eyeK);
            for(unsigned b=0;b<3;++b){float rows[12];const float* inv=cam.inverse; /* view rows of this world matrix under this camera */
                for(unsigned row=0;row<3;++row){for(unsigned col=0;col<3;++col)rows[row*4+col]=world[b][col*3]*inv[row*4]+world[b][col*3+1]*inv[row*4+1]+world[b][col*3+2]*inv[row*4+2];
                    rows[row*4+3]=(world[b][9]-inv[12])*inv[row*4]+(world[b][10]-inv[13])*inv[row*4+1]+(world[b][11]-inv[14])*inv[row*4+2];}
                assert(worldBone(rows,inv,live.data()+12*b));}
            step2(r9,fence,away);}
        assert(cand.size()==1&&k==stillSteps(t));
        // Converted, then one bone starts moving: removed that frame (mobile for good).
        auto id=r9.begin(fence[0],1000,now,nullptr);Result rr;rr.id=id;rr.mesh=box(150,60,10);r9.accept(std::move(rr),now);r9.ready(id,1000);step2(r9,fence,away);step2(r9,fence,away);assert(fence[0].caster==id);
        live[12+10]+=.1f;step2(r9,fence,away);assert(!fence[0].caster&&r9.stats().removed[Moved]==1);}
    // Batched publication: two casters ready 100 ms apart join the cache in one signature change.
    {Registry rb;std::vector<Observation> two={actor(21,20,0,0),actor(22,25,0,0)};for(unsigned i=0;i<stillSteps(t);++i)step2(rb,two,away);
        std::uint32_t ids[2];for(unsigned k=0;k<2;++k){ids[k]=rb.begin(two[k],1000,now,nullptr);Result rr;rr.id=ids[k];rr.mesh=box(two[k].root[0],0,0);rb.accept(std::move(rr),now);}
        rb.ready(ids[0],1000);step2(rb,two,away);float lm[16];light(0,0,lm);const auto s1=rb.signature(lm);assert(s1&&rb.stats().publications==1);
        rb.ready(ids[1],1000);for(unsigned i=0;i<5;++i){step2(rb,two,away,20);assert(rb.signature(lm)==s1);} /* < publishMs: held back, replay drawn */
        for(unsigned i=0;i<30;++i)step2(rb,two,away,20);assert(rb.signature(lm)!=s1&&rb.stats().publications==2&&two[1].caster==ids[1]);}
    // In-view absence with every 10th frame incomplete (capture shortfall): held, not reset.
    {Registry rh;std::vector<Observation> tent={actor(23,20,0,0)};for(unsigned i=0;i<stillSteps(t);++i)step2(rh,tent,away);auto hid=rh.begin(tent[0],1000,now,nullptr);Result hr;hr.id=hid;hr.mesh=box(20,0,0);rh.accept(std::move(hr),now);rh.ready(hid,1000);
        std::vector<Observation> none;unsigned k=0;for(;k<200&&rh.count(Registry::State::Ready)==1;++k)step2(rh,none,toward,16,k%10!=9);
        assert(rh.stats().removed[InView]==1&&k<=40);
        // Shortfall frames while looking away hold the count too (turning the camera through a crowd).
        Registry rg;std::vector<Observation> tent2={actor(26,20,0,0)};for(unsigned i=0;i<stillSteps(t);++i)step2(rg,tent2,away);auto gid=rg.begin(tent2[0],1000,now,nullptr);Result gr;gr.id=gid;gr.mesh=box(20,0,0);rg.accept(std::move(gr),now);rg.ready(gid,1000);
        for(unsigned i=0;i<5;++i)step2(rg,none,toward);step2(rg,none,away,16,false);for(unsigned i=0;i<40&&rg.count(Registry::State::Ready)==1;++i)step2(rg,none,toward);assert(rg.stats().removed[InView]==1);}
    std::vector<Observation> pair={actor(12,20,0,0),actor(13,22,0,0)};
    assert(r7.attachedToActor(pair,1,[](size_t n){return n==1;}));assert(!r7.attachedToActor(pair,1,[](size_t){return true;}));assert(!r7.attachedToActor(pair,0,[](size_t n){return n==1;}));
    pair[0].caster=99;assert(!r7.attachedToActor(pair,1,[](size_t n){return n==1;})); /* beside a persistent structure: fine */
    {Registry ra;std::vector<Observation> duo={actor(24,20,0,0),actor(25,22,0,0)};step2(ra,duo,away);assert(ra.attachedToActor(duo,1,[](size_t n){return n==1;}));step2(ra,duo,away);assert(ra.stats().attachBlocked==1);}
    std::puts("PASS removal/gates: vertex swing, in-view absence (shortfall frames hold), unseen 60 s, rekey, more parts, clear/epoch, distinct bone matrices, full-pose stillness (limb motion, missing pose, moving camera), pose motion removes, batched publication, rigid attachment rule + counter");
}
static void budget(){
    Tuning tuning;tuning.budgetBytes=1u<<20;tuning.maxCasters=4;Registry reg(tuning);std::vector<size_t> cand;unsigned now=1000;const float pivot[3]={0,0,0};
    Camera c=camera(0,0,2,false);
    std::vector<Observation> obs;for(unsigned i=0;i<6;++i)obs.push_back(actor(100+i,10.f+30*i,0,0,1,10.f+30*i));
    for(unsigned i=0;i<stillSteps(tuning);++i){now+=16;reg.frame(obs,now,pivot,c.inverse,c.projection,true,cand);}
    assert(cand.size()==6&&obs[cand[0]].key==100); /* nearest first */
    std::vector<std::uint32_t> ids;
    for(size_t n:cand){auto id=reg.begin(obs[n],300000,now,nullptr);if(!id)break;ids.push_back(id);Result r;r.id=id;r.mesh=box(obs[n].root[0],0,0,300000-300*2);assert(reg.accept(std::move(r),now));reg.ready(id,300000);}
    assert(ids.size()==3&&reg.usedBytes()==900000); /* 1 MiB cap */
    // A farther actor never evicts; a clearly nearer one evicts the farthest, at most once per interval.
    // (Observation::track is valid for the frame that filled it: use this frame's list.)
    std::vector<Observation> all;
    for(unsigned i=0;i<stillSteps(tuning);++i){now+=16;all=obs;all.push_back(actor(200,5,5,0,1,5));all.push_back(actor(201,6,6,0,1,6));reg.frame(all,now,pivot,c.inverse,c.projection,true,cand);}
    const auto& farther=all[5];const auto& nearer=all[6];const auto& nearer2=all[7];
    assert(!reg.room(farther,300000,now)&&reg.begin(farther,300000,now,nullptr)==0&&reg.room(nearer,300000,now));
    const auto nid=reg.begin(nearer,300000,now,nullptr);assert(nid&&reg.stats().removed[Evicted]==1&&reg.usedBytes()==900000);
    assert(!reg.room(nearer2,300000,now)&&reg.begin(nearer2,300000,now,nullptr)==0); /* one eviction per frame */
    now+=500;reg.frame(all,now,pivot,c.inverse,c.projection,true,cand);assert(!reg.room(all[7],300000,now)); /* and per evictIntervalMs */
    now+=600;reg.frame(all,now,pivot,c.inverse,c.projection,true,cand);assert(reg.room(all[7],300000,now));
    // Count cap.
    Tuning few;few.maxCasters=1;Registry one(few);std::vector<Observation> two={actor(1,10,0,0),actor(2,40,0,0,2,40)};
    for(unsigned i=0;i<stillSteps(few);++i){now+=16;one.frame(two,now,pivot,c.inverse,c.projection,true,cand);}
    assert(one.begin(two[0],10,now,nullptr)&&!one.begin(two[1],10,now,nullptr));
    std::puts("PASS memory cap: nearest first, 1 MiB/300 KiB -> 3 casters, farther never evicts, nearer evicts farthest once per interval, count cap");
}
static void invalidation(){
    Registry reg;std::vector<size_t> cand;unsigned now=1000;const float pivot[3]={0,0,0};Camera c=camera(0,0,2,false);
    std::vector<Observation> obs={actor(1,10,0,0),actor(2,60,0,0),actor(3,-5000,0,0,2,5000)};
    for(unsigned i=0;i<stillSteps(reg.tuning());++i){now+=16;reg.frame(obs,now,pivot,c.inverse,c.projection,true,cand);}
    assert(cand.size()==2); /* beyond Range: never a candidate */
    float m[16];light(0,0,m);Content empty;reg.record(m,empty);assert(empty.valid&&empty.slices.empty()&&reg.signature(m)==0);
    std::uint32_t ids[2];for(unsigned k=0;k<2;++k){ids[k]=reg.begin(obs[k],1000,now,nullptr);Result r;r.id=ids[k];r.mesh=box(obs[k].root[0],0,0);reg.accept(std::move(r),now);}
    assert(reg.signature(m)==0); /* converted but not uploaded: not drawn, not in the signature */
    auto publish=[&]{now+=600;reg.frame(obs,now,pivot,c.inverse,c.projection,true,cand);};
    reg.ready(ids[0],1000);assert(reg.signature(m)==0);publish();const auto s1=reg.signature(m);assert(s1);
    std::vector<Vec3> boxes;reg.changedBounds(m,empty,[&](Vec3 a,Vec3){boxes.push_back(a);});assert(boxes.size()==1&&boxes[0].x==9);
    Content one;reg.record(m,one);reg.ready(ids[1],1000);publish();assert(reg.signature(m)!=s1);boxes.clear();reg.changedBounds(m,one,[&](Vec3 a,Vec3){boxes.push_back(a);});assert(boxes.size()==1&&boxes[0].x==59);
    // A caster outside a light matrix never changes that slot's signature.
    float far[16];light(3000,3000,far);assert(reg.signature(far)==0);assert(!reg.changedBounds(far,one,[](Vec3,Vec3){})); /* recorded for another matrix: full redraw */
    Content both;reg.record(m,both);assert(committed(both,ids[0])&&committed(both,ids[1])&&!committed(both,ids[1]+7)&&!committed(Content{},ids[0]));
    std::puts("PASS invalidation: signature only over visible ready casters, added/removed boxes, other slots untouched, committed()");
}
// ---- raster parity of the persistent partial (dirty-rect) cache redraw ----
// Software model of the cache pass: R32F colour = depth, D24 LESSEQUAL, static
// quads first, then the published casters in id order; the partial path clears
// the dirty rects and redraws everything that touches each rect under its
// scissor (renderer: staticDirtyRects + renderCache + drawPersistentCasters).
namespace raster {
constexpr long Size=128,Margin=2,Tile=16;constexpr size_t MaxRects=8;
struct Target {std::vector<float> color=std::vector<float>(Size*Size,1.f);std::vector<uint32_t> depth=std::vector<uint32_t>(Size*Size,0xffffffu);};
struct Quad {Vec3 low,high;};
static void quad(Target& t,const float* m,const Quad& q,const NorthlightShadowBounds::TexelRect* scissor){
    const float z=(q.low.z+q.high.z)*.5f;
    auto sx=[&](float x){return (x*m[0]+m[12]+1)*.5f*Size;};auto sy=[&](float y){return (1-(y*m[5]+m[13]))*.5f*Size;};
    const float x0=sx(q.low.x),x1=sx(q.high.x),y0=sy(q.high.y),y1=sy(q.low.y);const float d=z*m[10]+m[14];
    for(long y=0;y<Size;++y)for(long x=0;x<Size;++x){if(scissor&&(x<scissor->left||x>=scissor->right||y<scissor->top||y>=scissor->bottom))continue;
        const float px=x+.5f,py=y+.5f;if(px<x0||px>=x1||py<y0||py>=y1||d>1)continue;
        const float c=std::max(0.f,d);const uint32_t qd=uint32_t(std::lround(double(c)*16777215.0));auto& dst=t.depth[y*Size+x];if(qd<=dst){dst=qd;t.color[y*Size+x]=c;}}
}
static void render(Target& t,const float* m,const std::vector<Quad>& statics,const Registry& reg,const std::vector<NorthlightShadowBounds::TexelRect>* rects){
    if(!rects){t=Target{};}else for(const auto& r:*rects)for(long y=r.top;y<r.bottom;++y)for(long x=r.left;x<r.right;++x){t.color[y*Size+x]=1;t.depth[y*Size+x]=0xffffffu;}
    auto draw=[&](const Quad& q){if(!rects){quad(t,m,q,nullptr);return;}NorthlightShadowBounds::TexelRect f;const bool known=NorthlightShadowBounds::texelFootprint(q.low,q.high,m,Size,Margin,f);
        for(const auto& r:*rects)if(!known||NorthlightShadowBounds::intersects(f,r))quad(t,m,q,&r);};
    for(const auto& q:statics)draw(q);
    reg.forVisible(m,[&](const Registry::Entry& e){draw({e.low,e.high});});
}
}
static void parity(){
    using namespace raster;const float m[16]={.01f,0,0,0, 0,.01f,0,0, 0,0,.01f,0, 0,0,0,1};
    Registry reg;std::vector<size_t> cand;unsigned now=1000;const float pivot[3]={0,0,0};Camera c=camera(0,0,200,false);std::mt19937 rng(141);
    std::uniform_real_distribution<float> pos(-90.f,90.f),depth(10.f,80.f);
    std::vector<Quad> statics;for(unsigned i=0;i<10;++i){const float x=pos(rng),y=pos(rng),z=depth(rng);statics.push_back({Vec3(x-6,y-4,z),Vec3(x+6,y+4,z)});}
    // A caster coplanar (same D24 quantum) with a static quad: order must match the full redraw.
    std::vector<Observation> obs;for(unsigned i=0;i<14;++i){const float x=i?pos(rng):statics[0].low.x+7,y=i?pos(rng):statics[0].low.y+5;obs.push_back(actor(300+i,x,y,0,1,std::sqrt(x*x+y*y)));}
    std::vector<float> zs;for(unsigned i=0;i<obs.size();++i)zs.push_back(i?depth(rng):statics[0].low.z-3); /* box z..z+3: center = the static quad's depth */
    auto step=[&](unsigned dt){now+=dt;reg.frame(obs,now,pivot,c.inverse,c.projection,true,cand);};
    for(unsigned i=0;i<stillSteps(reg.tuning());++i)step(16);
    std::vector<std::uint32_t> ids;for(size_t n:std::vector<size_t>(cand)){auto id=reg.begin(obs[n],1000,now,nullptr);assert(id);Result r;r.id=id;
        r.mesh=box(obs[n].root[0],obs[n].root[1],zs[n]);assert(reg.accept(std::move(r),now));ids.push_back(id);}
    assert(ids.size()==obs.size());
    Target cache;Content content;render(cache,m,statics,reg,nullptr);reg.record(m,content);
    unsigned partials=0,removals=0,ghosts=0,adds=0,staticChanges=0;size_t next=0;long long texels=0;
    for(unsigned it=0;it<60;++it){const unsigned kind=it%4;std::vector<std::pair<Vec3,Vec3>> staticBoxes;std::vector<std::pair<Vec3,Vec3>> removedBoxes;
        if((kind==0||kind==3)&&next<ids.size()){reg.ready(ids[next++],1000);++adds;}
        if(kind==1){ /* move a published caster's root: removed, its old box dirty */
            for(auto& o:obs)if(o.caster){for(const auto& e:reg.entries())if(e.id==o.caster)removedBoxes.push_back({e.low,e.high});o.root[0]+=.5f;++removals;break;}}
        if(kind==2||kind==3){auto& q=statics[rng()%statics.size()];staticBoxes.push_back({q.low,q.high});const float dx=pos(rng)*.1f;q.low.x+=dx;q.high.x+=dx;staticBoxes.push_back({q.low,q.high});++staticChanges;}
        step(600); /* publish + removals */
        Target reference;render(reference,m,statics,reg,nullptr);
        std::vector<NorthlightShadowBounds::TexelRect> footprints,rects;std::vector<std::pair<Vec3,Vec3>> persistentBoxes;
        assert(reg.changedBounds(m,content,[&](Vec3 a,Vec3 b){persistentBoxes.push_back({a,b});}));
        auto foot=[&](const std::pair<Vec3,Vec3>& b){NorthlightShadowBounds::TexelRect f;assert(NorthlightShadowBounds::texelFootprint(b.first,b.second,m,Size,Margin,f));footprints.push_back(f);};
        for(const auto& b:staticBoxes)foot(b);for(const auto& b:persistentBoxes)foot(b);
        for(const auto& b:removedBoxes){bool reported=false;for(const auto& p:persistentBoxes)reported=reported||(p.first.x==b.first.x&&p.first.y==b.first.y);assert(reported);}
        NorthlightShadowBounds::dirtyRects(footprints,Size,Tile,MaxRects,rects);for(const auto& r:rects)texels+=r.area();
        // Power check: dropping the removed caster's box from the rects leaves its ghost.
        if(!removedBoxes.empty()){std::vector<NorthlightShadowBounds::TexelRect> f2,r2;for(const auto& b:staticBoxes){NorthlightShadowBounds::TexelRect f;NorthlightShadowBounds::texelFootprint(b.first,b.second,m,Size,Margin,f);f2.push_back(f);}
            for(const auto& b:persistentBoxes)if(!(b.first.x==removedBoxes[0].first.x&&b.first.y==removedBoxes[0].first.y)){NorthlightShadowBounds::TexelRect f;NorthlightShadowBounds::texelFootprint(b.first,b.second,m,Size,Margin,f);f2.push_back(f);}
            NorthlightShadowBounds::dirtyRects(f2,Size,Tile,MaxRects,r2);Target naive=cache;render(naive,m,statics,reg,&r2);ghosts+=naive.color!=reference.color;}
        render(cache,m,statics,reg,&rects);++partials;
        assert(cache.color==reference.color&&cache.depth==reference.depth); /* bit-identical to a full redraw */
        reg.record(m,content);}
    // (a removed caster inside another dirty rect, or hidden by a nearer quad, leaves no ghost even without its own box)
    assert(adds>=14&&removals>=10&&staticChanges>=25&&ghosts*5>=removals*4);
    std::printf("PASS persistent dirty-rect parity: %u partial redraws (adds %u, removals %u, static changes %u incl. add+static) bit-identical to full redraw, coplanar tie, avg rect %.1f%%; without the removed box a ghost remains (%u of %u removals)\n",
        partials,adds,removals,staticChanges,100.0*double(texels)/double(partials)/double(Size*Size),ghosts,removals);
}
static void worker(){
    // Round trip through the background thread with a real conversion.
    Worker w;Job job;job.id=42;auto shared=std::make_shared<Job>(job);w.submit(shared);shared.reset();
    std::vector<Result> out;for(unsigned i=0;i<2000&&out.empty();++i){std::this_thread::sleep_for(std::chrono::milliseconds(1));w.collect(out);}
    assert(out.size()==1&&out[0].id==42&&out[0].failure==Failure::Empty&&w.inFlight()==0);
    std::puts("PASS worker: background conversion round trip");
}
static void screen(){
    const Camera c=camera(0,0,2,true);
    assert(fullyOnScreen(c.inverse,c.projection,Vec3(19,-1,0),Vec3(21,1,3),60));
    assert(!fullyOnScreen(c.inverse,c.projection,Vec3(79,-1,0),Vec3(81,1,3),60)); /* too far to be sure it is drawn */
    assert(!fullyOnScreen(c.inverse,c.projection,Vec3(-21,-1,0),Vec3(-19,1,3),60)); /* behind */
    assert(!fullyOnScreen(c.inverse,c.projection,Vec3(9,-30,0),Vec3(11,-28,3),60)); /* beside */
    assert(!fullyOnScreen(c.inverse,c.projection,Vec3(-1,-1,0),Vec3(1,1,3),60)); /* around the camera */
    std::puts("PASS on-screen test: in front, far, behind, beside, around the camera");
}
// ---- rigid props (PersistentRigidProps) --------------------------------
static Tuning rigidTuning(bool general=false){Tuning t;t.general=general;t.rigidProps=true;return t;}
// Converted rigid caster at the observation's root (the lifecycle uses boxes; conversion is tested below).
static std::uint32_t convertNow(Registry& reg,const Observation& o,unsigned now){const auto id=reg.begin(o,1000,now,nullptr);assert(id);Result r;r.id=id;r.mesh=box(o.root[0],o.root[1],o.root[2]);assert(reg.accept(std::move(r),now));reg.ready(id,1000);return id;}
static void rigidLifecycle(){
    static const float pivot[3]={0,0,0};Camera away=camera(0,0,2,false),toward=camera(0,0,2,true);std::vector<size_t> cand;unsigned now=1000;
    auto step=[&](Registry& g,std::vector<Observation>& o,const Camera& c,unsigned dt=16,bool complete=true,const float* at=nullptr){now+=dt;g.frame(o,now,at?at:pivot,c.inverse,c.projection,complete,cand);};
    // (a) One-bone sign, still 2 s: a rigid candidate (general class off), converted, kept off-screen for 120 s.
    Registry reg(rigidTuning());const auto& t=reg.tuning();std::vector<Observation> sign={actor(40,20,0,0,2,20,1)};unsigned k=0;
    for(;k<400&&cand.empty();++k)step(reg,sign,away);assert(k==stillSteps(t)&&cand.size()==1&&sign[0].candidate&&sign[0].rigid);
    const auto id=convertNow(reg,sign[0],now);assert(reg.stats().rigidSubmitted==1&&reg.stats().rigidConverted==1&&reg.countRigid(Registry::State::Ready)==1&&reg.rigidBytes()==1000);
    step(reg,sign,away,600);step(reg,sign,away);assert(sign[0].caster==id);float m[16];light(0,0,m);const auto signature=reg.signature(m);assert(signature);
    std::vector<Observation> none;for(unsigned i=0;i<120;++i)step(reg,none,away,1000); /* 120 s unseen, camera turned away */
    assert(reg.count(Registry::State::Ready)==1&&reg.stats().removed[Unseen]==0&&reg.signature(m)==signature);
    step(reg,sign,away);assert(sign[0].caster==id); /* back in view: the same caster */
    // Despawn seen on screen: removed after the in-view absence rule (as the still class).
    unsigned absent=0;while(reg.count(Registry::State::Ready)==1&&absent<200){step(reg,none,toward);++absent;}
    assert(absent==std::max(t.absentInView,(t.absentInViewMs+15)/16+1)&&reg.stats().removed[InView]==1&&reg.stats().rigidRemoved==1);
    std::vector<Registry::Event> ev;reg.takeEvents(ev);assert(ev.size()==1&&ev[0].rigid&&ev[0].reason==InView);
    // Range (the pivot travels 420 yd away) and map change (clear).
    {Registry rr(rigidTuning());std::vector<Observation> s2={actor(41,20,0,0,2,20,1)};for(unsigned i=0;i<stillSteps(t);++i)step(rr,s2,away);convertNow(rr,s2[0],now);
        for(unsigned i=0;i<100;++i)step(rr,none,away,1000);assert(rr.count(Registry::State::Ready)==1);
        for(float x=0;x<=420;x+=10){const float far[3]={x,0,0};step(rr,none,away,16,true,far);} /* travelling (a jump would be a teleport) */assert(rr.count(Registry::State::Ready)==0&&rr.stats().removed[Range]==1);
        Registry rc(rigidTuning());std::vector<Observation> s3={actor(42,20,0,0,2,20,1)};for(unsigned i=0;i<stillSteps(t);++i)step(rc,s3,away);const auto cid=convertNow(rc,s3[0],now);
        for(unsigned i=0;i<100;++i)step(rc,none,away,1000);rc.clear();std::vector<std::uint32_t> ids;rc.takeRemoved(ids);assert(ids.size()==1&&ids[0]==cid&&rc.stats().removed[Cleared]==1);}
    // Rigid props stay subject to motion while seen: a moved sign is removed and stays dynamic.
    {Registry rm(rigidTuning());std::vector<Observation> s4={actor(43,20,0,0,2,20,1)};for(unsigned i=0;i<stillSteps(t);++i)step(rm,s4,away);const auto mid=convertNow(rm,s4[0],now);
        step(rm,s4,away,600);step(rm,s4,away);assert(s4[0].caster==mid);s4[0].root[2]+=.3f;step(rm,s4,away);assert(rm.stats().removed[Moved]==1&&!s4[0].caster);
        for(unsigned i=0;i<400;++i){step(rm,s4,away);assert(cand.empty());}}
    // Multi-bone models whose bones all sit at rest (one matrix: an event tent) are rigid props;
    // once two distinct matrices were seen (a banner that waved) never again, even when it rests.
    {Registry rt(rigidTuning());poses.emplace_back(12*4,0.f);auto& tent=poses.back();for(unsigned b=0;b<4;++b){float* q=tent.data()+12*b;q[0]=q[4]=q[8]=1;q[9]=20;q[11]=1;}
        std::vector<Observation> o={actor(44,20,0,0)};o[0].pose=tent.data();o[0].poseBones=4;unsigned n=0;for(;n<400&&cand.empty();++n)step(rt,o,away);assert(cand.size()==1&&o[0].rigid);
        Registry rw(rigidTuning());poses.emplace_back(tent);auto& flag=poses.back();std::vector<Observation> w={actor(45,20,0,0)};w[0].pose=flag.data();w[0].poseBones=4;
        for(unsigned i=0;i<200;++i){flag[12*3+10]=i<20?.3f*std::sin(i*.4f):0.f;step(rw,w,away);assert(cand.empty());}
        assert(!rw.needsPose(45,w[0].root)); /* rigid-only registry: its pose is no longer computed */}
    // (b) PersistentCasters semantics unchanged: with the rigid class off a one-matrix actor is a
    // still-class candidate (rigid=false) and leaves after unseenMs; in rigid-only mode a still
    // multi-bone actor never converts, and a character is not even posed again.
    {Registry rg;assert(rg.tuning().general&&!rg.tuning().rigidProps);std::vector<Observation> s5={actor(46,20,0,0,2,20,1)};for(unsigned i=0;i<stillSteps(t);++i)step(rg,s5,away);
        assert(cand.size()==1&&!s5[0].rigid);convertNow(rg,s5[0],now);step(rg,none,away,t.unseenMs+1);assert(rg.stats().removed[Unseen]==1&&rg.stats().rigidRemoved==0);
        Registry both(rigidTuning(true));std::vector<Observation> pair={actor(47,20,0,0,2,20,1),actor(48,30,0,0,2,30,3)};for(unsigned i=0;i<stillSteps(t);++i)step(both,pair,away);
        assert(cand.size()==2&&pair[0].rigid&&!pair[1].rigid);convertNow(both,pair[0],now);convertNow(both,pair[1],now);step(both,none,away,t.unseenMs+1);
        assert(both.stats().removed[Unseen]==1&&both.count(Registry::State::Ready)==1&&both.countRigid(Registry::State::Ready)==1);
        Registry only(rigidTuning());std::vector<Observation> npc={actor(49,20,0,0,2,20,30)};
        for(unsigned i=0;i<600;++i){step(only,npc,away);assert(cand.empty());}assert(only.stats().rejectedBones==0&&!only.needsPose(49,npc[0].root));
        // Switching classes returns everything to the replay path.
        Registry sw(rigidTuning());std::vector<Observation> s6={actor(50,20,0,0,2,20,1)};for(unsigned i=0;i<stillSteps(t);++i)step(sw,s6,away);convertNow(sw,s6[0],now);
        sw.classes(false,true);assert(sw.count(Registry::State::Ready)==1);sw.classes(true,true);assert(sw.entries().empty());std::vector<std::uint32_t> ids;sw.takeRemoved(ids);assert(ids.size()==1);}
    std::puts("PASS rigid props lifecycle: one-matrix still -> rigid candidate, kept 120 s off-screen (60 s unseen timeout exempt), in-view despawn, range, map change, motion removes; at-rest multi-bone yes, once-animated never; still class and PersistentCasters=0 paths unchanged");
}
// Held items. The weapon's root sits on its body; a sign hangs above a vendor's head.
static void rigidHeld(const char* shader){
    const float pivot[3]={0,0,0};Camera away=camera(0,0,2,false);std::vector<size_t> cand;unsigned now=1000;
    auto step=[&](Registry& g,std::vector<Observation>& o){now+=16;g.frame(o,now,pivot,away.inverse,away.projection,true,cand);};
    // (c1) A weapon carried by a walking character, or swaying with an idle stance, never becomes still.
    for(bool walking:{true,false}){Registry reg(rigidTuning());std::vector<Observation> o(2);
        for(unsigned i=0;i<900;++i){const float walk=walking?.05f*i:0.f,sway=walking?0.f:.04f*std::sin(i*.3f);
            o[0]=actor(60,20+walk,0,0,2,20,30);o[1]=actor(61,20.4f+walk+sway,.3f,1+sway,1,20,1);step(reg,o);assert(cand.empty());}}
    // (c2) Frozen body (a corpse, a stunned NPC): the weapon is still, becomes a rigid candidate and is held
    // by the body box; the renderer defers it as definitely worn (deferRigid definite): never a candidate again.
    const Observation body=actor(62,20,0,0,2,20,30),weapon=actor(63,20.45f,.35f,.95f,1,20,1);
    const float bodyLow[3]={19.6f,-.45f,0},bodyHigh[3]={20.6f,.45f,2.1f}; /* a humanoid's skinned box */
    auto bodyBox=[&](size_t k,float* l,float* hi){assert(k==0);std::memcpy(l,bodyLow,12);std::memcpy(hi,bodyHigh,12);return int(Registry::BodyKnown);};
    {Registry reg(rigidTuning());std::vector<Observation> o={body,weapon};unsigned held=0;
        for(unsigned i=0;i<2000;++i){step(reg,o);for(size_t c:cand){assert(c==1&&o[c].rigid);
            const auto h=reg.heldByBody(o,c,[&](size_t n){return n==1;},bodyBox);assert(h==Registry::Held);reg.deferRigid(o[c],now,true,true);++held;}}
        assert(held==1&&reg.entries().empty()&&reg.stats().rigidHeld==1&&reg.stats().rigidWorn==1&&reg.stats().rigidSubmitted==0);
        // Not definite (the old rule, and an Undecided hold): asked again after deferMs, every time.
        Registry again(rigidTuning());std::vector<Observation> v={body,weapon};unsigned asked=0;
        for(unsigned i=0;i<2000;++i){step(again,v);for(size_t c:cand){again.deferRigid(v[c],now,true);++asked;}}assert(asked>=10&&again.stats().rigidWorn==0);}
    // (c3) A still guard's polearm, held once: then the guard's body leaves the frustum (or is drawn alpha-blended:
    // not captured) while the polearm stays on screen, still, alone for 50 s -> never converts (sticky Held).
    {Registry reg(rigidTuning());std::vector<Observation> o={body,actor(64,20.5f,.4f,1.2f,1,20,1)};unsigned held=0,alone=0;
        for(unsigned i=0;i<400&&!held;++i){step(reg,o);for(size_t c:cand){assert(c==1&&reg.heldByBody(o,c,[&](size_t n){return n==1;},bodyBox)==Registry::Held);reg.deferRigid(o[c],now,true,true);++held;}}
        std::vector<Observation> pole={o[1]};for(unsigned i=0;i<3100;++i){step(reg,pole);assert(cand.empty());++alone;}
        assert(held==1&&alone*16>=49000&&reg.stats().rigidSubmitted==0&&reg.entries().empty()&&!reg.needsPose(64,pole[0].root));
        // Undecided (the body's box was not ready) is not sticky: once the body is gone and nothing holds it, the prop converts.
        Registry un(rigidTuning());std::vector<Observation> u={body,actor(65,21.5f,0,3.4f,1,20,1)};bool deferred=false;
        for(unsigned i=0;i<400&&!deferred;++i){step(un,u);for(size_t c:cand){assert(un.heldByBody(u,c,[&](size_t n){return n==1;},[](size_t,float*,float*){return int(Registry::BodyLater);})==Registry::Undecided);un.deferRigid(u[c],now,true,false);deferred=true;}}
        std::vector<Observation> sign={u[1]};bool converted=false;
        for(unsigned i=0;i<400&&!converted;++i){step(un,sign);for(size_t c:cand){assert(un.heldByBody(sign,c,[&](size_t n){return n==0;},bodyBox)==Registry::Free);convertNow(un,sign[c],now);converted=true;}}
        assert(deferred&&converted&&un.stats().rigidWorn==0&&un.countRigid(Registry::State::Ready)==1);}
    auto boxOf=[&](const float* l,const float* h){return [=](size_t,float* lo,float* hi){std::memcpy(lo,l,12);std::memcpy(hi,h,12);return int(Registry::BodyKnown);};};
    auto later=[](size_t,float*,float*){return int(Registry::BodyLater);};auto never=[](size_t,float*,float*){return int(Registry::BodyNever);};auto rigidOnly1=[](size_t n){return n==1;};
    Registry reg(rigidTuning());std::vector<Observation> o={body,weapon};
    auto held=[&](std::vector<Observation>& v,auto rigid,auto box){return reg.heldByBody(v,1,rigid,box)==Registry::Held;};
    auto freeOf=[&](std::vector<Observation>& v,auto rigid,auto box){return reg.heldByBody(v,1,rigid,box)==Registry::Free;};
    // Shoulder pad / helm / back-sheathed weapon: on the box surface or up to margin+slack outside.
    o[1].root[2]=2.1f+.5f+.15f*2.1f-.01f;assert(held(o,rigidOnly1,boxOf(bodyLow,bodyHigh)));
    o[1].root[2]=2.1f+.5f+.15f*2.1f+.05f;assert(freeOf(o,rigidOnly1,boxOf(bodyLow,bodyHigh)));
    // (3) A sign above a shop door, 1.5 yd out from the wall and 3.4 yd above the vendor standing below:
    // within the 4 yd radius (the still-class rule would wait indefinitely) but outside the vendor's box.
    std::vector<Observation> shop={body,actor(64,21.5f,0,3.4f,2,20,1)};
    assert(reg.attachedToActor(shop,1,rigidOnly1)&&freeOf(shop,rigidOnly1,boxOf(bodyLow,bodyHigh)));
    // Lead's case with realistic tall bodies: a sign 4 yd up and 1 yd out from a shop wall, a guard
    // (2.3 yd with helm) or a tauren vendor (2.9 yd, 1.6 wide) standing right under it: never held.
    {const float guardLow[3]={20.5f,-.5f,0},guardHigh[3]={21.5f,.5f,2.3f},taurenLow[3]={20.2f,-.8f,0},taurenHigh[3]={21.8f,.8f,2.9f};
        std::vector<Observation> door={actor(69,21,0,0,2,20,30),actor(70,21,0,4,2,20,1)};
        assert(reg.attachedToActor(door,1,rigidOnly1)&&freeOf(door,rigidOnly1,boxOf(guardLow,guardHigh))&&freeOf(door,rigidOnly1,boxOf(taurenLow,taurenHigh)));
        door[1].root[2]=3.5f;assert(freeOf(door,rigidOnly1,boxOf(guardLow,guardHigh))&&held(door,rigidOnly1,boxOf(taurenLow,taurenHigh))); /* a low sign over a tauren waits (fail-safe) */
        std::vector<Observation> sheath={door[0],actor(71,21.3f,.2f,1.4f,1,20,1)},feet={door[0],actor(72,20.6f,.6f,.05f,1,20,1)}; /* back sheath, an item at its feet */
        assert(held(sheath,rigidOnly1,boxOf(guardLow,guardHigh))&&held(feet,rigidOnly1,boxOf(guardLow,guardHigh)));}
    // Body box unknown: the still-class radius decides (held within attachRadius, free beyond it).
    // A body whose box is not ready (scan budget spent) leaves a candidate within attachReach undecided;
    // one whose box can never be known (owned snapshot, no POSITION) holds it within attachReach.
    assert(reg.heldByBody(shop,1,rigidOnly1,later)==Registry::Undecided&&held(shop,rigidOnly1,never));
    {std::vector<Observation> farShop={body,actor(65,25,0,3,2,20,1)},outOfReach={body,actor(66,33,0,3,2,20,1)};
        assert(reg.heldByBody(farShop,1,rigidOnly1,later)==Registry::Undecided&&held(farShop,rigidOnly1,never)&&freeOf(outOfReach,rigidOnly1,never)&&freeOf(outOfReach,rigidOnly1,later));
        // da5 B2: a giant's weapon 8 yd from its root with the giant's box unknown: held or undecided, never free.
        std::vector<Observation> giant={actor(67,20,0,0,2,20,30),actor(68,23,2,7,1,20,1)};
        assert(held(giant,rigidOnly1,never)&&reg.heldByBody(giant,1,rigidOnly1,later)==Registry::Undecided);
        // A known box elsewhere does not clear an undecided neighbour.
        std::vector<Observation> three={actor(69,20,0,0,2,20,30),actor(70,21.5f,0,3.4f,1,20,1),actor(71,28,0,0,2,20,30)};
        assert(reg.heldByBody(three,1,rigidOnly1,[&](size_t k,float* lo,float* hi){if(k==2)return int(Registry::BodyLater);std::memcpy(lo,bodyLow,12);std::memcpy(hi,bodyHigh,12);return int(Registry::BodyKnown);})==Registry::Undecided);}
    // A giant's weapon 8 yd from its feet (beyond the old 4 yd radius) is inside its box: held.
    {const float gl[3]={16,-3,0},gh[3]={24,3,12};std::vector<Observation> g={body,actor(66,23,2,7,1,20,1)};
        assert(!reg.attachedToActor(g,1,rigidOnly1)&&held(g,rigidOnly1,boxOf(gl,gh)));}
    // Rigid neighbours are not bodies; ready casters still are (a weapon never outlives its body's shadow); beyond attachReach nothing holds.
    assert(freeOf(o,[](size_t){return true;},boxOf(bodyLow,bodyHigh)));
    {auto c=o;c[1].root[2]=.95f;c[0].caster=7;assert(held(c,rigidOnly1,boxOf(bodyLow,bodyHigh)));
        const float bl[3]={-100,-100,-100},bh[3]={100,100,100};std::vector<Observation> distant={actor(67,40,0,0,2,20,30),actor(68,20,0,1,1,20,1)};
        assert(freeOf(distant,rigidOnly1,boxOf(bl,bh)));}
    // The body box (envelope of per-bone model boxes through the bone world matrices) encloses every skinned
    // vertex of the real client four-bone shader, and is tight for a one-bone model.
    auto words=load(shader);Program program;assert(NorthlightActorDeformation::compile(words.data(),words.size(),program));
    std::mt19937 rng(145);double slack=0;
    for(unsigned trial=0;trial<16;++trial){const float eye[3]={-8700.f+trial,600.f,90.f};const Pose p=pose(rng,trial?6:1,.2f*trial,.1f,eye);const unsigned bones=trial?6:1;
        Job job;job.packets.push_back(packet(program,p,400,bones,rng));const auto r=convert(job);assert(r.mesh);
        float boxes[6*6],worlds[6*12];for(unsigned b=0;b<bones;++b){float* x=boxes+6*b;x[0]=x[1]=x[2]=INFINITY;x[3]=x[4]=x[5]=-INFINITY;assert(worldBone(p.constants+4*(31+3*b),p.inverse,worlds+12*b));}
        const auto& m=job.packets[0].mesh;for(unsigned v=0;v<m.vertexCount;++v){const auto* q=m.streams[0].bytes.data()+size_t(v)*20;float xyz[3];std::memcpy(xyz,q,12);
            for(unsigned l=0;l<4;++l){if(!(l<3?q[12+l]:q[15]))continue;float* x=boxes+6*q[16+l];for(unsigned a=0;a<3;++a){x[a]=std::min(x[a],xyz[a]);x[3+a]=std::max(x[3+a],xyz[a]);}}}
        float lo[3],hi[3];assert(envelope(boxes,worlds,bones,lo,hi));
        for(const auto& v:r.mesh->vertices){const float w[3]={v.position.x,v.position.y,v.position.z};for(unsigned a=0;a<3;++a)assert(w[a]>=lo[a]-2e-3f&&w[a]<=hi[a]+2e-3f);}
        if(!trial){const float ml[3]={r.mesh->low.x,r.mesh->low.y,r.mesh->low.z},mh[3]={r.mesh->high.x,r.mesh->high.y,r.mesh->high.z}; /* one bone: its rotated box, at most sqrt(3)x the mesh box */
            for(unsigned a=0;a<3;++a)slack=std::max(slack,double((hi[a]-lo[a])/std::max(1e-3f,mh[a]-ml[a])));assert(slack<=1.75);}}
    std::printf("PASS rigid held rule: carried/swaying weapons never still, frozen body holds its weapon (worn: sticky, never a candidate again), polearm held once stays dynamic with its body off-screen 50 s, undecided not sticky, shoulder margin, sign above a vendor free (the radius rule held it), unknown box -> undecided (defer) or held within reach (never known), giant weapon held, rigid/caster/far neighbours; envelope encloses the four-bone skin (one-bone box ratio %.2f)\n",slack);
}
// (d) Cutout parity: the replay samples the game texture's level 0 (bilinear, no mip, its address
// modes, clip(a - cutoff)); the rigid caster samples the level <= 128 px of the same texture (the
// actorMaterial copy, decoded to A8R8G8B8 alpha) with the same filter, address modes and cutoff value.
namespace cutout {
static float sample(const std::vector<std::uint8_t>& a,unsigned size,float u,float v,bool wrap){
    auto at=[&](int x,int y){if(wrap){x=((x%int(size))+int(size))%int(size);y=((y%int(size))+int(size))%int(size);}else{x=std::clamp(x,0,int(size)-1);y=std::clamp(y,0,int(size)-1);}return a[size_t(y)*size+unsigned(x)]/255.f;};
    const float x=u*size-.5f,y=v*size-.5f;const int x0=int(std::floor(x)),y0=int(std::floor(y));const float fx=x-x0,fy=y-y0;
    return (at(x0,y0)*(1-fx)+at(x0+1,y0)*fx)*(1-fy)+(at(x0,y0+1)*(1-fx)+at(x0+1,y0+1)*fx)*fy;}
// A hanging sign's alpha: board with rounded corners, two bolt holes, two chains, 1-texel soft edges.
static std::vector<std::uint8_t> signAlpha(unsigned size){std::vector<std::uint8_t> a(size_t(size)*size);const float s=size/256.f;
    for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x){const float px=(x+.5f)/s,py=(y+.5f)/s;float d=-1e9f; /* signed "inside" distance in 256-space */
        auto rect=[&](float x0,float y0,float x1,float y1,float r){const float qx=std::fabs(px-(x0+x1)*.5f)-((x1-x0)*.5f-r),qy=std::fabs(py-(y0+y1)*.5f)-((y1-y0)*.5f-r);
            return -(std::hypot(std::max(qx,0.f),std::max(qy,0.f))+std::min(std::max(qx,qy),0.f)-r);};
        d=std::max(d,std::min(rect(20,70,236,216,18),std::min(std::hypot(px-60,py-110)-12,std::hypot(px-196,py-110)-12)));
        d=std::max(d,rect(58,0,66,72,0));d=std::max(d,rect(190,0,198,72,0));
        a[size_t(y)*size+x]=std::uint8_t(std::lround(255*std::clamp(.5f+d*s,0.f,1.f)));}
    return a;}
static std::vector<std::uint8_t> down(const std::vector<std::uint8_t>& a,unsigned size){std::vector<std::uint8_t> b(size_t(size/2)*(size/2));
    for(unsigned y=0;y<size/2;++y)for(unsigned x=0;x<size/2;++x)b[size_t(y)*(size/2)+x]=std::uint8_t((a[(2*y)*size+2*x]+a[(2*y)*size+2*x+1]+a[(2*y+1)*size+2*x]+a[(2*y+1)*size+2*x+1]+2)/4);return b;}
// The persistent path's texture: BGRA8 bytes -> NorthlightActorTexture::decode -> alpha plane.
static std::vector<std::uint8_t> copied(const std::vector<std::uint8_t>& a,unsigned size){std::vector<std::uint8_t> bgra(a.size()*4),rgba,out(a.size());
    for(size_t i=0;i<a.size();++i){bgra[4*i]=17;bgra[4*i+1]=90;bgra[4*i+2]=200;bgra[4*i+3]=a[i];}
    assert(NorthlightActorTexture::decode(bgra.data(),bgra.size(),size,size,size_t(size)*4,NorthlightActorTexture::Format::BGRA8,rgba)&&rgba.size()==a.size()*4);
    for(size_t i=0;i<a.size();++i)out[i]=rgba[4*i+3];return out;}
}
static void cutoutParity(){
    using namespace cutout;const unsigned grid=1024;
    for(float cutoff:{.5f,224/255.f})for(bool wrap:{false,true}){
        // <= 128 px texture: the copy is level 0 itself -> identical coverage.
        {const auto a=signAlpha(128);const auto c=copied(a,128);assert(c==a);unsigned diff=0;
            for(unsigned j=0;j<grid;++j)for(unsigned i=0;i<grid;++i){const float u=(i+.37f)/grid,v=(j+.61f)/grid;diff+=(sample(a,128,u,v,wrap)>=cutoff)!=(sample(c,128,u,v,wrap)>=cutoff);}assert(diff==0);}
        // 256 px texture: the copy is its 128 px mip (box filtered, as the game's chain).
        const auto a=signAlpha(256);const auto c=copied(down(a,256),128);unsigned covered=0,diff=0,far=0;
        for(unsigned j=0;j<grid;++j)for(unsigned i=0;i<grid;++i){const float u=(i+.37f)/grid,v=(j+.61f)/grid;const bool r=sample(a,256,u,v,wrap)>=cutoff,q=sample(c,128,u,v,wrap)>=cutoff;covered+=r;
            if(r==q)continue;++diff;bool edge=false; /* a mismatch must lie within 1.5 level-0 texels of the replay's own edge */
            for(int dy=-3;dy<=3&&!edge;++dy)for(int dx=-3;dx<=3&&!edge;++dx)edge=(sample(a,256,u+dx*.5f/256,v+dy*.5f/256,wrap)>=cutoff)!=r;far+=!edge;}
        assert(covered>grid*grid/3&&far==0&&diff*100<covered*2);
        std::printf("PASS cutout parity cutoff=%.3f %s: <=128 px identical; 256 px -> 128 mip differs on %.2f%% of covered samples, all within 1.5 texels of the replay edge\n",cutoff,wrap?"wrap":"clamp",100.0*diff/covered);}
}
// (a) A one-bone sign: opaque bracket + alpha-keyed board converts whole, its cutout in its own batch
// with the decoded texture alpha (the rigid-prop job keeps the cutout the still class refuses).
static void rigidSign(const char* shader){
    auto words=load(shader);Program program,uv;assert(NorthlightActorDeformation::compile(words.data(),words.size(),program)&&NorthlightActorDeformation::compile(words.data(),words.size(),uv,true));
    std::mt19937 rng(1450);const float eye[3]={-8800,640,100};const Pose p=pose(rng,1,.5f,.1f,eye);
    Job job;job.bones=1;job.packets.push_back(packet(program,p,60,1,rng));auto k=packet(program,p,240,1,rng);k.alphaTest=k.hasUV=true;k.uv=uv;k.material.alphaCutoff=224/255.f;
    bool needsUV=false;for(const auto& in:uv.inputs)needsUV=needsUV||in.usage==5;
    if(needsUV){auto& m=k.mesh;std::vector<uint8_t> bytes(size_t(m.vertexCount)*28);for(unsigned v=0;v<m.vertexCount;++v){std::memcpy(bytes.data()+v*28,m.streams[0].bytes.data()+v*20,20);const float t[2]={(v%16)/15.f,(v/16%16)/15.f};std::memcpy(bytes.data()+v*28+20,t,8);}
        m.streams[0].bytes=bytes;m.streams[0].stride=28;k.elements.insert(k.elements.end()-1,D3DVERTEXELEMENT9{0,20,1,0,5,0});}
    const auto alpha=cutout::signAlpha(128);k.texture.width=k.texture.height=128;k.texture.pitch=512;k.texture.format=NorthlightActorTexture::Format::BGRA8;k.texture.bytes.resize(128*128*4);
    for(size_t i=0;i<alpha.size();++i)k.texture.bytes[4*i+3]=alpha[i];
    job.packets.push_back(k);const auto r=convert(job);assert(r.failure==Failure::None&&r.mesh&&r.bones==1);const auto& m=*r.mesh;
    assert(m.batches.size()==2&&m.materials.size()==2&&m.materials[1].alphaCutoff==224/255.f&&m.materials[1].width==128&&m.vertices.size()==300);
    for(size_t i=0;i<alpha.size();++i)assert(m.materials[1].rgba[4*i+3]==alpha[i]); /* the copy's alpha, exactly */
    assert(m.gpuBytes()==300*32+m.indices.size()*2+128*128*4&&m.gpuBytes()<80*1024); /* the 128 px copy dominates: ~70 KiB per sign */
    std::puts("PASS rigid sign conversion: one bone, opaque + alpha-keyed batch with the replay cutoff, texture alpha exact, ~70 KiB");
}
// Rigid sub-cap, the 30 min safety timeout and the static-cache placement match.
static void rigidBudget(){
    const float pivot[3]={0,0,0};Camera away=camera(0,0,2,false);std::vector<size_t> cand;unsigned now=1000;
    Tuning t=rigidTuning(true);t.budgetBytes=1u<<20;t.rigidBudgetBytes=600000;Registry reg(t);
    auto run=[&](std::vector<Observation>& o,unsigned frames,unsigned dt=16){for(unsigned i=0;i<frames;++i){now+=dt;reg.frame(o,now,pivot,away.inverse,away.projection,true,cand);}};
    std::vector<Observation> o={actor(80,10,0,0,2,10,1),actor(81,20,0,0,2,20,1),actor(82,30,0,0,2,30,1),actor(83,60,0,0,2,60,3)};run(o,stillSteps(t));
    assert(cand.size()==4&&o[0].rigid&&o[1].rigid&&o[2].rigid&&!o[3].rigid);
    auto take=[&](const Observation& x,std::size_t bytes){if(!reg.room(x,bytes,now))return std::uint32_t(0);const auto id=reg.begin(x,bytes,now,nullptr);assert(id);Result r;r.id=id;r.mesh=box(x.root[0],0,0,bytes-600);assert(reg.accept(std::move(r),now));reg.ready(id,bytes);return id;};
    const auto a=take(o[0],250000),b=take(o[1],250000);assert(a&&b&&!take(o[2],250000)); /* rigid sub-cap: 2 x 250 KB of 600 KB */
    const auto g=take(o[3],300000);assert(g&&reg.usedBytes()==800000); /* the still class still has room */
    now+=2000;std::vector<Observation> more=o;more.push_back(actor(84,5,5,0,2,5,1));run(more,stillSteps(t));
    const auto d=take(more[4],250000);assert(d&&reg.stats().removed[Evicted]==1); /* a nearer sign evicts the farthest SIGN (20 yd), never the farther still caster (60 yd) */
    bool hasB=false,hasG=false;for(const auto& e:reg.entries()){hasB=hasB||e.id==b;hasG=hasG||e.id==g;}assert(!hasB&&hasG&&reg.rigidBytes()==500000);
    // Safety timeout: a rigid caster unseen for rigidUnseenMs goes (a despawn never seen on screen is bounded).
    {Registry rs(rigidTuning());std::vector<Observation> s1={actor(85,20,0,0,2,20,1)};for(unsigned i=0;i<stillSteps(t);++i){now+=16;rs.frame(s1,now,pivot,away.inverse,away.projection,true,cand);}
        convertNow(rs,s1[0],now);std::vector<Observation> none;now+=rs.tuning().rigidUnseenMs-1;rs.frame(none,now,pivot,away.inverse,away.projection,true,cand);assert(rs.count(Registry::State::Ready)==1);
        now+=2;rs.frame(none,now,pivot,away.inverse,away.projection,true,cand);assert(rs.count(Registry::State::Ready)==0&&rs.stats().removed[Unseen]==1);}
    // Static placement: origin and axes (row-major matrix columns) of a rigid doodad at rest; a Stormwind
    // M2 placement from the world cache (180 degrees about z, scale .569).
    {const float m[9]={-.569f,0,0, -0.f,-.569f,0, 0,0,.569f},tr[3]={-9134.8525f,420.7604f,94.3334f};float root[3]={tr[0],tr[1],tr[2]},axes[9];
        for(unsigned a=0;a<3;++a)for(unsigned w=0;w<3;++w)axes[a*3+w]=m[w*3+a];
        assert(staticPlacement(root,axes,tr,m));root[0]+=.03f;assert(staticPlacement(root,axes,tr,m));root[0]+=.1f;assert(!staticPlacement(root,axes,tr,m));root[0]=tr[0];
        axes[0]=-.569f*std::cos(.05f);axes[1]=-.569f*std::sin(.05f);assert(!staticPlacement(root,axes,tr,m)); /* turned 3 degrees: another object */
        axes[0]=-.569f;axes[1]=0;for(float& x:axes)x*=1.05f;assert(!staticPlacement(root,axes,tr,m)); /* scaled */
        Registry rc(rigidTuning());std::vector<Observation> c={actor(86,20,0,0,2,20,1)};for(unsigned i=0;i<stillSteps(t);++i){now+=16;rc.frame(c,now,pivot,away.inverse,away.projection,true,cand);}
        assert(cand.size()==1);rc.covered(c[0],now);for(unsigned i=0;i<600;++i){now+=16;rc.frame(c,now,pivot,away.inverse,away.projection,true,cand);assert(cand.empty());}assert(rc.stats().rigidCovered==1);}
    std::puts("PASS rigid budget: sub-cap (signs cannot fill the still class's room; a nearer sign evicts the farthest sign), safety timeout, static-cache placement match (origin 0.05 yd, axes 1%) stays on the replays");
}
// Stale-shadow bounds for rigid props: in-view absence by the box CENTRE (complete frames), a totem
// despawning beside the camera, a lift moving away while unseen, teleports, the 10 min timeout.
static Camera facing(float ex,float ey,float ez,float fx,float fy){Camera c;const float n=std::sqrt(fx*fx+fy*fy);fx/=n;fy/=n; /* right = forward x up, up = +z */
    c.inverse[0]=fy;c.inverse[1]=-fx;c.inverse[2]=0;c.inverse[4]=0;c.inverse[5]=0;c.inverse[6]=1;c.inverse[8]=-fx;c.inverse[9]=-fy;c.inverse[10]=0;c.inverse[12]=ex;c.inverse[13]=ey;c.inverse[14]=ez;c.inverse[15]=1;return c;}
static void rigidDespawn(){
    static const float origin[3]={0,0,0};Camera away=camera(0,0,2,false),toward=camera(0,0,2,true);std::vector<size_t> cand;unsigned now=1000;
    {Camera f=facing(0,0,2,1,0);for(unsigned i=0;i<16;++i)assert(std::fabs(f.inverse[i]-toward.inverse[i])<1e-6f);}
    auto step=[&](Registry& g,std::vector<Observation>& o,const Camera& c,bool complete=true,const float* pivot=nullptr,unsigned dt=16){now+=dt;g.frame(o,now,pivot?pivot:origin,c.inverse,c.projection,complete,cand);};
    auto place=[&](Registry& g,std::vector<Observation>& o,Vec3 lo,Vec3 hi){for(unsigned i=0;i<stillSteps(g.tuning());++i)step(g,o,away);assert(cand.size()==o.size());
        for(auto& x:o){const auto id=g.begin(x,1000,now,nullptr);auto m=std::make_shared<Mesh>(*box(0,0,0));m->low=lo;m->high=hi;Result r;r.id=id;r.mesh=m;assert(g.accept(std::move(r),now));g.ready(id,1000);}};
    // Centre test: in front and inside the screen (not the 90% frame of the still class), near.
    assert(centreInView(toward.inverse,toward.projection,Vec3(14,-10.6f,0),Vec3(16,-8.6f,2),60)&&!fullyOnScreen(toward.inverse,toward.projection,Vec3(14,-10.6f,0),Vec3(16,-8.6f,2),60));
    assert(!centreInView(toward.inverse,toward.projection,Vec3(14,-13,0),Vec3(16,-11,2),60)); /* centre past the screen edge */
    assert(!centreInView(away.inverse,away.projection,Vec3(14,-1,0),Vec3(16,1,2),60)&&!centreInView(toward.inverse,toward.projection,Vec3(90,-1,0),Vec3(92,1,2),60));
    std::vector<Observation> none;
    // Totem 2 yd beside the camera despawns: kept while beside (its centre is not in view), removed once the camera turns to it.
    for(bool rigid:{true,false}){Registry g(rigid?rigidTuning():Tuning{});std::vector<Observation> totem={actor(90,0,-2,0,1,5,rigid?1:3)};
        place(g,totem,Vec3(-.4f,-2.4f,0),Vec3(.4f,-1.6f,1.2f));assert(totem[0].rigid==rigid);
        for(unsigned i=0;i<600;++i)step(g,none,toward);assert(g.count(Registry::State::Ready)==1); /* beside the camera: its shadow stays */
        const Camera look=facing(0,2,2,0,-1); /* standing 4 yd from it, looking at it */unsigned k=0;
        for(;k<200&&g.count(Registry::State::Ready)==1;++k)step(g,none,look,k%2==0); /* half the frames with a shortfall: they hold */
        if(rigid)assert(g.stats().removed[InView]==1&&k<=40);else assert(g.count(Registry::State::Ready)==1); /* still class: whole box on screen within the 90% frame */}
    // Trap at the screen edge (box half outside): the centre is in view -> removed.
    {Registry g(rigidTuning());std::vector<Observation> trap={actor(91,15,-9.6f,0,1,15,1)};place(g,trap,Vec3(14,-10.6f,0),Vec3(16,-8.6f,0.5f));
        for(unsigned i=0;i<200;++i)step(g,none,toward,false);assert(g.count(Registry::State::Ready)==1); /* only shortfall frames: never judged absent */
        for(unsigned k=0;k<40&&g.count(Registry::State::Ready);++k)step(g,none,toward);assert(g.stats().removed[InView]==1);}
    // A lift converted at its stop, then seen moving elsewhere while its stop is out of view: the stale caster
    // stays (no rekey: another place) until the stop is looked at, then goes; the lift itself never converts while it moves.
    {Registry g(rigidTuning());std::vector<Observation> lift={actor(92,30,0,-3,1,30,1)};place(g,lift,Vec3(22,-6,-4),Vec3(38,6,-2));
        std::vector<Observation> moving={actor(92,-40,0,-3,1,40,1)};
        for(unsigned i=0;i<300;++i){moving[0]=actor(92,-40,0,-3.f+i*.05f,1,40,1);step(g,moving,away);assert(cand.empty());}
        assert(g.count(Registry::State::Ready)==1&&g.stats().removed[Rekey]==0);
        for(unsigned k=0;k<40&&g.count(Registry::State::Ready);++k){moving[0]=actor(92,-40,0,12,1,40,1);step(g,moving,toward);}assert(g.stats().removed[InView]==1);}
    // Teleport on the same map: the pivot jumps 150 yd in one frame -> rigid props are dropped (re-confirmed later), still-class casters stay.
    {Registry g(rigidTuning(true));std::vector<Observation> both={actor(93,20,0,0,2,20,1),actor(94,25,0,0,2,25,3)};place(g,both,Vec3(19,-1,0),Vec3(21,1,2));
        const float walked[3]={60,0,0},jumped[3]={210,0,0};
        for(unsigned i=0;i<60;++i){const float at[3]={float(i),0,0};step(g,none,away,true,at);} /* walking 1 yd per frame (faster than any mount): kept */
        step(g,none,away,true,walked);assert(g.count(Registry::State::Ready)==2);step(g,none,away,true,jumped);
        assert(g.stats().removed[Teleport]==1&&g.count(Registry::State::Ready)==1&&g.countRigid(Registry::State::Ready)==0);}
    // Safety timeout: 10 min unseen.
    {Registry g(rigidTuning());std::vector<Observation> s1={actor(95,20,0,0,2,20,1)};place(g,s1,Vec3(19,-1,0),Vec3(21,1,2));
        step(g,none,away,true,nullptr,g.tuning().rigidUnseenMs-1);assert(g.count(Registry::State::Ready)==1);step(g,none,away,true,nullptr,2);
        assert(g.stats().removed[Unseen]==1&&g.tuning().rigidUnseenMs==10u*60u*1000u);}
    std::puts("PASS rigid stale shadows: centre-in-view absence on complete frames (a totem despawned beside the camera goes when looked at, a trap at the screen edge, a lift that moved away while unseen), same-map teleport re-confirms rigid props only, 10 min safety timeout; still class unchanged");
}
// The static-cache placement index: cells, neighbours across a cell border, negative coordinates.
static void placementIndex(){
    PlacementIndex x;int token=0;x.reset(&token,7);assert(x.scene==&token&&x.revision==7&&!x.complete);
    const float at[][3]={{-9134.85f,420.76f,94.33f},{-9135.9f,420.76f,94.33f},{15.99f,-0.01f,3},{500,500,0}};
    for(unsigned i=0;i<4;++i)x.add(at[i][0],at[i][1],at[i][2],i);x.add(NAN,0,0,9);assert(x.size()==4);
    auto hits=[&](const float* r){std::vector<std::uint32_t> h;x.find(r,.05f,[&](std::uint32_t i){h.push_back(i);return false;});return h;};
    {const float r[3]={-9134.83f,420.78f,94.31f};auto h=hits(r);assert(h.size()==1&&h[0]==0);}
    {const float r[3]={16.02f,0.02f,3};auto h=hits(r);assert(h.size()==1&&h[0]==2);} /* across the 16 yd cell border */
    {const float r[3]={500,500,.2f};assert(hits(r).empty());const float n[3]={NAN,0,0};assert(hits(n).empty());}
    {const float r[3]={-9135.9f,420.76f,94.33f};assert(x.find(r,.05f,[](std::uint32_t i){return i==1;})&&!x.find(r,.05f,[](std::uint32_t){return false;}));}
    x.reset(nullptr,0);assert(x.size()==0);
    std::puts("PASS placement index: 16 yd cells, 3x3 neighbourhood across borders, negative world coordinates, non-finite ignored, match predicate decides");
}
// ---- real client one-influence program (signs) --------------------------
// The active 3.3.5a one-influence variants (actor_client_programs.h, client_fixtures.py) are the
// exact template; the four-weight and zero-influence programs and any altered one are not.
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
    // Referenced slots: the one-influence program reads lane x whatever the weights; the blend, weighted lanes.
    auto slots=[](NorthlightActorDeformation::Four i,NorthlightActorDeformation::Four w,unsigned lanes){std::vector<unsigned> s;const bool ok=referencedSlots(i,w,lanes,[&](unsigned x){s.push_back(x);});return ok?s:std::vector<unsigned>{99};};
    assert((slots({0,7,3,9},{1,0,0,0},1)==std::vector<unsigned>{0})&&(slots({0,7,3,9},{.5f,.5f,0,0},1)==std::vector<unsigned>{0})&&(slots({2,7,3,9},{1,1,1,1},1)==std::vector<unsigned>{2}));
    assert((slots({0,7,3,9},{.5f,.5f,0,0},4)==std::vector<unsigned>{0,7})&&(slots({0,75,3,9},{1,0,0,0},1)==std::vector<unsigned>{0})&&(slots({75,0,3,9},{1,0,0,0},1)==std::vector<unsigned>{99})&&(slots({1.5f,0,0,0},{1,0,0,0},4)==std::vector<unsigned>{99}));
    std::printf("PASS one-influence program: 4 real client variants (w move first/last/middle, t=r0/r1, vs_2_0 swapped mul) are the exact template, four-weight/zero-influence are not, %u altered programs refused; slots: lane x only\n",refused);
}
// World camera: rows right, up, back (view z = -forward) and the eye (the renderer's inverse view).
static Camera looking(const double* eye,const double* at){double f[3]={at[0]-eye[0],at[1]-eye[1],at[2]-eye[2]};double n=std::sqrt(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);for(double& x:f)x/=n;
    double r[3]={f[1],-f[0],0};n=std::sqrt(r[0]*r[0]+r[1]*r[1]);for(double& x:r)x/=n;const double u[3]={r[1]*f[2]-r[2]*f[1],r[2]*f[0]-r[0]*f[2],r[0]*f[1]-r[1]*f[0]};
    Camera c;for(unsigned k=0;k<3;++k){c.inverse[k]=float(r[k]);c.inverse[4+k]=float(u[k]);c.inverse[8+k]=float(-f[k]);c.inverse[12+k]=float(eye[k]);}c.inverse[15]=1;return c;}
// The game's palette slot: view * world (model->world M row-major column vector, translation t), as float rows.
static void slot(const Camera& c,const double* eye,const float* M,const float* t,float* rows){
    for(unsigned row=0;row<3;++row){const float* v=c.inverse+4*row;double w=0;
        for(unsigned j=0;j<3;++j){double s=0;for(unsigned k=0;k<3;++k)s+=double(v[k])*M[k*3+j];rows[4*row+j]=float(s);}
        for(unsigned k=0;k<3;++k)w+=double(v[k])*(double(t[k])-eye[k]);rows[4*row+3]=float(w);}}
// A board: one row of `quads` quads (2 triangles each), game layout: float3 POSITION, UBYTE4N
// BLENDWEIGHT (255,0,0,0), UBYTE4 BLENDINDICES (0 and junk in the unread lanes).
static std::shared_ptr<NorthlightDrawSnapshot::Mesh> board(unsigned quads,float width,float height){auto m=std::make_shared<NorthlightDrawSnapshot::Mesh>();
    const unsigned nx=quads+1,ny=2;m->vertexCount=nx*ny;m->streams[0].stride=20;m->streams[0].bytes.resize(size_t(m->vertexCount)*20);
    for(unsigned y=0;y<ny;++y)for(unsigned x=0;x<nx;++x){auto* out=m->streams[0].bytes.data()+size_t(y*nx+x)*20;const float p[3]={width*(float(x)/quads-.5f),.05f*(y&1),-height*float(y)/(ny-1)};
        std::memcpy(out,p,12);out[12]=255;out[13]=out[14]=out[15]=0;out[16]=0;out[17]=7;out[18]=3;out[19]=9;}
    m->indexed=true;m->topology=D3DPT_TRIANGLELIST;
    for(unsigned y=0;y+1<ny;++y)for(unsigned x=0;x<quads;++x){const std::uint32_t a=y*nx+x,b=a+1,c=a+nx,d=c+1;for(auto i:{a,b,c,b,d,c})m->indices.push_back(i);}
    m->primitiveCount=unsigned(m->indices.size()/3);return m;}
static const std::vector<D3DVERTEXELEMENT9> gameLayout={{0,0,2,0,0,0},{0,12,8,0,1,0},{0,16,5,0,2,0},{0xff,0,17,0,0,0}};
// persistentDrawBones: referenced slots over the snapshot with the program's lanes.
static std::vector<unsigned> meshSlots(const NorthlightDrawSnapshot::Mesh& m,unsigned lanes){unsigned char used[75]={};
    for(auto v:m.indices){NorthlightActorDeformation::Four i,w;const auto* b=m.streams[0].bytes.data()+size_t(v)*20;assert(NorthlightActorDeformation::decodeElement(b+16,5,i)&&NorthlightActorDeformation::decodeElement(b+12,8,w));
        assert(referencedSlots(i,w,lanes,[&](unsigned s){used[s]=1;}));}
    std::vector<unsigned> s;for(unsigned b=0;b<75;++b)if(used[b])s.push_back(b);return s;}
// One captured prop drawn with the real one-influence program: palette slot 0 = view * world,
// every other slot stale data (another model's bones); `world` its model->world placement.
struct Prop {std::uint64_t key=0;float M[9]={},t[3]={};std::shared_ptr<NorthlightDrawSnapshot::Mesh> mesh;bool placed=false;unsigned appear=0,poses=0,candidates=0;std::uint32_t id=0;};
static void propConstants(const Prop& p,const Camera& c,const double* eye,std::mt19937& rng,std::array<float,1024>& constants){
    std::uniform_real_distribution<float> junk(-50,50);for(unsigned r=34;r<256;++r)for(unsigned k=0;k<4;++k)constants[4*r+k]=junk(rng);
    constants[0]=3;constants[1]=1;slot(c,eye,p.M,p.t,constants.data()+4*31);}
// persistentFrame for props: observation (rootWorld, bone 0 axes), the static screen, the pose of the
// referenced slots (only when needsPose), then the registry frame.
struct PropScene {
    const Program& program;Registry& reg;const PlacementIndex& index;const std::vector<std::array<float,12>>& placements;
    std::vector<Prop>& props;std::vector<Observation> obs;std::deque<std::vector<float>> poolPoses;std::vector<size_t> cand;std::mt19937 rng{146};unsigned screens=0;
    PropScene(const Program& p,Registry& r,const PlacementIndex& x,const std::vector<std::array<float,12>>& pl,std::vector<Prop>& v):program(p),reg(r),index(x),placements(pl),props(v){}
    /* persistentStaticCovered: -1 while the scene is being indexed */
    int covered(const Observation& o){if(!index.complete)return -1;return index.find(o.root,.05f,[&](std::uint32_t i){return staticPlacement(o.root,o.axes,placements[i].data()+9,placements[i].data());})?1:0;}
    void frame(unsigned now,const double* eye,const Camera& c,const float* pivot,bool visible=true,unsigned serial=0){
        obs.clear();poolPoses.clear();std::vector<size_t> who;
        if(visible)for(size_t n=0;n<props.size();++n){auto& p=props[n];if(p.appear>serial)continue;std::array<float,1024> k{};propConstants(p,c,eye,rng,k);Observation o;o.key=p.key;o.count=1;o.first=n;o.end=n+1;o.vertices=p.mesh->vertexCount;o.triangles=p.mesh->primitiveCount;
            assert(NorthlightActorDeformation::rootWorld(program,k.data(),c.inverse,o.root));float w[12];assert(worldBone(k.data()+4*31,c.inverse,w));std::memcpy(o.axes,w,36);
            float q=0;for(unsigned a=0;a<3;++a)q+=(o.root[a]-pivot[a])*(o.root[a]-pivot[a]);o.distanceSquared=q;
            reg.screen(o.key,o.root,[&]{++screens;return covered(o);});
            if(reg.needsPose(o.key,o.root)){++p.poses;poolPoses.emplace_back();auto& pose=poolPoses.back();
                for(unsigned s:meshSlots(*p.mesh,1)){float b[12];assert(worldBone(k.data()+4*(31+3*s),c.inverse,b));pose.insert(pose.end(),b,b+12);}
                o.pose=pose.data();o.poseBones=unsigned(pose.size()/12);}
            obs.push_back(o);who.push_back(n);}
        reg.frame(obs,now,pivot,c.inverse,c.projection,true,cand);
        // The renderer's candidate loop: static-covered (fallback), held, room, at most one job per frame.
        for(size_t ci:cand){auto& o=obs[ci];auto& p=props[who[ci]];++p.candidates;assert(o.rigid);
            const int placed=covered(o);if(placed<0)continue;if(placed){reg.covered(o,now);continue;}
            if(reg.heldByBody(obs,ci,[](size_t){return true;},[](size_t,float*,float*){return int(Registry::BodyNever);})!=Registry::Free){reg.deferRigid(o,now,true);continue;}
            if(!reg.room(o,70000,now)){reg.defer(o,now,true);break;}
            Job job;job.bones=1;NorthlightActorGeometry::Packet k;k.sharedMesh=p.mesh;k.position=program;k.elements=gameLayout;propConstants(p,c,eye,rng,k.constants);std::memcpy(k.inverseView.data(),c.inverse,64);job.packets.push_back(k);
            const auto id=reg.begin(o,70000,now,nullptr);assert(id);job.id=id;auto r=convert(job);assert(r.failure==Failure::None&&r.mesh);
            // The converted world mesh is the placement of the model (independent double evaluation).
            double worst=0;const auto& m=*p.mesh;std::vector<std::uint32_t> order;{std::vector<char> seen(m.vertexCount,0);for(auto i:m.indices)if(!seen[i]){seen[i]=1;order.push_back(i);}} /* first-reference order */
            assert(order.size()==r.mesh->vertices.size());for(size_t n=0;n<order.size();++n){const auto v=order[n];float x[3];std::memcpy(x,m.streams[0].bytes.data()+size_t(v)*20,12);
                const auto& w=r.mesh->vertices[n].position;const double e[3]={p.M[0]*double(x[0])+p.M[1]*double(x[1])+p.M[2]*double(x[2])+p.t[0],p.M[3]*double(x[0])+p.M[4]*double(x[1])+p.M[5]*double(x[2])+p.t[1],p.M[6]*double(x[0])+p.M[7]*double(x[1])+p.M[8]*double(x[2])+p.t[2]};
                worst=std::max({worst,std::fabs(w.x-e[0]),std::fabs(w.y-e[1]),std::fabs(w.z-e[2])});}
            assert(worst<5e-3);assert(reg.accept(std::move(r),now));reg.ready(id,70000);p.id=id;break;}
    }
};
static void rotZ(float angle,float scale,float* M){const float c=std::cos(angle)*scale,s=std::sin(angle)*scale;const float m[9]={c,-s,0,s,c,0,0,0,scale};std::memcpy(M,m,36);}
// (e) A Stormwind shop sign drawn with the real one-influence program, the camera orbiting the player
// the whole time (every palette row changes each frame): observed, still, rigid candidate, converted to
// its exact world placement, kept while the camera looks away; junk in the unread index lanes and stale
// palette slots never make it multi-matrix.
static void realSign(){
    const Program program=CLIENT_PROGRAM(OneBoneVs3);Registry reg(rigidTuning());PlacementIndex index;std::vector<std::array<float,12>> placements;int token=0;index.reset(&token,1);index.complete=true;
    std::vector<Prop> props(1);auto& sign=props[0];sign.key=0x5167;rotZ(.7f,1,sign.M);sign.t[0]=-8850.31f;sign.t[1]=612.74f;sign.t[2]=104.2f;sign.mesh=board(75,1.6f,1.1f);
    assert(sign.mesh->primitiveCount==150&&(meshSlots(*sign.mesh,1)==std::vector<unsigned>{0})); /* the HD sign board: 150 triangles */
    {auto m=board(4,1,1);for(size_t v=0;v<m->vertexCount;++v)m->streams[0].bytes[v*20+13]=1; /* nonzero weight in lane y: the program still reads x only */
        assert((meshSlots(*m,1)==std::vector<unsigned>{0})&&(meshSlots(*m,4)==std::vector<unsigned>{0,7}));}
    PropScene scene{program,reg,index,placements,props};const float pivot[3]={-8856.f,610.f,98.5f};const double player[3]={-8856,610,99.5};unsigned now=1000,frames=0;
    auto orbit=[&](double yaw,double* eye){eye[0]=player[0]-12*std::cos(yaw);eye[1]=player[1]-12*std::sin(yaw);eye[2]=player[2]+5;return looking(eye,player);};
    for(double yaw=0;frames<400&&!sign.id;++frames,yaw+=.02){double eye[3];const Camera c=orbit(yaw,eye);now+=16;scene.frame(now,eye,c,pivot);}
    assert(sign.id&&sign.candidates==1&&frames==stillSteps(reg.tuning())+reg.tuning().poseAfterFrames&&scene.screens==1&&sign.poses==frames-reg.tuning().poseAfterFrames); /* posed from the 5th still frame, then 2 s */
    assert(reg.stats().rigidSubmitted==1&&reg.stats().rigidConverted==1&&reg.stats().rigidScreened==0&&reg.countRigid(Registry::State::Ready)==1);
    for(unsigned i=0;i<40;++i){double eye[3];const Camera c=orbit(.02*(frames+i),eye);now+=16;scene.frame(now,eye,c,pivot);}assert(scene.obs[0].caster==sign.id);
    float m[16];light(pivot[0],pivot[1],m);const auto signature=reg.signature(m);assert(signature);
    for(unsigned i=0;i<120;++i){double eye[3];orbit(0,eye);const double back[3]={eye[0]-10,eye[1],eye[2]};const Camera c=looking(eye,back);now+=1000;scene.frame(now,eye,c,pivot,false);} /* 120 s turned away: the game draws nothing */
    assert(reg.count(Registry::State::Ready)==1&&reg.signature(m)==signature&&reg.stats().removed[Unseen]==0);
    // The same sign through the four-lane rule (the pre-fix reading of junk lanes) would be multi-matrix.
    {std::array<float,1024> k{};double eye[3];const Camera c=orbit(0,eye);propConstants(sign,c,eye,scene.rng,k);std::vector<float> pose;
        for(unsigned s:{0u,3u,7u,9u}){float b[12];assert(worldBone(k.data()+4*(31+3*s),c.inverse,b));pose.insert(pose.end(),b,b+12);}assert(!Registry::singleMatrix(pose.data(),4));}
    std::printf("PASS real sign: one-influence program, camera orbiting, rigid candidate after %u frames (%u ms), converted to its placement, kept 120 s turned away\n",frames,frames*16);
}
// (f) Static-doodad flood: real Stormwind trade-district / Goldshire placements from the world cache
// (WMO doodads scaled .4-2.2 and ADT doodads) plus a synthetic flood of rotated/scaled doodads, all
// drawn by the game with the one-influence program at rest (palette = view * placement): each is
// screened ONCE when its root becomes still, before any pose work, and never becomes a candidate; the
// server-spawned signs among them all convert (the rigid sub-cap is not starved).
struct RealPlacement {unsigned category;const char* model;float m[9],t[3];};
static const RealPlacement tradeDistrict[]={
#include "stormwind_rigid_placements.inc" /* written by test_persistent_casters.py from the tester's world cache */
};
static void staticFlood(){
    const Program program=CLIENT_PROGRAM(OneBoneVs3);Registry reg(rigidTuning());PlacementIndex index;std::vector<std::array<float,12>> placements;int token=0;
    std::vector<Prop> props;std::mt19937 rng(1461);std::uniform_real_distribution<float> spread(-90,90),angle(-3.1f,3.1f),scale(.4f,2.2f),height(90,110);
    const float centre[2]={-8850,620};auto base=board(20,1,1);
    auto add=[&](const float* M,const float* t,bool placed){Prop p;p.key=0x1000+props.size()%37;std::memcpy(p.M,M,36);std::memcpy(p.t,t,12);p.mesh=base;p.placed=placed;props.push_back(p);
        if(placed){std::array<float,12> x;std::memcpy(x.data(),M,36);std::memcpy(x.data()+9,t,12);placements.push_back(x);}};
    // Wave 1 (from the start, the static scene still being indexed for the first second): the real
    // placements, 180 synthetic doodads, 6 signs. Wave 2 (walking on, scene complete): 200 doodads, 6 signs.
    for(const auto& r:tradeDistrict){const bool goldshire=r.t[0]<-9000;const float t[3]={goldshire?r.t[0]+610.f:r.t[0],goldshire?r.t[1]+560.f:r.t[1],r.t[2]};add(r.m,t,true);}
    auto flood=[&](unsigned doodads,unsigned signs,unsigned appear){
        for(unsigned i=0;i<doodads;++i){float M[9];rotZ(angle(rng),scale(rng),M);const float t[3]={centre[0]+spread(rng),centre[1]+spread(rng),height(rng)};add(M,t,true);props.back().appear=appear;}
        for(unsigned i=0;i<signs;++i){float M[9];rotZ(angle(rng),1,M);const float t[3]={centre[0]+spread(rng)*.3f,centre[1]+spread(rng)*.3f,104};add(M,t,false);props.back().key=0x9000+props.size();props.back().appear=appear;}};
    flood(180,6,0);const size_t first=props.size();flood(200,5,300);
    // Identical crates side by side, 1 yd apart (the same snapshot key): the static one is screened,
    // the server-spawned one next to it converts (each observation screens its nearest track).
    {float M[9];rotZ(.3f,1.1f,M);const float a[3]={centre[0]+4,centre[1]+3,97},b[3]={a[0]+.8f,a[1]+.6f,97};add(M,a,true);props.back().key=0x7777;props.back().appear=300;
        add(M,b,false);props.back().key=0x7777;props.back().appear=300;}
    index.reset(&token,1);for(size_t i=0;i<placements.size();++i)index.add(placements[i][9],placements[i][10],placements[i][11],std::uint32_t(i));
    PropScene scene{program,reg,index,placements,props};const float pivot[3]={centre[0],centre[1],98};const double player[3]={centre[0],centre[1],99};unsigned now=1000;
    auto orbit=[&](double yaw,double* eye){eye[0]=player[0]-12*std::cos(yaw);eye[1]=player[1]-12*std::sin(yaw);eye[2]=player[2]+5;return looking(eye,player);};
    unsigned frame=0;for(;frame<1500;++frame){index.complete=frame>=60;double eye[3];const Camera c=orbit(.01*frame,eye);now+=16;scene.frame(now,eye,c,pivot,true,frame);
        bool all=frame>=300;for(const auto& p:props)all=all&&(p.placed||p.id);if(all)break;}
    size_t statics=0,signs=0,posed=0;
    for(size_t n=0;n<props.size();++n){const auto& p=props[n];if(p.placed){++statics;assert(!p.candidates&&!p.id);posed+=p.poses>0;if(n>=first)assert(p.poses==0);}else{assert(p.id&&p.candidates==1);++signs;}}
    const auto& s=reg.stats();assert(signs==12&&s.rigidScreened==statics&&s.rigidCovered==statics&&s.rigidSubmitted==12&&reg.countRigid(Registry::State::Ready)==12);
    assert(posed<=first&&scene.screens>statics+12); /* wave 1 posed until the scene was indexed (asked again each frame), wave 2 never */
    for(unsigned i=0;i<600;++i){double eye[3];const Camera c=orbit(.01*(frame+i),eye);now+=16;scene.frame(now,eye,c,pivot,true,frame+i);assert(scene.cand.empty());}
    assert(s.rigidScreened==statics&&reg.countRigid(Registry::State::Ready)==12);
    std::printf("PASS static-doodad flood: %zu doodads (%zu real world-cache placements, scale .4-2.2) screened once still and never candidates (none posed once the scene was indexed), 11 signs and a spawned crate beside its static twin converted (%u frames), %u screen calls\n",statics,sizeof tradeDistrict/sizeof*tradeDistrict,frame,scene.screens);
}
// ---- diagnostics (PERSISTENT near lines: explain(), removal events) -----------
static bool has(const Registry::Explained& e,const char* s){if(std::strstr(e.text,s))return true;std::printf("missing '%s' in: %s\n",s,e.text);return false;}
static void diagnosticPath(){
    const float pivot[3]={0,0,0};Camera away=camera(0,0,2,false);std::vector<size_t> cand;unsigned now=1000;Registry::Explained e;using P=Registry::Phase;
    auto step=[&](Registry& g,std::vector<Observation>& o,bool complete=true,unsigned dt=16){now+=dt;g.frame(o,now,pivot,away.inverse,away.projection,complete,cand);};
    auto until=[&](Registry& g,std::vector<Observation>& o,size_t n){unsigned i=0;do step(g,o);while(++i<400&&cand.size()<n);assert(cand.size()==n);};
    // Settling, candidate, then a shortfall streak defers it (waitingCompleteFrame with the streak), a complete frame
    // without the static scene defers it again (waitingStaticScene); both cool down deferMs.
    Registry reg(rigidTuning());reg.diagnostics(24,pivot);const auto& t=reg.tuning();
    std::vector<Observation> sign={actor(90,5,0,3,1,5,1)};sign[0].triangles=150;
    step(reg,sign);step(reg,sign);assert(reg.explain(90,sign[0].root,now,1,e)&&e.phase==P::Moving&&e.seen&&e.serial&&has(e,"still=1/30"));
    until(reg,sign,1);assert(reg.explain(90,sign[0].root,now,1,e)&&e.phase==P::Candidate&&e.single&&!e.multi&&e.bones==1&&has(e,"candidate"));
    for(unsigned i=0;i<3;++i)step(reg,sign,false);assert(reg.shortfallFrames()==3&&cand.size()==1);reg.deferRigid(sign[0],now,false);
    assert(reg.explain(90,sign[0].root,now,1,e)&&e.phase==P::Cooldown&&has(e,"cooldown until=+2000ms")&&has(e,"gate=waitingCompleteFrame ago=0ms streak=3"));
    step(reg,sign,true,t.deferMs);assert(cand.size()==1&&reg.shortfallFrames()==0);reg.deferRigid(sign[0],now,false);
    assert(reg.explain(90,sign[0].root,now,1,e)&&has(e,"gate=waitingStaticScene"));
    // Parts: a frame missing a part seen while still is not a candidate.
    step(reg,sign,true,t.deferMs);assert(cand.size()==1);{auto two=sign;two[0].count=2;step(reg,two);std::vector<Observation> one=sign;step(reg,one);assert(cand.empty()&&reg.explain(90,sign[0].root,now,1,e)&&e.phase==P::Parts&&has(e,"waitingAllParts(draws=1<2)"));}
    // Ready, published: the caster id; unseen (camera turned away) keeps it with the time unseen.
    {Registry rr(rigidTuning());rr.diagnostics(24,pivot);std::vector<Observation> s={actor(89,5,1,3,1,5,1)};until(rr,s,1);const auto id=convertNow(rr,s[0],now);
        assert(rr.explain(89,s[0].root,now,1,e)&&e.phase==P::Ready&&e.caster==id&&e.rigid&&has(e,"ready id=")&&has(e,"published=0"));
        step(rr,s,true,600);step(rr,s);assert(rr.explain(89,s[0].root,now,1,e)&&e.phase==P::Published&&has(e,"published=1"));
        std::vector<Observation> none;for(unsigned i=0;i<60;++i)step(rr,none);assert(rr.explain(89,s[0].root,now,0,e)&&e.phase==P::Published&&!e.seen&&has(e,"unseen=960ms"));}
    // Worn: a body box holds it (holder key, position, distance, box); an unaudited group holds another (its sampled vertex).
    {std::vector<Observation> pair={actor(91,20,0,0,2,20,30),actor(92,20.4f,.3f,1,1,20,1)};until(reg,pair,1);assert(cand[0]==1);
        const float bl[3]={19.6f,-.45f,0},bh[3]={20.6f,.45f,2.1f};
        assert(reg.heldByBody(pair,1,[](size_t n){return n==1;},[&](size_t,float* l,float* h){std::memcpy(l,bl,12);std::memcpy(h,bh,12);return int(Registry::BodyKnown);})==Registry::Held);
        reg.deferRigid(pair[1],now,true,true);
        assert(reg.explain(92,pair[1].root,now,1,e)&&e.phase==P::Worn&&has(e,"worn(sticky")&&has(e,"gate=worn")&&has(e,"heldBy=box holderKey=0000005b holderAt=(20.0 0.0 0.0) holderDistance=1.1 holderBox=(19.6"));
        assert(reg.explain(91,pair[0].root,now,2,e)&&e.phase==P::Multi&&e.distinct==30&&has(e,"multi(distinct=30"));
        std::vector<Observation> lamp={actor(93,-10,0,3,1,10,1)};until(reg,lamp,1);assert(reg.heldByBody(lamp,0,[](size_t){return true;},[](size_t,float*,float*){return int(Registry::BodyKnown);})==Registry::Free);
        const float vertex[3]={-12,0,1};reg.noteHold(lamp[0],HoldKind::Other,vertex);reg.deferRigid(lamp[0],now,true,true);
        assert(reg.explain(93,lamp[0].root,now,1,e)&&e.phase==P::Worn&&has(e,"heldBy=unaudited holderAt=(-12.0 0.0 1.0) holderDistance=2.8"));
        std::vector<Observation> post={actor(88,-10,4,3,1,10,1)};until(reg,post,1);reg.heldByBody(post,0,[](size_t){return true;},[](size_t,float*,float*){return int(Registry::BodyKnown);});
        reg.noteHold(post[0],HoldKind::Unlocated,nullptr);reg.deferRigid(post[0],now,true,false);assert(reg.explain(88,post[0].root,now,1,e)&&e.phase==P::Cooldown&&has(e,"gate=undecided")&&has(e,"heldBy=unaudited(unlocated)"));}
    // Decisions are recorded at any distance (a sign judged from 60 yd, explained when the player stands under it);
    // a permanent reject names its cause (oversize, alpha, bones...); cover says what was matched, nothing more.
    {Registry fr(rigidTuning());fr.diagnostics(24,pivot);std::vector<Observation> far={actor(110,60,0,4,1,60,1),actor(111,-60,0,4,1,60,1),actor(112,0,60,4,1,60,1)};until(fr,far,3);
        const float vertex[3]={61,0,1};fr.noteHold(far[0],HoldKind::Other,vertex);fr.deferRigid(far[0],now,true,true);fr.reject(far[1],now,true,Failure::Oversize);fr.covered(far[2],now);
        assert(fr.explain(110,far[0].root,now,1,e)&&e.phase==P::Worn&&has(e,"heldBy=unaudited holderAt=(61.0 0.0 1.0)"));
        assert(fr.explain(111,far[1].root,now,1,e)&&e.phase==P::Rejected&&has(e,"rejected(permanent) by=oversize"));
        assert(fr.explain(112,far[2].root,now,1,e)&&e.phase==P::Covered&&has(e,"covered(static placement at the same root/axes)"));
        Registry pr(rigidTuning());pr.diagnostics(24,pivot);std::vector<Observation> pp={actor(113,50,0,4,1,50,1),actor(114,-50,0,4,1,50,1)};until(pr,pp,2);const auto a=convertNow(pr,pp[0],now);(void)a;
        const auto b=pr.begin(pp[1],1000,now,nullptr);Result rb;rb.id=b;rb.mesh=box(-50,0,4);pr.accept(std::move(rb),now);pr.uploadFailed(b,now);
        std::vector<Observation> moved={actor(115,50,0,4,1,50,1)};std::vector<Observation> none;for(unsigned i=0;i<10;++i)step(pr,none);step(pr,moved);
        assert(pr.explain(113,pp[0].root,now,0,e)&&has(e,"removed=rekey")&&pr.explain(114,pp[1].root,now,0,e)&&has(e,"by=upload"));}
    // Stillness resets: root, axes, pose (by how much); two matrices: multi.
    {Registry mv(rigidTuning());mv.diagnostics(24,pivot);std::vector<Observation> m={actor(99,6,0,3,1,6,1)};for(unsigned i=0;i<10;++i)step(mv,m);
        m[0].root[0]+=.3f;step(mv,m);assert(mv.explain(99,m[0].root,now,1,e)&&has(e,"lastReset=root by=0.300 ago=0ms")&&has(e,"still=0/30"));
        for(unsigned i=0;i<10;++i)step(mv,m);m[0].axes[1]=.03f;step(mv,m);assert(mv.explain(99,m[0].root,now,1,e)&&has(e,"lastReset=axes by=0.030"));
        for(unsigned i=0;i<10;++i)step(mv,m);poses.emplace_back(12,0.f);auto& q=poses.back();std::memcpy(q.data(),m[0].pose,48);q[11]+=.2f;m[0].pose=q.data();step(mv,m);
        assert(mv.explain(99,m[0].root,now,1,e)&&has(e,"lastReset=pose by=0.200")&&has(e,"poseBasisBy=0.000(tolerance 0.010) poseOriginBy=0.200(tolerance 0.020)")&&has(e,"pose=0/30"));
        std::vector<Observation> banner={actor(100,7,0,3,1,7,2)};step(mv,banner);step(mv,banner);assert(mv.explain(100,banner[0].root,now,1,e)&&e.phase==P::Multi&&has(e,"multi(distinct=2"));
        const auto* d=mv.diagnosis(e.serial);assert(d&&d->distinct==2);
        unsigned near=0;mv.tracksNear(pivot,7.2f,[&](std::uint64_t k,const float*,std::uint32_t){near+=k==99;});assert(near==1);}
    // Rekey: a sign's board and chains (two constant groups at one root) converted, the camera turned away,
    // back in view with a new key for the chains (same part: tri/draws equal) -> the chains caster is removed
    // with both keys; a group holding board+chains under a new key removes the board (other part); a draw
    // repeated in one frame (its key already known) rekeys the unmatched caster beside it (newKeyKnown).
    {Registry rk(rigidTuning());rk.diagnostics(24,pivot);std::vector<Observation> sg={actor(94,8,0,4,1,8,1),actor(95,8,0,4,1,8,1)};sg[0].triangles=150;sg[1].triangles=32;
        until(rk,sg,2);const auto board=convertNow(rk,sg[0],now),chains=convertNow(rk,sg[1],now);(void)chains;step(rk,sg,true,600);step(rk,sg);assert(sg[0].caster==board);
        std::vector<Observation> none;for(unsigned i=0;i<300;++i)step(rk,none);assert(rk.countRigid(Registry::State::Ready)==2);
        std::vector<Registry::Event> ev;rk.takeEvents(ev);
        std::vector<Observation> back={sg[0],actor(96,8,0,4,1,8,1)};back[1].triangles=32;step(rk,back);rk.takeEvents(ev);
        assert(ev.size()==1&&ev[0].reason==Rekey&&ev[0].rigid&&ev[0].key==95&&ev[0].other==96&&ev[0].triangles==32&&ev[0].otherTriangles==32&&ev[0].draws==1&&ev[0].otherDraws==1&&!ev[0].otherKnown);
        assert(rk.explain(95,sg[1].root,now,0,e)&&e.phase==P::Cooldown&&has(e,"removed=rekey")&&has(e,"oldKey=0000005f(tri=32 draws=1) newKey=00000060(tri=32 draws=1) part=same dupDraw=0"));
        assert(rk.explain(96,back[1].root,now,1,e)&&has(e,"replacedCasterKey=0000005f ago=0ms"));
        for(unsigned i=0;i<60;++i)step(rk,none);std::vector<Observation> merged={actor(97,8,0,4,2,8,1)};merged[0].triangles=182;step(rk,merged);rk.takeEvents(ev);
        assert(ev.size()==1&&ev[0].key==94&&ev[0].other==97&&ev[0].otherDraws==2);assert(rk.explain(94,sg[0].root,now,0,e)&&has(e,"oldKey=0000005e(tri=150 draws=1) newKey=00000061(tri=182 draws=2) part=other"));
        Registry dup(rigidTuning());dup.diagnostics(24,pivot);std::vector<Observation> ab={actor(0x70,9,0,4,1,9,1),actor(0x71,9,0,4,1,9,1)};until(dup,ab,2);convertNow(dup,ab[1],now);
        std::vector<Observation> twice={ab[0],ab[0]};step(dup,twice);dup.takeEvents(ev);assert(ev.size()==1&&ev[0].key==0x71&&ev[0].other==0x70&&ev[0].otherKnown);
        assert(dup.explain(0x71,ab[1].root,now,0,e)&&has(e,"dupDraw=1"));}
    // Diagnostics off (the renderer without Diagnostics or PersistentRigidProps): nothing recorded, and the
    // registry's decisions are identical with it on (same candidates, casters, removals, frame by frame).
    {Registry on(rigidTuning()),off(rigidTuning());on.diagnostics(24,pivot);unsigned t0=now;std::vector<size_t> a,b;std::vector<Registry::Event> ea,eb;
        auto script=[&](Registry& g,std::vector<size_t>& trace,std::vector<Registry::Event>& events){now=t0;std::vector<Observation> o={actor(101,6,0,3,1,6,1),actor(102,6.5f,0,3,1,6,30),actor(103,7,0,3,1,7,1)};
            for(unsigned f=0;f<900;++f){if(f==300)o[2].root[2]+=.3f;if(f==500)o[1].key=104;
                const bool shortfall=f%7==3||(f>=120&&f<135);now+=16;g.frame(o,now,pivot,away.inverse,away.projection,!shortfall,cand);
                for(size_t c:cand){if(shortfall){g.deferRigid(o[c],now,false);continue;}
                    if(g.heldByBody(o,c,[&](size_t n){return n!=1;},[](size_t,float* l,float* h){l[0]=l[1]=l[2]=100;h[0]=h[1]=h[2]=101;return int(Registry::BodyKnown);})!=Registry::Free)continue;
                    if(g.room(o[c],1000,now))convertNow(g,o[c],now);break;}
                trace.push_back(cand.size()*1000+g.entries().size()*10+o[0].caster+o[2].caster);}
            g.takeEvents(events);};
        script(on,a,ea);script(off,b,eb);assert(a==b&&ea.size()==eb.size()&&on.stats().rigidSubmitted==off.stats().rigidSubmitted&&on.stats().rigidShortfall==off.stats().rigidShortfall&&on.stats().removed[Moved]==off.stats().removed[Moved]);
        assert(on.stats().rigidSubmitted>=2&&on.stats().rigidShortfall>0);
        unsigned seen=0;off.tracksNear(pivot,30,[&](std::uint64_t k,const float* r,std::uint32_t serial){assert(!off.diagnosis(serial));Registry::Explained x;assert(off.explain(k,r,now,0,x));++seen;
            assert(!std::strstr(x.text,"gate=")&&!std::strstr(x.text,"lastReset=")&&!std::strstr(x.text,"removed="));});assert(seen>=3);
        on.diagnostics(0,nullptr);assert(!on.diagnosis(1)&&!on.diagnosis(2));}
    // Record table past 4096 (a crowd): only forgotten tracks' records are evicted; a worn record of a live track survives.
    {Registry big(rigidTuning());big.diagnostics(1000,pivot);std::vector<Observation> crowd;for(unsigned i=0;i<4500;++i)crowd.push_back(actor(200+i,float(i%70)*2,float(i/70)*2,0,1,20,2));
        std::vector<Observation> w={actor(120,-5,-5,1,1,8,1)};until(big,w,1);big.noteHold(w[0],HoldKind::Never,nullptr);big.deferRigid(w[0],now,true,true);
        for(unsigned f=0;f<3;++f)step(big,crowd);assert(big.explain(120,w[0].root,now,0,e)&&e.phase==P::Worn&&has(e,"gate=worn"));
        std::vector<Observation> other;for(unsigned i=0;i<3000;++i)other.push_back(actor(9000+i,float(i%70)*2,float(i/70)*2+200,0,1,20,2));
        for(unsigned f=0;f<3;++f)step(big,other);assert(big.explain(120,w[0].root,now,0,e)&&has(e,"gate=worn")&&big.explain(200,crowd[0].root,now,0,e)&&big.diagnosis(e.serial)&&big.diagnosis(e.serial)->distinct==2);}
    {char small[8]="";NorthlightPersistentCasters::append(small,sizeof small,"%s","0123456789");assert(std::strlen(small)==7);NorthlightPersistentCasters::append(small,sizeof small,"x");assert(std::strlen(small)==7);}
    std::puts("PASS diagnostic path: settling/candidate/parts, shortfall streak and static-scene deferrals with cooldown, ready/published/unseen, worn by a body box (holder key/position/box) or an unaudited group, undecided, stillness resets (root/axes/pose by how much), multi (distinct matrices), rekey (both keys, same/other part, repeated draw), decisions recorded at any distance (worn, oversize, upload, cover), records live as long as their tracks, recording off = identical decisions");
}
// ---- 0.3.150 track order: an index permutation instead of sorting the tracks ----
// Registry::Track's members (heavy: 70 floats and a pose vector).
struct Replica {std::uint64_t key=0;float at[3]={},seen[3]={},axes[9]={},anchor[3]={},anchorAxes[9]={},origin[3]={},originAxes[9]={};
    unsigned stillFrames=0,stillSinceMs=0,lastSeenMs=0,notBeforeMs=0,poseFrames=0,poseSinceMs=0;std::size_t maxCount=0;std::uint32_t entry=0;bool used=false,mobile=false,rejected=false,attachBlocked=false,multi=false;
    signed char placed=0;std::uint32_t serial=0;std::vector<float> pose;};
static bool before(const Replica& a,const Replica& b){return a.key<b.key||(a.key==b.key&&a.at[0]<b.at[0]);}
static bool sameReplica(const Replica& a,const Replica& b){
    return a.key==b.key&&!std::memcmp(a.at,b.at,12)&&!std::memcmp(a.seen,b.seen,12)&&!std::memcmp(a.axes,b.axes,36)&&!std::memcmp(a.anchor,b.anchor,12)&&!std::memcmp(a.anchorAxes,b.anchorAxes,36)&&
        !std::memcmp(a.origin,b.origin,12)&&!std::memcmp(a.originAxes,b.originAxes,36)&&a.stillFrames==b.stillFrames&&a.stillSinceMs==b.stillSinceMs&&a.lastSeenMs==b.lastSeenMs&&a.notBeforeMs==b.notBeforeMs&&
        a.poseFrames==b.poseFrames&&a.poseSinceMs==b.poseSinceMs&&a.maxCount==b.maxCount&&a.entry==b.entry&&a.used==b.used&&a.mobile==b.mobile&&a.rejected==b.rejected&&a.attachBlocked==b.attachBlocked&&
        a.multi==b.multi&&a.placed==b.placed&&a.serial==b.serial&&a.pose==b.pose;}
// Registry::frame() of 0.3.149 after `tracks_.insert(fresh_)`, verbatim (Track = Replica).
static void order0149(std::vector<Replica>& tracks_,std::size_t old,std::size_t trackCapacity,unsigned now){
        std::sort(tracks_.begin()+std::ptrdiff_t(old),tracks_.end(),before);std::inplace_merge(tracks_.begin(),tracks_.begin()+std::ptrdiff_t(old),tracks_.end(),before);
        for(std::size_t i=1;i<tracks_.size();++i)if(before(tracks_[i],tracks_[i-1])){std::sort(tracks_.begin(),tracks_.end(),before);break;} /* roots moved: restore order */
        if(tracks_.size()>trackCapacity){ /* keep casters and the most recently seen */
            std::stable_sort(tracks_.begin(),tracks_.end(),[&](const Replica& a,const Replica& b){return (a.entry!=0)!=(b.entry!=0)?a.entry!=0:unsigned(now-a.lastSeenMs)<unsigned(now-b.lastSeenMs);});
            tracks_.resize(trackCapacity);std::sort(tracks_.begin(),tracks_.end(),before);}
}
static std::uint64_t rank0150(const Replica& t,unsigned now){return (t.entry?std::uint64_t(0):std::uint64_t(1)<<32)|unsigned(now-t.lastSeenMs);}
static void order0150(std::vector<Replica>& tracks,std::size_t old,std::size_t capacity,unsigned now,std::vector<TrackSlot>& slots){
    orderTracks(tracks,old,capacity,[&](const Replica& t){return rank0150(t,now);},slots);}
// The fallback when the slots cannot be allocated.
static void order0150InPlace(std::vector<Replica>& tracks,std::size_t old,std::size_t capacity,unsigned now){
    orderTracksInPlace(tracks,old,capacity,[&](const Replica& t){return rank0150(t,now);});}
// `old` tracks in order except `moved` roots, then `fresh` unsorted: keys 1..keys and x on a 0.5 grid (many
// equal (key, x) pairs, which only std::sort's own choices order), ages in 100 ms steps (equal ages), some casters.
static std::vector<Replica> trackTable(std::mt19937& rng,std::size_t old,std::size_t fresh,unsigned moved,unsigned now,unsigned keys){
    std::uniform_int_distribution<unsigned> key(1,keys),grid(0,40),age(0,40),coin(0,9);std::vector<Replica> t(old+fresh);std::uint32_t serial=1;
    for(auto& r:t){r.key=key(rng);r.at[0]=float(grid(rng))*.5f;r.at[1]=float(serial);r.at[2]=float(coin(rng));r.seen[0]=r.at[0];r.axes[0]=1;r.anchor[1]=float(serial);r.origin[2]=1;
        r.lastSeenMs=now-age(rng)*100u;r.entry=coin(rng)==0?serial:0;r.serial=serial++;r.stillFrames=coin(rng);r.maxCount=coin(rng);r.mobile=coin(rng)<2;r.placed=coin(rng)<3?-1:0;
        r.pose.assign(12*(1+coin(rng)%3),float(r.serial));}
    std::sort(t.begin(),t.begin()+std::ptrdiff_t(old),[](const Replica& a,const Replica& b){return before(a,b)||(!before(b,a)&&a.serial<b.serial);});
    for(unsigned m=0;m<moved&&old;++m)t[rng()%old].at[0]+=float(grid(rng))*.5f-10.f;
    return t;
}
template<class F> static double medianMs(unsigned reps,const std::vector<Replica>& input,F f){std::vector<double> ms;
    for(unsigned r=0;r<reps;++r){auto t=input;const auto start=std::chrono::steady_clock::now();f(t);ms.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
    std::sort(ms.begin(),ms.end());return ms[ms.size()/2];}
// One scripted registry session: crowd identities (keys 1..12, x on a 0.5 grid, some exact
// duplicates: same key and root), new ones every frame, walkers passing their neighbours in
// (key, x), shortfall frames, conversions. The trace: every observation's track index (its
// position in the table), caster and candidate flags, the candidates, the whole table in
// order (key, root, serial, explain() text), entries, removals and events, frame by frame.
template<class Reg,class Obs,class Res,class M> static std::string registryTrace(unsigned seed,std::size_t capacity,unsigned unseenMs,unsigned frames,unsigned crowd,std::size_t* peakTracks,double* frameMs=nullptr,unsigned spawnMax=3,bool trace=true){
    std::decay_t<decltype(std::declval<Reg&>().tuning())> tuning;tuning.trackCapacity=capacity;tuning.unseenMs=unseenMs;tuning.rigidProps=true;tuning.stillMs=160;tuning.stillFrames=8;
    Reg reg(tuning);const float pivot[3]={0,0,0};reg.diagnostics(trace?40.f:0.f,pivot);const Camera c=camera(0,0,2,false);
    std::mt19937 rng(seed);std::uniform_int_distribution<unsigned> key(1,12),grid(0,60),coin(0,99);
    struct Actor {std::uint64_t key=0;float x=0,y=0;unsigned bones=1;};std::vector<Actor> pool;
    auto spawn=[&]{Actor a;a.key=key(rng);a.x=float(grid(rng))*.5f;a.y=float(coin(rng)%20);a.bones=coin(rng)%2?3:1;if(!pool.empty()&&coin(rng)<8)a=pool[rng()%pool.size()];pool.push_back(a);};
    for(unsigned i=0;i<crowd;++i)spawn();
    std::string out;char line[200];unsigned now=5000;std::vector<std::size_t> cand;std::vector<typename Reg::Event> events;std::vector<std::uint32_t> removed;std::deque<std::vector<float>> pool2;
    for(unsigned f=0;f<frames;++f){now+=16;
        for(unsigned s=coin(rng)%(spawnMax+1);s-->0;)spawn();
        for(auto& a:pool)if(coin(rng)<3)a.x+=(coin(rng)%2?.5f:-.5f)*float(1+coin(rng)%4);
        std::vector<Obs> obs;pool2.clear();
        for(const auto& a:pool){if(coin(rng)<25)continue;Obs o;o.key=a.key;o.root[0]=a.x;o.root[1]=a.y;o.root[2]=0;o.axes[0]=o.axes[4]=o.axes[8]=1;o.count=1;o.first=0;o.end=1;
            o.distanceSquared=a.x*a.x+a.y*a.y;o.vertices=100;o.triangles=50;pool2.emplace_back(12*a.bones,0.f);float* p=pool2.back().data();
            for(unsigned b=0;b<a.bones;++b){p[12*b]=p[12*b+4]=p[12*b+8]=1;p[12*b+9]=a.x+.3f*float(b);p[12*b+10]=a.y;p[12*b+11]=1;}o.pose=p;o.poseBones=a.bones;obs.push_back(o);}
        const bool complete=coin(rng)<90;const auto start=std::chrono::steady_clock::now();
        reg.frame(obs,now,pivot,c.inverse,c.projection,complete,cand);
        if(frameMs)*frameMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if(!cand.empty()&&coin(rng)<50){const auto& o=obs[cand[0]];if(reg.room(o,1000,now)){const auto id=reg.begin(o,1000,now,nullptr);
            if(id){Res r;r.id=id;auto m=std::make_shared<M>();m->low=NorthlightGI::Vec3(o.root[0]-1,o.root[1]-1,0);m->high=NorthlightGI::Vec3(o.root[0]+1,o.root[1]+1,2);r.mesh=m;reg.accept(std::move(r),now);reg.ready(id,1000);}}}
        *peakTracks=std::max<std::size_t>(*peakTracks,reg.stats().tracks);if(!trace)continue;
        for(const auto& o:obs){std::snprintf(line,sizeof line,"o%zu,%u,%d,%d ",o.track,o.caster,int(o.candidate),int(o.rigid));out+=line;}
        out+="| ";for(auto k:cand){std::snprintf(line,sizeof line,"%zu ",k);out+=line;}out+="| ";
        reg.tracksNear(pivot,1e6f,[&](std::uint64_t k,const float* r,std::uint32_t serial){std::snprintf(line,sizeof line,"t%llu,%a,%a,%a,%u:",(unsigned long long)k,r[0],r[1],r[2],serial);out+=line;
            typename Reg::Explained e;if(reg.explain(k,r,now,0,e))out+=e.text;out+=' ';});
        for(const auto& e:reg.entries()){std::snprintf(line,sizeof line,"e%u,%llu,%d,%d ",e.id,(unsigned long long)e.key,int(e.state),int(e.published));out+=line;}
        reg.takeRemoved(removed);for(auto id:removed){std::snprintf(line,sizeof line,"r%u ",id);out+=line;}
        reg.takeEvents(events);for(const auto& v:events){std::snprintf(line,sizeof line,"v%u,%u,%llu,%llu ",v.id,v.reason,(unsigned long long)v.key,(unsigned long long)v.other);out+=line;}
        std::snprintf(line,sizeof line,"s%u,%llu\n",reg.stats().tracks,(unsigned long long)reg.stats().submitted);out+=line;
    }
    return out;
}
namespace Old=NorthlightPersistentCasters0149;
static void trackOrder(){
    // (1) The order function against the 0.3.149 lines, field by field: over capacity, equal (key, x), moved roots, new tracks.
    std::mt19937 rng(150);std::vector<TrackSlot> order;unsigned trims=0,resorts=0,cases=0;
    for(unsigned trial=0;trial<3000;++trial){const unsigned now=1000000+trial;const std::size_t old=rng()%300,fresh=rng()%40;const unsigned moved=rng()%4;
        const std::size_t capacity=trial%3==0?old+fresh+rng()%3:std::max<std::size_t>(1,(old+fresh)*(50+rng()%50)/100);
        const auto input=trackTable(rng,old,fresh,moved,now,trial%2?3:12);auto a=input,b=input,c=input;
        order0149(a,old,capacity,now);order0150(b,old,capacity,now,order);order0150InPlace(c,old,capacity,now);
        assert(a.size()==b.size()&&a.size()==c.size());for(std::size_t i=0;i<a.size();++i)assert(sameReplica(a[i],b[i])&&sameReplica(a[i],c[i]));
        trims+=input.size()>capacity;resorts+=moved>0;++cases;}
    for(unsigned moved=0;moved<3;++moved)for(std::size_t fresh:{std::size_t(0),std::size_t(8),std::size_t(300)}){const unsigned now=2000000;
        const auto input=trackTable(rng,8192,fresh,moved,now,12);auto a=input,b=input;order0149(a,8192,8192,now);order0150(b,8192,8192,now,order);
        assert(a.size()==b.size());for(std::size_t i=0;i<a.size();++i)assert(sameReplica(a[i],b[i]));++cases;}
    std::printf("PASS track order == 0.3.149 field by field (slots and the in-place fallback): %u tables (%u over capacity, %u with moved roots, 8192+{0,8,300}), equal (key, x) ties\n",cases,trims,resorts);
    // (2) Whole registries (0.3.149 header copy vs this one), frame by frame, on scripted sessions.
    struct Session {unsigned seed;std::size_t capacity;unsigned unseenMs,frames,crowd;};
    for(const Session& s:{Session{1,48,3000,700,30},Session{2,48,60000,700,60},Session{3,200,1500,500,150},Session{4,4096,60000,12,4200}}){
        std::size_t peakOld=0,peakNew=0;
        const auto a=registryTrace<Old::Registry,Old::Observation,Old::Result,Old::Mesh>(s.seed,s.capacity,s.unseenMs,s.frames,s.crowd,&peakOld);
        const auto b=registryTrace<Registry,Observation,Result,Mesh>(s.seed,s.capacity,s.unseenMs,s.frames,s.crowd,&peakNew);
        if(a!=b){std::size_t i=0;while(i<a.size()&&i<b.size()&&a[i]==b[i])++i;std::printf("FAIL registry trace differs at byte %zu: ...%s | ...%s\n",i,a.substr(i>80?i-80:0,160).c_str(),b.substr(i>80?i-80:0,160).c_str());}
        assert(a==b&&peakOld==s.capacity&&peakNew==s.capacity&&a.size()>1000);
        std::printf("PASS registry == 0.3.149 frame by frame: capacity %zu, unseen %u ms, %u frames, trace %zu bytes\n",s.capacity,s.unseenMs,s.frames,a.size());}
    // (3) Informational: 8200 tracks (8192 kept), median of 31 runs.
    const unsigned now=3000000;
    const auto trim=trackTable(rng,8192,8,0,now,12),trimMoved=trackTable(rng,8192,8,2,now,12),resort=trackTable(rng,8192,0,2,now,12),steady=trackTable(rng,8192,0,0,now,12);
    auto oldMs=[&](const std::vector<Replica>& in,std::size_t old){return medianMs(31,in,[&](std::vector<Replica>& t){order0149(t,old,8192,now);});};
    auto newMs=[&](const std::vector<Replica>& in,std::size_t old){return medianMs(31,in,[&](std::vector<Replica>& t){order0150(t,old,8192,now,order);});};
    std::printf("BENCH track order, 8200 tracks (sizeof Track %zu B): trim (8192+8 new) old %.3f ms new %.3f ms; trim with 2 moved roots old %.3f ms new %.3f ms; re-sort (2 moved roots) old %.3f ms new %.3f ms; ordered old %.3f ms new %.3f ms\n",
        sizeof(Replica),oldMs(trim,8192),newMs(trim,8192),oldMs(trimMoved,8192),newMs(trimMoved,8192),oldMs(resort,8192),newMs(resort,8192),oldMs(steady,8192),newMs(steady,8192));
    double frameOld=0,frameNew=0;std::size_t peakOld=0,peakNew=0;
    registryTrace<Old::Registry,Old::Observation,Old::Result,Old::Mesh>(9,8192,60000,40,8200,&peakOld,&frameOld,12,false);
    registryTrace<Registry,Observation,Result,Mesh>(9,8192,60000,40,8200,&peakNew,&frameNew,12,false);
    std::printf("BENCH Registry::frame at 8192 tracks (~6000 observations, walkers, new identities), mean of 40 frames: 0.3.149 %.3f ms, 0.3.150 %.3f ms\n",frameOld/40,frameNew/40);
}
int main(int argc,char** argv){
    assert(argc>1);transform(argv[1]);lifecycle();budget();invalidation();parity();worker();screen();rigidLifecycle();rigidHeld(argv[1]);cutoutParity();rigidSign(argv[1]);rigidBudget();rigidDespawn();placementIndex();oneBoneProgram(argv[1]);realSign();staticFlood();diagnosticPath();trackOrder();
    std::puts("persistent casters: all passed");
}
