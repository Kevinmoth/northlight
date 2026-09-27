#include "replay_capture_constants.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <random>
#include <vector>
using namespace NorthlightReplayCaptureConstants;
struct Packet {
    float constantStorage[1024]={};int boolStorage[16]={},intStorage[64]={};
    const float* constants=constantStorage;const int* bools=boolStorage;const int* ints=intStorage;
    NorthlightConstantEpoch::Stamp constantStamp;
    NorthlightShaderConstants::Usage constantUsage;
    unsigned projectionKind=2,constantGroup=0;
    Packet()=default;Packet(const Packet&)=delete;Packet& operator=(const Packet&)=delete;
};
struct Source {
    NorthlightConstantEpoch::Clock clock;
    float floats[1024]={};int booleans[16]={},integers[64]={};
    bool enabled=true,fail=false,mutateWhileReading=false;size_t reads=0,bytes=0;
    static bool reader(void* context,NorthlightConstantEpoch::Stamp& stamp){
        auto& s=*static_cast<Source*>(context);stamp=s.clock.stamp(s.enabled);return stamp.valid;
    }
    bool fetch(Packet& p){
        ++reads;if(fail)return false;
        const auto& u=p.constantUsage;
        std::memcpy(p.constantStorage+4*u.floats.first,floats+4*u.floats.first,u.floats.count*16);
        std::memcpy(p.boolStorage+u.booleans.first,booleans+u.booleans.first,u.booleans.count*4);
        std::memcpy(p.intStorage+4*u.integers.first,integers+4*u.integers.first,u.integers.count*16);
        bytes+=u.floats.count*16+u.booleans.count*4+u.integers.count*16;
        if(mutateWhileReading)clock.invalidate();return true;
    }
};
static void equal(const Packet& a,const Packet& b){
    const auto& u=a.constantUsage;
    assert(!std::memcmp(a.constants+4*u.floats.first,b.constants+4*u.floats.first,u.floats.count*16));
    assert(!std::memcmp(a.bools+u.booleans.first,b.bools+u.booleans.first,u.booleans.count*4));
    assert(!std::memcmp(a.ints+4*u.integers.first,b.ints+4*u.integers.first,u.integers.count*16));
}
static bool get(Packet& p,const Packet* previous,Source& source,Stats* stats=nullptr){
    return capture(p,previous,&source,Source::reader,[&]{return source.fetch(p);},stats);
}
static void edges(){
    Source s;Packet first,next,rejected;Stats stats;
    assert(get(first,nullptr,s,&stats));assert(s.reads==1);
    assert(get(next,&first,s,&stats));assert(s.reads==1&&next.constants==first.constants&&stats.hits==1);
    assert(get(rejected,&next,s,&stats));assert(rejected.constants==first.constants);
    reset(rejected);assert(rejected.constants==rejected.constantStorage&&!rejected.constantStamp.valid);
    s.floats[100]=2;s.clock.writeFloat(25,1);assert(get(rejected,&next,s));
    assert(first.constants[100]==0&&next.constants[100]==0&&rejected.constants[100]==2);
    assert(!samePose(next,rejected));
    // A projection-only write must still copy its new bytes, while sharing pose identity.
    s.floats[8]=7;s.clock.writeFloat(2,1);Packet projection;
    assert(get(projection,&rejected,s));assert(projection.constants!=rejected.constants);
    assert(samePose(rejected,projection,&stats)&&projection.constants[8]==7);
    s.clock.writeFloat(2,1);Packet redundant;
    assert(get(redundant,&projection,s)&&samePose(projection,redundant));
    // A -> B -> A between draws is a conservative epoch miss, then exact-byte pose hit.
    s.floats[100]=3;s.clock.writeFloat(25,1);s.floats[100]=2;s.clock.writeFloat(25,1);
    Packet changedBack;auto reads=s.reads;
    assert(get(changedBack,&redundant,s)&&s.reads==reads+1&&samePose(redundant,changedBack));
    // Uncaptured/failed/recorded/disabled state cannot acquire reusable authority.
    s.clock.invalidate();s.fail=true;Packet failed;assert(!get(failed,&changedBack,s)&&!failed.constantStamp.valid);
    s.fail=false;s.enabled=false;assert(get(failed,&changedBack,s)&&!failed.constantStamp.valid);
    s.enabled=true;s.mutateWhileReading=true;Packet raced;assert(get(raced,&failed,s)&&!raced.constantStamp.valid);
    s.mutateWhileReading=false;Packet fallback;reads=s.reads;
    assert(capture(fallback,&changedBack,nullptr,nullptr,[&]{return s.fetch(fallback);})&&s.reads==reads+1&&!fallback.constantStamp.valid);
    // A successful getter for one layout is not proof for other register ranges.
    Packet narrow;narrow.constantUsage.floats={2,4};narrow.constantUsage.booleans={};narrow.constantUsage.integers={};
    assert(get(narrow,&changedBack,s));Packet wide;reads=s.reads;
    assert(get(wide,&narrow,s)&&s.reads==reads+1&&!compatible(narrow,wide));
    Packet kind1;kind1.projectionKind=1;reads=s.reads;assert(get(kind1,&wide,s)&&s.reads==reads+1);
    Packet unsupported;unsupported.projectionKind=3;assert(!compatible(unsupported,unsupported));
    Packet invalid;invalid.constantUsage.floats={250,10};assert(!compatible(invalid,invalid));
    // Signed zero and NaN payloads are compared as bits on fallback.
    Packet bits1,bits2;uint32_t nan=0x7fc01234;std::memcpy(s.floats+100,&nan,4);s.clock.writeFloat(25,1);assert(get(bits1,nullptr,s));
    assert(get(bits2,&bits1,s)&&samePose(bits1,bits2));nan++;std::memcpy(s.floats+100,&nan,4);s.clock.writeFloat(25,1);assert(get(bits2,&bits1,s)&&!samePose(bits1,bits2));
    s.floats[100]=0;s.clock.writeFloat(25,1);assert(get(bits1,nullptr,s));s.floats[100]=-0.f;s.clock.writeFloat(25,1);assert(get(bits2,&bits1,s)&&!samePose(bits1,bits2));
}
int main(){
    edges();std::mt19937 random(127);Source source;Stats stats;
    size_t frames=100,draws=300,legacyBytes=0,accepted=0;
    for(size_t frame=0;frame<frames;++frame){
        source.clock.invalidate();std::vector<std::unique_ptr<Packet>> actual,reference;
        for(size_t draw=0;draw<draws;++draw){
            if(draw%7==0){unsigned bank=random()%3,reg=random()%(bank?16:256);uint32_t bits=random();
                if(bank==0){std::memcpy(source.floats+reg*4,&bits,4);source.clock.writeFloat(reg,1);}
                if(bank==1){std::memcpy(source.booleans+reg,&bits,4);source.clock.writeBool(1);}
                if(bank==2){std::memcpy(source.integers+reg*4,&bits,4);source.clock.writeInt(1);}}
            if(draw%29==0){source.clock.invalidate();}
            source.enabled=draw%41>1;
            auto p=std::make_unique<Packet>(),r=std::make_unique<Packet>();
            if((draw/47)%2){p->constantUsage.floats={2,32};p->constantUsage.booleans={2,4};p->constantUsage.integers={3,7};}
            p->projectionKind=(draw/79)%2?1:2;r->projectionKind=p->projectionKind;r->constantUsage=p->constantUsage;
            const auto* previous=actual.empty()?nullptr:actual.back().get();
            assert(get(*p,previous,source,&stats));
            const auto before=source.bytes;assert(source.fetch(*r));legacyBytes+=source.bytes-before;
            equal(*p,*r);
            p->constantGroup=previous?previous->constantGroup+!samePose(*previous,*p,&stats):0;
            r->constantGroup=reference.empty()?0:reference.back()->constantGroup+!NorthlightReplayPoses::sameCaptured(*reference.back(),*r);
            assert(p->constantGroup==r->constantGroup);
            if(draw%19==0){reset(*p);assert(p->constants==p->constantStorage);continue;}
            ++accepted;actual.push_back(std::move(p));reference.push_back(std::move(r));
        }
        // Old captures must remain immutable through subsequent writes and every cascade.
        for(unsigned pass=0;pass<6;++pass){
            NorthlightReplayPoses::Pass<int> a,b;float projection[16];for(auto& value:projection)value=float(random());
            for(size_t i=0;i<actual.size();++i){equal(*actual[i],*reference[i]);
                assert(a.prepare(*actual[i],projection)&&b.prepare(*reference[i],projection));
                assert(a.floats.first==b.floats.first&&a.floats.count==b.floats.count);
                assert(a.booleans.first==b.booleans.first&&a.booleans.count==b.booleans.count);
                assert(a.integers.first==b.integers.first&&a.integers.count==b.integers.count);
                const auto u=actual[i]->constantUsage.floats;assert(!std::memcmp(a.desired()+4*u.first,b.desired()+4*u.first,u.count*16));
            }
            assert(a.prepared==b.prepared&&a.reused==b.reused);
        }
        // Destruction/reset may happen in any order after all consumers finish.
        for(auto& p:actual)reset(*p);
    }
    assert(stats.hits>10000&&stats.poseFastHits>10000&&stats.bytesAvoided>10000000);
    std::printf("{\"draws\":%zu,\"accepted\":%zu,\"snapshotHits\":%llu,\"poseFastHits\":%llu,\"bytesAvoided\":%llu,\"legacyBytes\":%zu}\n",frames*draws,accepted,(unsigned long long)stats.hits,(unsigned long long)stats.poseFastHits,(unsigned long long)stats.bytesAvoided,legacyBytes);
}
