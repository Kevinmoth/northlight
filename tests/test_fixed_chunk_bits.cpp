// 0.3.176 (U1c): the fixed-chunk bitmap beside the set. Membership equals the set for every grid cell
// and for coordinates outside the grid (the (-1,-1) of a non-chunk triangle, and past 1023), across
// reassignment to another set; appendLiveDirectionalBy() with the bitmap gives the same directional
// lists and per-triangle test counts as appendLiveDirectional() with the set, including straddling
// snapshots. Native, no game or GPU.
#include "shadow_terrain.h"
#include <cassert>
#include <cstdio>
#include <random>
using NorthlightGI::Vec3;
struct Chunk {int x,y;};struct Bounds {std::vector<Chunk> chunks;};
struct Snapshot {Bounds bounds;std::vector<Vec3> positions;std::vector<uint32_t> indices;};
using Set=std::set<std::pair<int,int>>;
static void membership(){
    std::mt19937 rng(1176);NorthlightShadowTerrain::FixedChunkBits bits;
    assert(!bits.source()&&!bits.contains({0,0})&&!bits.contains({-1,-1}));
    for(unsigned round=0;round<6;++round){
        Set set;const unsigned n=round==0?0:round==1?1:1000+rng()%8000;
        while(set.size()<n)set.insert({int(rng()%1024),int(rng()%1024)});
        if(round>=2){set.insert({-1,-1});set.insert({1024,7});set.insert({3,-5});} /* the set's own out-of-grid entries */
        if(round==5)set.insert({0,0}),set.insert({1023,1023}),set.insert({1023,0}),set.insert({0,1023});
        bits.assign(&set);assert(bits.source()==&set);
        size_t members=0;
        for(int y=0;y<1024;++y)for(int x=0;x<1024;++x){const bool in=set.count({x,y})!=0;assert(bits.contains({x,y})==in);members+=in;}
        for(auto c:{std::pair<int,int>{-1,-1},{1024,7},{3,-5},{-1,0},{0,-1},{1024,1024},{7,1024},{-2147483647-1,5}})assert(bits.contains(c)==(set.count(c)!=0));
        assert(members+(round>=2?3:0)==set.size());
    }
    Set empty;bits.assign(&empty);for(int y=0;y<1024;y+=7)for(int x=0;x<1024;x+=5)assert(!bits.contains({x,y}));
    bits.reset();assert(!bits.source()&&!bits.contains({1,1}));
}
static void directional(){
    using NorthlightShadowTerrain::Origin;using NorthlightShadowTerrain::Chunk;
    std::mt19937 rng(2176);size_t lists=0,tests=0,straddling=0,outside=0;
    for(unsigned round=0;round<400;++round){
        // A 3x3 chunk block around one chunk; triangles inside one chunk, across a border, or spanning several.
        const int cx=100+int(rng()%800),cy=100+int(rng()%800);
        Set fixed;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(rng()%2)fixed.insert({cx+dx,cy+dy});
        if(rng()%2)fixed.insert({-1,-1});
        NorthlightShadowTerrain::FixedChunkBits bits;bits.assign(&fixed);
        Snapshot s;
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(rng()%3)s.bounds.chunks.push_back({cx+dx,cy+dy});
        // world x from chunk y, world y from chunk x (NorthlightShadowTerrain::chunk)
        auto at=[&](double fx,double fy){return Vec3(float(Origin-(cy+fy)*Chunk),float(Origin-(cx+fx)*Chunk),float(rng()%50));};
        const unsigned triangles=1+rng()%40;
        for(unsigned t=0;t<triangles;++t){
            const unsigned kind=rng()%3;std::uniform_real_distribution<double> in(.1,.9),wide(-1.5,1.9);
            const double ox=kind==0?0:double(int(rng()%3)-1),oy=kind==0?0:double(int(rng()%3)-1);
            for(unsigned v=0;v<3;++v){
                const double fx=kind==2?wide(rng):ox+in(rng),fy=kind==2?wide(rng):oy+in(rng);
                s.indices.push_back(uint32_t(s.positions.size()));s.positions.push_back(at(fx,fy));}
            const Vec3* p=&s.positions[s.positions.size()-3];
            if(NorthlightShadowTerrain::chunk(p[0],p[1],p[2]).first<0)++outside;
        }
        std::vector<uint32_t> bySet={5},byBits={5};size_t setTests=0,bitTests=0;
        NorthlightShadowTerrain::appendLiveDirectional(s,fixed,17,bySet,&setTests);
        NorthlightShadowTerrain::appendLiveDirectionalBy(s,[&](std::pair<int,int> c){return bits.contains(c);},17,byBits,&bitTests);
        assert(bySet==byBits&&setTests==bitTests);
        std::vector<uint32_t> plain={5};NorthlightShadowTerrain::appendLiveDirectional(s,fixed,17,plain);assert(plain==bySet); /* the counter is optional */
        ++lists;tests+=setTests;straddling+=setTests!=0;
    }
    assert(straddling>50&&outside>50);
    std::printf("bitmap directional == set directional: snapshots=%zu straddling=%zu triangleTests=%zu nonChunkTriangles=%zu\n",lists,straddling,tests,outside);
}
int main(){membership();directional();std::puts("fixed chunk bits: membership == set (grid, outside, reassignment), directional lists == set path passed");}
