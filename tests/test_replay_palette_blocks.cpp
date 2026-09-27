// Differential test: captureBlocks()/samePose() block serials against the
// full-copy capture + byte comparison on randomized write sequences.
#include "replay_capture_constants.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <memory>
#include <random>
#include <vector>
using namespace NorthlightReplayCaptureConstants;
struct Packet {
    float constantStorage[1024];int boolStorage[16]={},intStorage[64]={};
    const float* constants=constantStorage;const int* bools=boolStorage;const int* ints=intStorage;
    NorthlightConstantEpoch::Stamp constantStamp;
    NorthlightShaderConstants::Usage constantUsage;
    unsigned projectionKind=2,constantGroup=0;
    // Poison pooled bytes: only captured ranges may ever be read or compared.
    Packet(){for(auto& value:constantStorage)value=std::numeric_limits<float>::quiet_NaN();}
    Packet(const Packet&)=delete;Packet& operator=(const Packet&)=delete;
};
struct Source {
    NorthlightConstantEpoch::Clock clock;
    float floats[1024]={};int booleans[16]={},integers[64]={};
    bool enabled=true,failFloats=false,mutateWhileReading=false;size_t fullReads=0,floatReads=0,bytes=0;
    static bool reader(void* context,NorthlightConstantEpoch::Stamp& stamp){
        auto& s=*static_cast<Source*>(context);stamp=s.clock.stamp(s.enabled);return stamp.valid;
    }
    bool fetch(Packet& p){
        ++fullReads;const auto& u=p.constantUsage;
        std::memcpy(p.constantStorage+4*u.floats.first,floats+4*u.floats.first,u.floats.count*16);
        std::memcpy(p.boolStorage+u.booleans.first,booleans+u.booleans.first,u.booleans.count*4);
        std::memcpy(p.intStorage+4*u.integers.first,integers+4*u.integers.first,u.integers.count*16);
        bytes+=u.floats.count*16+u.booleans.count*4+u.integers.count*16;return true;
    }
    bool fetchFloats(unsigned first,unsigned count,float* out){
        ++floatReads;if(failFloats)return false;assert(first+count<=256);
        std::memcpy(out,floats+4*first,count*16);bytes+=count*16;
        if(mutateWhileReading){floats[4*first]+=1;clock.writeFloat(first,1);}
        return true;
    }
    // Mirrors MirrorDevice: the serial advances after the backend accepted it.
    void writeFloats(unsigned first,unsigned count,const float* data){
        for(unsigned r=0;r<count;++r)if(first+r<256)std::memcpy(floats+4*(first+r),data+4*r,16);
        clock.writeFloat(first,count);
    }
};
static void equal(const Packet& a,const Packet& b){
    const auto& u=a.constantUsage;
    assert(!std::memcmp(a.constants+4*u.floats.first,b.constants+4*u.floats.first,u.floats.count*16));
    assert(!std::memcmp(a.bools+u.booleans.first,b.bools+u.booleans.first,u.booleans.count*4));
    assert(!std::memcmp(a.ints+4*u.integers.first,b.ints+4*u.integers.first,u.integers.count*16));
}
static bool get(Packet& p,const Packet* previous,Source& s,Stats* stats=nullptr){
    return captureBlocks(p,previous,&s,Source::reader,[&]{return s.fetch(p);},
        [&](unsigned first,unsigned count,float* out){return s.fetchFloats(first,count,out);},stats);
}
static void serials(){
    using namespace NorthlightConstantEpoch;Clock c;
    auto a=c.stamp(true);unsigned first=0,last=0;
    c.writeFloat(255,1);auto b=c.stamp(true);dirtySpan(a,b,0,256,first,last);assert(first==240&&last==256);
    dirtySpan(a,b,0,240,first,last);assert(first==last);
    c.writeFloat(250,20);auto d=c.stamp(true);dirtySpan(b,d,0,256,first,last);assert(first==240&&last==256);
    c.writeFloat(300,4);auto e=c.stamp(true);dirtySpan(d,e,0,256,first,last);assert(first==last&&!exact(d,e));
    c.writeFloat(15,2);auto f=c.stamp(true);dirtySpan(e,f,0,256,first,last);assert(first==0&&last==32);
    dirtySpan(e,f,2,30,first,last);assert(first==2&&last==30);
    c.writeFloat(0,0);assert(exact(f,c.stamp(true)));
    dirtySpan(f,f,250,300,first,last);assert(first==250&&last==300); // invalid range: all dirty, never "proven clean"
    dirtySpan(f,f,10,10,first,last);assert(first==last);
    // Different blocks: conservative contiguous span; a rewrite always dirties.
    c.writeFloat(20,1);c.writeFloat(100,1);auto g=c.stamp(true);dirtySpan(f,g,0,256,first,last);assert(first==16&&last==112);
    c.writeFloat(20,1);auto h=c.stamp(true);dirtySpan(g,h,0,256,first,last);assert(first==16&&last==32);
    // Bank serials: invalidate/apply/reset and unread bank writes.
    c.writeBool(1);auto i=c.stamp(true);assert(banks(h,i,false,true)&&!banks(h,i,true,false));
    c.invalidate();auto j=c.stamp(true);assert(!banks(i,j,false,false));
    assert(!banks(j,c.stamp(false),false,false));
}
static void edges(){
    Source s;Stats stats;Packet first,next,copy,rewrite,failed,raced;
    assert(get(first,nullptr,s,&stats)&&s.fullReads==1);
    float bones[16*4];for(unsigned n=0;n<64;++n)bones[n]=float(n);
    s.writeFloats(40,16,bones);assert(get(next,&first,s,&stats));
    assert(s.fullReads==1&&s.floatReads==1+ValidateEvery&&next.constants==next.constantStorage&&stats.blockCopies==1);assert(!std::memcmp(next.constants,s.floats,1024*4));
    assert(!samePose(first,next,&stats)&&next.constants[4*40+1]==1);
    float same[4];std::memcpy(same,s.floats+4*50,16);s.writeFloats(50,1,same);
    assert(get(rewrite,&next,s,&stats)&&rewrite.constants==next.constants&&stats.blockRewrites==1);
    assert(samePose(next,rewrite,&stats));
    s.writeFloats(2,1,bones);assert(get(copy,&rewrite,s,&stats)&&samePose(rewrite,copy,&stats)); // projection row only
    s.writeFloats(9,1,bones+4);s.failFloats=true;assert(!get(failed,&copy,s)&&!failed.constantStamp.valid);s.failFloats=false;
    s.mutateWhileReading=true;auto reads=s.fullReads;assert(get(raced,&copy,s)&&s.fullReads==reads+1);s.mutateWhileReading=false;
    assert(!raced.constantStamp.valid&&!std::memcmp(raced.constants,s.floats,1024*4));
    // Writes straddling the projection rows (kind 2: c2..5, kind 1: c4..7):
    // the pose excludes only projection rows, capture keeps every byte.
    for(unsigned kind:{1u,2u}){Packet base,projection,material;base.projectionKind=projection.projectionKind=material.projectionKind=kind;
        s.clock.invalidate();assert(get(base,nullptr,s));
        const unsigned projectionRow=kind==1?6:3,materialRow=kind==1?3:6;float rows[8*4];
        std::memcpy(rows,s.floats+4,sizeof rows);rows[4*(projectionRow-1)]+=1;s.writeFloats(1,8,rows); // c1..c8 straddle
        assert(get(projection,&base,s)&&projection.constants!=base.constants&&!std::memcmp(projection.constants,s.floats,1024*4));
        assert(samePose(base,projection)&&NorthlightReplayPoses::sameCaptured(base,projection));
        std::memcpy(rows,s.floats+4,sizeof rows);rows[4*(materialRow-1)]+=1;s.writeFloats(1,8,rows);
        assert(get(material,&projection,s)&&material.constants!=projection.constants&&!std::memcmp(material.constants,s.floats,1024*4));
        assert(!samePose(projection,material)&&!NorthlightReplayPoses::sameCaptured(projection,material));
    }
    s.clock.invalidate();Packet applied;reads=s.fullReads;assert(get(applied,&copy,s)&&s.fullReads==reads+1);
    s.enabled=false;Packet disabled;reads=s.fullReads;assert(get(disabled,&applied,s)&&s.fullReads==reads+1&&!disabled.constantStamp.valid);
}
int main(){
    serials();edges();std::mt19937 random(136);Source source;Stats stats;
    size_t frames=200,draws=400,accepted=0,legacyBytes=0;
    const NorthlightShaderConstants::Range layouts[]={{0,256},{0,256},{0,256},{2,32},{0,40},{2,120}};
    for(size_t frame=0;frame<frames;++frame){
        source.clock.invalidate();std::vector<std::unique_ptr<Packet>> actual,reference;
        unsigned layout=0,kind=2;
        for(size_t draw=0;draw<draws;++draw){
            const unsigned action=random()%100;
            float data[64*4];for(auto& v:data){uint32_t bits=random()%4?random()%64:random();std::memcpy(&v,&bits,4);}
            if(action<25){unsigned first=random()%260,count=1+random()%(random()%4?4:64);source.writeFloats(first,count,data);} // partial/overlapping
            else if(action<40){unsigned first=random()%256,count=1+random()%24;if(first+count>256)count=256-first;
                float copy[24*4];std::memcpy(copy,source.floats+4*first,count*16);source.writeFloats(first,count,copy);} // identical-value rewrite
            else if(action<45){unsigned first=random()%250;source.writeFloats(first,1+random()%6,data);
                source.writeFloats(first+1,2,data+64);} // overlapping second write
            else if(action<48){source.floats[4*(random()%256)]=float(random());source.clock.invalidate();} // state-block Apply
            else if(action<50){source.booleans[random()%16]=int(random()%2);source.clock.writeBool(1);}
            else if(action<52){source.integers[random()%64]=int(random());source.clock.writeInt(1);}
            else if(action<54){source.clock.writeFloat(300+random()%100,4);}
            source.enabled=draw%53>1;
            if(draw%37==0)layout=random()%6;
            if(draw%61==0)kind=random()%2?1:2;
            auto p=std::make_unique<Packet>(),r=std::make_unique<Packet>();
            p->constantUsage.floats=layouts[layout];
            if(layout==3){p->constantUsage.booleans={2,4};p->constantUsage.integers={3,7};}
            if(layout==4){p->constantUsage.booleans={};p->constantUsage.integers={};}
            p->projectionKind=kind;r->projectionKind=kind;r->constantUsage=p->constantUsage;
            const auto* previous=actual.empty()?nullptr:actual.back().get();
            source.mutateWhileReading=!ValidateEvery&&draw%97==5;source.failFloats=!ValidateEvery&&draw%89==7; // validation reads would observe injected faults
            // A self-check read is not a race model: keep it out of raced draws.
            const bool ok=get(*p,previous,source,source.mutateWhileReading?nullptr:&stats);
            source.mutateWhileReading=false;source.failFloats=false;
            if(!ok){reset(*p);continue;}
            // The reference is the full-copy path at the same device state.
            const auto before=source.bytes;assert(source.fetch(*r));legacyBytes+=source.bytes-before;
            equal(*p,*r);
            const bool poseActual=previous&&samePose(*previous,*p,&stats);
            const bool poseReference=!reference.empty()&&NorthlightReplayPoses::sameCaptured(*reference.back(),*r);
            assert(poseActual==poseReference);
            p->constantGroup=previous?previous->constantGroup+!poseActual:0;
            r->constantGroup=reference.empty()?0:reference.back()->constantGroup+!poseReference;
            if(draw%23==0){reset(*p);continue;}
            ++accepted;actual.push_back(std::move(p));reference.push_back(std::move(r));
        }
        // Shared storage must stay immutable through later writes and all passes.
        for(unsigned pass=0;pass<3;++pass){
            NorthlightReplayPoses::Pass<int> a,b;float projection[16];for(auto& value:projection)value=float(random());
            for(size_t i=0;i<actual.size();++i){equal(*actual[i],*reference[i]);
                assert(a.prepare(*actual[i],projection)&&b.prepare(*reference[i],projection));
                assert(a.floats.first==b.floats.first&&a.floats.count==b.floats.count);
                const auto u=actual[i]->constantUsage.floats;assert(!std::memcmp(a.desired()+4*u.first,b.desired()+4*u.first,u.count*16));
            }
            assert(a.prepared==b.prepared&&a.reused==b.reused);
        }
        for(auto& p:actual)reset(*p);
    }
    assert(stats.blockCopies>1000&&stats.blockShared>1000&&stats.blockRewrites>500&&stats.poseBlockHits>1000);
    assert(stats.selfChecks>10&&stats.selfCheckMismatches==0);
    // Validation build: every certified capture was re-read (raced draws run without stats).
    if(ValidateEvery)assert(stats.selfChecks==stats.hits+stats.blockTests);
    std::printf("{\"draws\":%zu,\"accepted\":%zu,\"epochHits\":%llu,\"blockTests\":%llu,\"blockShared\":%llu,\"blockRewrites\":%llu,\"blockCopies\":%llu,\"registersReused\":%llu,\"registersFetched\":%llu,\"poseFastHits\":%llu,\"poseBlockHits\":%llu,\"poseBytesAvoided\":%llu,\"bytesAvoided\":%llu,\"selfChecks\":%llu,\"selfCheckMismatches\":%llu,\"validateEvery\":%d,\"legacyBytes\":%zu}\n",
        frames*draws,accepted,(unsigned long long)stats.hits,(unsigned long long)stats.blockTests,(unsigned long long)stats.blockShared,
        (unsigned long long)stats.blockRewrites,(unsigned long long)stats.blockCopies,(unsigned long long)stats.registersReused,
        (unsigned long long)stats.registersFetched,(unsigned long long)stats.poseFastHits,(unsigned long long)stats.poseBlockHits,
        (unsigned long long)stats.poseBytesAvoided,(unsigned long long)stats.bytesAvoided,(unsigned long long)stats.selfChecks,
        (unsigned long long)stats.selfCheckMismatches,int(ValidateEvery),legacyBytes);
}
