#include "world_dynamic_probes.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <random>

// 0.3.138 static path replay: solveProbeMoving and the layer's record-aware
// solve must equal solveProbePrepared(..., &moving) in every Probe field on
// randomized static/actor scenes, across actor moves and record invalidation.
using namespace NorthlightGI;
using Random=std::mt19937;
static float uniform(Random& r,float a,float b){return std::uniform_real_distribution<float>(a,b)(r);}
static void box(WorldScene& s,Vec3 c,Vec3 h,uint32_t m,Random& r){
    const Vec3 p[8]={{c.x-h.x,c.y-h.y,c.z-h.z},{c.x+h.x,c.y-h.y,c.z-h.z},{c.x+h.x,c.y+h.y,c.z-h.z},{c.x-h.x,c.y+h.y,c.z-h.z},
                     {c.x-h.x,c.y-h.y,c.z+h.z},{c.x+h.x,c.y-h.y,c.z+h.z},{c.x+h.x,c.y+h.y,c.z+h.z},{c.x-h.x,c.y+h.y,c.z+h.z}};
    const unsigned f[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{2,3,7,6},{1,2,6,5},{3,0,4,7}};
    for(auto& q:f){uint32_t n=uint32_t(s.vertices.size());Vec3 normal=normalized(cross(p[q[1]]-p[q[0]],p[q[2]]-p[q[0]]));
        for(unsigned k=0;k<4;++k)s.vertices.push_back({p[q[k]],normal,uniform(r,-2,2),uniform(r,-2,2)});
        s.triangles.push_back({n,n+1,n+2,m});s.triangles.push_back({n,n+2,n+3,m});}
}
static WorldMaterial material(Random& r,bool textured){
    WorldMaterial m;m.albedo={uniform(r,.1f,.9f),uniform(r,.1f,.9f),uniform(r,.1f,.9f)};
    if(textured){m.width=m.height=8;m.rgba.resize(256);for(auto& b:m.rgba)b=uint8_t(r());m.addressU=1+r()%3;m.addressV=1+r()%3;}
    return m;
}
static BVH build(WorldScene s){BVH b;std::string error;assert(b.build(std::move(s),error));return b;}
static WorldScene world(Random& r){
    WorldScene s;s.materials.push_back(material(r,false));s.materials.push_back(material(r,true));
    box(s,{0,0,-1},{40,40,1},0,r);
    for(int i=0;i<24;++i)box(s,{uniform(r,-30,30),uniform(r,-30,30),uniform(r,0,8)},{uniform(r,.5f,4),uniform(r,.5f,4),uniform(r,.5f,6)},r()%2,r);
    return s;
}
static WorldScene actors(Random& r,unsigned count){
    WorldScene s;
    for(unsigned i=0;i<count;++i){s.materials.push_back(material(r,r()%2));
        box(s,{uniform(r,-12,12),uniform(r,-12,12),uniform(r,.2f,3)},{uniform(r,.2f,1.5f),uniform(r,.2f,1.5f),uniform(r,.3f,2)},i,r);}
    return s;
}
static void same(const Probe& a,const Probe& b){
    assert(!std::memcmp(&a.position,&b.position,sizeof a.position)&&!std::memcmp(a.sh,b.sh,sizeof a.sh)&&!std::memcmp(a.moments,b.moments,sizeof a.moments));
    assert(a.samples==b.samples&&a.backFaceSamples==b.backFaceSamples&&!std::memcmp(&a.maxDistance,&b.maxDistance,4)&&a.valid==b.valid);
}
int main(){
    Random r(1380);MovingSolveStats total;unsigned probes=0;
    for(unsigned scene=0;scene<6;++scene){
        BVH bvh=build(world(r));
        Lighting l;l.sunDirection={uniform(r,-1,1),uniform(r,-1,1),uniform(r,.1f,1)};l.maxBounces=scene%4;
        if(scene%2)l.additionalDirections.push_back({{uniform(r,-1,1),uniform(r,-1,1),.5f},{.3f,.2f,.1f}});
        for(unsigned i=0;i<4;++i)l.points.push_back({{uniform(r,-20,20),uniform(r,-20,20),uniform(r,1,6)},{2,1,.5f},1,uniform(r,6,24)});
        const auto prepared=prepareLighting(l);assert(prepared.valid());
        const unsigned rays=scene==5?7:32;
        std::vector<Vec3> positions;for(unsigned i=0;i<10;++i)positions.push_back({uniform(r,-16,16),uniform(r,-16,16),uniform(r,.5f,6)});
        std::vector<StaticPathRecord> records(positions.size());
        for(unsigned pose=0;pose<5;++pose){
            BVH moving=build(actors(r,1+pose*3));
            for(size_t i=0;i<positions.size();++i){
                const uint32_t seed=uint32_t(i*977+scene);MovingSolveStats stats;
                same(solveProbeMoving(bvh,positions[i],prepared,rays,seed,moving,records[i],1+scene,&stats),
                     solveProbePrepared(bvh,positions[i],prepared,rays,seed,&moving));
                assert(stats.recorded==(pose==0)&&stats.replayed+stats.retraced==rays);
                total.recorded+=stats.recorded;total.replayed+=stats.replayed;total.retraced+=stats.retraced;++probes;
            }
        }
        // Every record input invalidates: generation, BVH, position, seed, rays.
        BVH moving=build(actors(r,6));BVH copy=build(bvh.scene());
        auto check=[&](const BVH& b,Vec3 p,unsigned n,uint32_t seed,uint64_t generation,bool record){
            MovingSolveStats stats;same(solveProbeMoving(b,p,prepared,n,seed,moving,records[0],generation,&stats),solveProbePrepared(b,p,prepared,n,seed,&moving));
            assert(stats.recorded==record);};
        const uint32_t s0=scene;
        check(bvh,positions[0],rays,s0,1+scene,false);
        check(bvh,positions[0],rays,s0,99,true);check(bvh,positions[0],rays,s0,99,false);
        check(copy,positions[0],rays,s0,99,true);
        check(copy,positions[1],rays,s0,99,true);check(copy,positions[1],rays,s0+1,99,true);check(copy,positions[1],rays+1,s0+1,99,true);
        check(copy,positions[1],rays+1,s0+1,99,false);
        // Changed lighting under the caller's new generation.
        auto brighter=l;brighter.sunIrradiance={2,2,2};const auto next=prepareLighting(brighter);MovingSolveStats stats;
        same(solveProbeMoving(copy,positions[1],next,rays+1,1,moving,records[0],100,&stats),solveProbePrepared(copy,positions[1],next,rays+1,1,&moving));
        assert(stats.recorded==1);
    }
    assert(total.replayed>total.retraced&&total.retraced>0);
    std::printf("PASS solveProbeMoving bit-identical over %u probes: %llu rays replayed, %llu retraced\n",probes,
        (unsigned long long)total.replayed,(unsigned long long)total.retraced);
    // Adversarial actors: triangles exactly at recorded query ends (the
    // trace/occluded [.0005,max) tie), grazing along a query, enclosing the
    // probe, and far beyond the weight radius.
    {
        BVH bvh=build(world(r));Lighting l;l.maxBounces=3;l.points.push_back({{3,2,4},{2,2,2},1,20});const auto prepared=prepareLighting(l);
        const Vec3 p{1,-2,2.5f};StaticPathRecord record;MovingSolveStats stats;BVH none=build(actors(r,1));
        solveProbeMoving(bvh,p,prepared,32,5,none,record,1,&stats);assert(stats.recorded==1&&record.queries.size()>40);
        for(unsigned variant=0;variant<4;++variant)for(size_t q=0;q<record.queries.size();q+=7){
            const auto query=record.queries[q];const Vec3 d=normalized(query.direction);
            const Vec3 helper=std::fabs(d.z)<.9f?Vec3(0,0,1):Vec3(1,0,0),t=normalized(cross(helper,d)),b=cross(d,t);
            WorldScene s;s.materials.push_back(material(r,false));
            auto tri=[&](Vec3 a,Vec3 c,Vec3 e){uint32_t n=uint32_t(s.vertices.size());for(Vec3 v:{a,c,e})s.vertices.push_back({v,d,0,0});s.triangles.push_back({n,n+1,n+2,0});};
            const float at=std::min(query.maxDistance,60.f);const Vec3 end=query.origin+d*at;
            if(variant==0)tri(end-t-b,end+t*2-b,end-t+b*2);
            else if(variant==1)tri(query.origin+t*.0001f,end+t*.0001f,end+t*.0001f+d);
            else if(variant==2){box(s,p,{.3f,.3f,.3f},0,r);}
            else box(s,p+Vec3(40,0,0),{1,1,1},0,r);
            BVH moving=build(s);MovingSolveStats replay;
            same(solveProbeMoving(bvh,p,prepared,32,5,moving,record,1,&replay),solveProbePrepared(bvh,p,prepared,32,5,&moving));
            assert(replay.recorded==0);
        }
    }
    std::puts("PASS tie at recorded query end, grazing, enclosing and far actors match the direct solve");
    // Invalid input matches the direct path, and leaves no usable record.
    {BVH bvh=build(world(r)),moving=build(actors(r,2));StaticPathRecord record;const auto prepared=prepareLighting(Lighting{});
     same(solveProbeMoving(bvh,{NAN,0,0},prepared,32,1,moving,record,1),solveProbePrepared(bvh,{NAN,0,0},prepared,32,1,&moving));
     same(solveProbeMoving(bvh,{0,0,1},prepared,5,1,moving,record,1),solveProbePrepared(bvh,{0,0,1},prepared,5,1,&moving));
     Lighting bad;bad.maxDistance=-1;same(solveProbeMoving(bvh,{0,0,1},prepareLighting(bad),32,1,moving,record,1),solveProbePrepared(bvh,{0,0,1},prepareLighting(bad),32,1,&moving));
     assert(record.rays.empty());}
    // Layer: record-aware solve publishes the same atlas as the direct solve
    // through observe/apply cycles with moving actors, and records age out.
    {
        BVH bvh=build(world(r));Lighting l;l.maxBounces=2;const auto prepared=prepareLighting(l);
        ProbeCache cache;
        for(int z=0;z<2;++z)for(int y=-3;y<=3;++y)for(int x=-3;x<=3;++x){Vec3 p{float(x)*8,float(y)*8,float(z)*8};
            ProbeGridKey key{x,y,z};Probe s=solveProbePrepared(bvh,p,prepared,16,probeSeed(key));cache.put(key,s);}
        DynamicProbeLayer direct,replay;std::vector<WorldScene> poses;
        for(unsigned i=0;i<4;++i)poses.push_back(actors(r,4));
        poses.push_back(poses.back());for(auto& v:poses.back().vertices)v.position.x+=3;
        direct.reset(&poses[0]);replay.reset(&poses[0]);MovingSolveStats stats;unsigned nulls=0;
        for(size_t step=0;step<poses.size();++step){
            if(step){direct.observe(&poses[step]);replay.observe(&poses[step]);}
            BVH moving=build(poses[step]);
            auto a=cache.atlas(),b=cache.atlas();unsigned ra=0,ca=0,rb=0,cb=0;
            assert(direct.apply(a,[&](ProbeGridKey key,Vec3 p){return solveProbePrepared(bvh,p,prepared,16,probeSeed(key),&moving);},[]{return false;},ra,ca));
            assert(replay.apply(b,[&](ProbeGridKey key,Vec3 p,StaticPathRecord* record){
                if(!record){++nulls;return solveProbePrepared(bvh,p,prepared,16,probeSeed(key),&moving);}
                return solveProbeMoving(bvh,p,prepared,16,probeSeed(key),moving,*record,7,&stats);},[]{return false;},rb,cb));
            assert(ra==rb&&ca==cb&&ca>0);
            for(size_t i=0;i<a.size();++i){assert(a[i].occupied==b[i].occupied);if(a[i].occupied)same(a[i].probe,b[i].probe);}
        }
        assert(nulls==0&&stats.replayed>0&&stats.recorded==replay.pathRecords());
        assert(replay.pathBytes()>0);
        for(unsigned i=0;i<17;++i)replay.observe(nullptr);assert(replay.pathRecords()==0&&replay.pathBytes()==0);
        replay.reset(nullptr);assert(replay.pathRecords()==0&&replay.pathBytes()==0);
    }
    std::puts("PASS layer record-aware solve equals direct solve across poses; records bounded, aged and reset");
}
