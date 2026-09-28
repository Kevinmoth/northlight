// 0.3.141 persistent casters: CPU transform accuracy against the replay VS
// emulation (and an independent double-precision skin), conversion gates, and
// the registry lifecycle (still -> caster, off-screen persistence, movement,
// memory cap, cache-slot invalidation). Built by test_persistent_casters.py.
#include "persistent_casters.h"
#include "persistent_casters_0149.h" /* the harness: reference/persistent_casters-0.3.149.h in namespace NorthlightPersistentCasters0149 */
#include "replay_bounds.h"
#include "world_math.h"
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
// Referenced palette slots (persistentDrawBones): the one-influence program reads lane x whatever the
// weights; the four-weight blend, the weighted lanes. (The one-influence program itself: test_rigid_memory.)
static void referencedSlotLanes(){
    auto slots=[](NorthlightActorDeformation::Four i,NorthlightActorDeformation::Four w,unsigned lanes){std::vector<unsigned> s;const bool ok=referencedSlots(i,w,lanes,[&](unsigned x){s.push_back(x);});return ok?s:std::vector<unsigned>{99};};
    assert((slots({0,7,3,9},{1,0,0,0},1)==std::vector<unsigned>{0})&&(slots({0,7,3,9},{.5f,.5f,0,0},1)==std::vector<unsigned>{0})&&(slots({2,7,3,9},{1,1,1,1},1)==std::vector<unsigned>{2}));
    assert((slots({0,7,3,9},{.5f,.5f,0,0},4)==std::vector<unsigned>{0,7})&&(slots({0,75,3,9},{1,0,0,0},1)==std::vector<unsigned>{0})&&(slots({75,0,3,9},{1,0,0,0},1)==std::vector<unsigned>{99})&&(slots({1.5f,0,0,0},{1,0,0,0},4)==std::vector<unsigned>{99}));
    std::puts("PASS referenced slots: lane x only for the one-influence program, weighted lanes for the blend, 0..74 integral");
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
// order (key, root, serial), entries, removals and events, frame by frame. The general class only: the
// rigid class and its diagnostics (explain(), diagnostics()) were retired in 0.3.172, and with
// rigidProps=false (its default) the 0.3.149 registry takes none of their branches.
template<class Reg,class Obs,class Res,class M> static std::string registryTrace(unsigned seed,std::size_t capacity,unsigned unseenMs,unsigned frames,unsigned crowd,std::size_t* peakTracks,double* frameMs=nullptr,unsigned spawnMax=3,bool trace=true){
    std::decay_t<decltype(std::declval<Reg&>().tuning())> tuning;tuning.trackCapacity=capacity;tuning.unseenMs=unseenMs;tuning.stillMs=160;tuning.stillFrames=8;
    Reg reg(tuning);const float pivot[3]={0,0,0};const Camera c=camera(0,0,2,false);
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
        for(const auto& o:obs){std::snprintf(line,sizeof line,"o%zu,%u,%d ",o.track,o.caster,int(o.candidate));out+=line;}
        out+="| ";for(auto k:cand){std::snprintf(line,sizeof line,"%zu ",k);out+=line;}out+="| ";
        reg.tracksNear(pivot,1e6f,[&](std::uint64_t k,const float* r,std::uint32_t serial){std::snprintf(line,sizeof line,"t%llu,%a,%a,%a,%u ",(unsigned long long)k,r[0],r[1],r[2],serial);out+=line;});
        for(const auto& e:reg.entries()){std::snprintf(line,sizeof line,"e%u,%llu,%d,%d ",e.id,(unsigned long long)e.key,int(e.state),int(e.published));out+=line;}
        reg.takeRemoved(removed);for(auto id:removed){std::snprintf(line,sizeof line,"r%u ",id);out+=line;}
        reg.takeEvents(events);for(const auto& v:events){std::snprintf(line,sizeof line,"v%u,%u,%d,%d,%u,%a,%a,%a ",v.id,v.reason,int(v.ready),int(v.rejected),v.bones,v.root[0],v.root[1],v.root[2]);out+=line;}
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
    assert(argc>1);transform(argv[1]);lifecycle();budget();invalidation();parity();worker();screen();referencedSlotLanes();trackOrder();
    std::puts("persistent casters: all passed");
}
