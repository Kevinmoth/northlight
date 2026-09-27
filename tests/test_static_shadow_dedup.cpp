#include "static_shadow_dedup.h"
#include <cassert>
#include <cstdio>
using namespace StaticShadowDedup;
static StaticShadow::Snapshot fixture(){
    StaticShadow::Snapshot s;s.map="Azeroth";s.mapGeneration=1;
    auto m=std::make_shared<StaticShadow::Model>();m->key="fixture-content";m->contentRevision=7;
    m->vertices={{0,0,0,0,0},{1,0,0,1,0},{1,1,0,1,1},{0,1,0,0,1}};
    m->indices={0,1,2,0,2,3};m->batches.push_back({0,6,0,{0,0,0},{1,1,0}});m->materials.push_back({});m->materials[0].alphaCutoff=0;
    s.models[m->key]=m;StaticShadow::Placement p;p.uid=42;p.category=2;p.modelKey=m->key;p.translation={8,10,2};p.low={8,10,2};p.high={9,11,2};s.placements.push_back(p);return s;
}
static std::vector<Triangle> geometry(){return {{{{8,10,2},{9,10,2},{9,11,2}}},{{{8,10,2},{9,11,2},{8,11,2}}}};}
static Eligibility allowed(){return {true,false,false,false};}
static Budget fresh(){Budget b;b.maxMilliseconds=1000;return b;}
int main(){
    auto s=fixture();auto tris=geometry();Matcher matcher;Budget b=fresh();
    auto ready=[](const StaticShadow::Placement&,uint64_t){return 5u;};
    auto q=matcher.match(s,tris,allowed(),b,ready);assert(q.valid&&q.uid==42&&q.covers(0)&&!q.covers(1)&&q.covers(2));
    // Opaque geometry is double-sided: different order/winding is equivalent.
    std::swap(tris[0][0],tris[0][2]);std::swap(tris[0],tris[1]);b=fresh();assert(matcher.match(s,tris,allowed(),b,ready).valid);
    // One matching triangle is insufficient when another is uncovered, even
    // when the aggregate bounds and triangle count remain identical.
    auto wrong=tris;wrong[0][1]={8.5f,10.5f,2};b=fresh();assert(!matcher.match(s,wrong,allowed(),b,ready).valid);
    wrong=tris;wrong[0][0].x=std::nextafter(wrong[0][0].x,9.f);b=fresh();assert(!matcher.match(s,wrong,allowed(),b,ready).valid);
    wrong=tris;wrong[0][0].x=std::numeric_limits<float>::quiet_NaN();b=fresh();assert(!matcher.match(s,wrong,allowed(),b,ready).valid);
    for(unsigned i=0;i<4;++i){auto e=allowed();if(i==0)e.auditedRigid=false;if(i==1)e.dynamic=true;if(i==2)e.skinned=true;if(i==3)e.alphaTest=true;b=fresh();assert(!matcher.match(s,tris,e,b,ready).valid);}
    b=fresh();assert(!matcher.match(s,tris,allowed(),b,[](const StaticShadow::Placement&,uint64_t){return 0u;}).valid);
    auto old=s.models.begin()->second;auto replacement=std::make_shared<StaticShadow::Model>(*old);replacement->contentRevision=8;replacement->indices.resize(3);replacement->batches[0].indexCount=3;s.models.begin()->second=replacement;
    b=fresh();assert(!matcher.match(s,tris,allowed(),b,ready).valid); // old proof cannot hide resource replacement
    s=fixture();s.placements[0].category=1;b=fresh();assert(!matcher.match(s,tris,allowed(),b,ready).valid); // M2 bindpose never eligible
    s=fixture();auto cutout=std::make_shared<StaticShadow::Model>(*s.models.begin()->second);cutout->materials[0].alphaCutoff=.5f;s.models.begin()->second=cutout;b=fresh();assert(!matcher.match(s,tris,allowed(),b,ready).valid);
    s=fixture();s.map="Kalimdor";s.mapGeneration=2;s.placements.clear();b=fresh();assert(!matcher.match(s,tris,allowed(),b,ready).valid&&matcher.bytes()==0);
    s=fixture();b=fresh();b.maxTriangles=1;assert(!matcher.match(s,tris,allowed(),b,ready).valid);
    b=fresh();b.maxMilliseconds=0;assert(!matcher.match(s,tris,allowed(),b,ready).valid);
    Matcher tiny(sizeof(Canonical));b=fresh();assert(!tiny.match(s,tris,allowed(),b,ready).valid&&tiny.bytes()==0);
    // Memo exact byte comparison, map/reset invalidation and current residency.
    Memo memo;memo.context("Azeroth",1,1);auto owner=std::make_shared<int>(17);std::vector<uint8_t> evidence={1,2,3,4};memo.remember(owner,evidence,q);
    auto proofReady=[](const Match&){return 1u;};assert(memo.find(owner,evidence,proofReady).covers(0));
    auto changed=evidence;changed[3]++;assert(!memo.find(owner,changed,proofReady).valid);
    assert(!memo.find(owner,evidence,[](const Match&){return 0u;}).valid);
    memo.context("Azeroth",1,2);assert(!memo.find(owner,evidence,proofReady).valid);
    memo.remember(owner,evidence,q);memo.context("Kalimdor",1,2);assert(!memo.find(owner,evidence,proofReady).valid);
    std::vector<NorthlightGI::Vec3> positions={{0,0,0},{1,0,0},{0,1,0},{1,1,0}};
    std::vector<Triangle> extracted;std::vector<uint32_t> list={0,1,2,1,3,2};
    assert(makeTriangles(positions,list,true,2,false,extracted)&&extracted.size()==2);
    assert(makeTriangles(positions,{},false,2,true,extracted)&&extracted.size()==2);
    assert(!makeTriangles(positions,{0,1,9},true,1,false,extracted)&&extracted.empty());
    assert(!makeTriangles(positions,list,true,2,false,extracted,1)&&extracted.empty());
    matcher.clear();assert(matcher.bytes()==0);
    std::puts("PASS strict opaque WMO full coverage, order/winding, near matches, dynamic/alpha/unknown exclusion, GPU slot residency, resource/map/reset invalidation and budgets");
}
