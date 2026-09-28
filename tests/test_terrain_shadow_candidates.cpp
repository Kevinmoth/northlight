// 0.3.175 (S2) terrain shadow selection (terrain_shadow_candidates.h): prepareListed() over the
// per-generation terrain-outside-fixed list with the ChunkSet bitmap selects exactly prepare()'s
// candidates in the same order (random batches and chunk sets, chunks outside the grid, -1 chunks);
// forEach() replays them, reuses the membership work and falls back identically.
#include "terrain_shadow_candidates.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <random>
#include <set>
#include <vector>
struct Batch {int chunkX=-1,chunkY=-1;bool terrain=false;};
using Chunks=std::set<std::pair<int,int>>;
using namespace NorthlightTerrainCandidates;
int main(){
    std::mt19937 rng(175);unsigned cases=0;std::size_t selected=0;
    for(unsigned trial=0;trial<400;++trial){
        std::uniform_int_distribution<int> chunk(trial%4==0?-2:0,trial%4==1?1030:120),coin(0,99);
        std::vector<Batch> batches(1+rng()%3000);for(auto& b:batches){b.terrain=coin(rng)<70;if(coin(rng)<95){b.chunkX=chunk(rng);b.chunkY=chunk(rng);}}
        Chunks fixed,liveSet;ChunkSet live;
        for(unsigned i=rng()%600;i-->0;)fixed.emplace(chunk(rng),chunk(rng));
        for(unsigned i=rng()%300;i-->0;){const int x=chunk(rng),y=chunk(rng);liveSet.emplace(x,y);live.emplace(x,y);}
        if(coin(rng)<30){live.clear();liveSet.clear();for(unsigned i=rng()%50;i-->0;){const int x=chunk(rng),y=chunk(rng);liveSet.emplace(x,y);live.emplace(x,y);}} /* a frame's clear */
        assert(live.size()==liveSet.size()&&std::equal(live.begin(),live.end(),liveSet.begin()));
        for(int x=-3;x<1034;x+=7)for(int y=-3;y<1034;y+=11)assert(live.count({x,y})==liveSet.count({x,y}));
        std::vector<std::uint32_t> list;for(std::uint32_t i=0;i<batches.size();++i)if(batches[i].terrain&&!fixed.count({batches[i].chunkX,batches[i].chunkY}))list.push_back(i);
        // The reference: the set-based rule over every batch.
        Scratch<> a,b;Selection<> old(a),listed(b);old.prepare(batches,fixed,liveSet,true);listed.prepareListed(batches,list,fixed,live,true);
        assert(old.ready()&&listed.ready()&&old.candidates()==listed.candidates());
        std::vector<const Batch*> first,second;
        assert(old.forEach(batches,fixed,liveSet,[&](const Batch& x){first.push_back(&x);return true;}));
        assert(listed.forEach(batches,fixed,live,[&](const Batch& x){second.push_back(&x);return true;}));
        assert(first==second);selected+=first.size();
        // A second pass reuses the prepared membership work; another live set falls back to the full rule (same output).
        second.clear();assert(listed.forEach(batches,fixed,live,[&](const Batch& x){second.push_back(&x);return true;})&&first==second);
        assert(listed.stats().reusedMembershipChecks==list.size()&&listed.stats().scanned==list.size()&&listed.stats().fallbackPasses==0);
        ChunkSet other;for(const auto& c:liveSet)other.emplace(c.first,c.second);second.clear();
        assert(listed.forEach(batches,fixed,other,[&](const Batch& x){second.push_back(&x);return true;})&&first==second&&listed.stats().fallbackPasses==1);
        ++cases;}
    // Informational: 8000 batches, 4000 fixed chunks, 150 live.
    {std::vector<Batch> batches(8000);Chunks fixed,liveSet;ChunkSet live;std::uniform_int_distribution<int> chunk(300,420);
     for(auto& x:batches){x.terrain=rng()%100<85;x.chunkX=chunk(rng);x.chunkY=chunk(rng);}for(unsigned i=0;i<4000;++i)fixed.emplace(chunk(rng),chunk(rng));
     for(unsigned i=0;i<150;++i){const int x=chunk(rng),y=chunk(rng);liveSet.emplace(x,y);live.emplace(x,y);}
     std::vector<std::uint32_t> list;for(std::uint32_t i=0;i<batches.size();++i)if(batches[i].terrain&&!fixed.count({batches[i].chunkX,batches[i].chunkY}))list.push_back(i);
     auto time=[&](auto f){double best=1e9;for(int r=0;r<21;++r){Scratch<> s;Selection<> sel(s);const auto t=std::chrono::steady_clock::now();f(sel);best=std::min(best,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());}return best;};
     const double before=time([&](Selection<>& s){s.prepare(batches,fixed,liveSet);}),after=time([&](Selection<>& s){s.prepareListed(batches,list,fixed,live);});
     std::printf("BENCH terrain selection, 8000 batches / 4000 fixed / 150 live: set rule %.3f ms, listed + bitmap %.3f ms (native, best of 21)\n",before,after);}
    std::printf("PASS terrain shadow candidates: %u random cases (%zu selections), listed + bitmap == set rule, same order, reuse and fallback unchanged\n",cases,selected);
}
