#include "replay_capture_constants.h"
#include "replay_bounds.h"
#include <cassert>
#include <cstdio>
#include <memory>
struct CapturePacket {
    float constantStorage[1024];int boolStorage[16]={},intStorage[64]={};
    const float* constants=constantStorage;const int* bools=boolStorage;const int* ints=intStorage;
    NorthlightConstantEpoch::Stamp constantStamp;NorthlightShaderConstants::Usage constantUsage;
    unsigned projectionKind=2,constantGroup=0;
    CapturePacket(){for(auto& value:constantStorage)value=std::numeric_limits<float>::quiet_NaN();}
};
struct ConstantSource {
    float constants[1024]={};NorthlightConstantEpoch::Clock clock;
    static bool read(void* raw,NorthlightConstantEpoch::Stamp& stamp){stamp=static_cast<ConstantSource*>(raw)->clock.stamp(true);return true;}
};
std::vector<uint32_t> shader(unsigned major,bool relative){
    std::vector<uint32_t> w={major==1?0xfffe0101u:major==2?0xfffe0200u:0xfffe0300u};
    auto op=[&](unsigned code,std::initializer_list<uint32_t> args){w.push_back(code|(major==1?0:unsigned(args.size())<<24));w.insert(w.end(),args);};
    op(81,{0xa00f0000,0x40400000,0x3f800000,0,0});
    op(31,{0x80000000,0x900f0000});if(relative)op(31,{0x80000002,0x900f0001});
    if(major==3)op(31,{0x80000000,0xe00f0000});
    op(1,{0x80080000,0xa0550000});
    if(relative){op(5,{0x80010001,0x90000001,0xa0000000});op(major==1?1:46,{0xb0010000,0x80000001});}
    for(unsigned row=0;row<3;++row){
        auto target=0x80000000u|(1u<<(16+row));
        if(relative&&major>1)op(9,{target,0xa0e4201fu+row,0xb0000000,0x90e40000});
        else op(9,{target,(relative?0xa0e4201fu:0xa0e4000au)+row,0x90e40000});
    }
    for(unsigned row=0;row<4;++row)op(9,{(major==3?0xe0000000u:0xc0000000u)|(1u<<(16+row)),0xa0e40002u+row,0x80e40000});
    w.push_back(0xffff);return w;
}
int main(){
    NorthlightDrawSnapshot::Mesh mesh;mesh.vertexCount=3;mesh.primitiveCount=1;mesh.indexed=true;mesh.indices={0,1,2};mesh.streams[0].stride=20;mesh.streams[0].bytes.resize(60);
    float xyz[3][3]={{2,3,4},{5,-2,9},{-7,8,-1}};
    for(unsigned i=0;i<3;++i){std::memcpy(mesh.streams[0].bytes.data()+i*20,xyz[i],12);mesh.streams[0].bytes[i*20+16]=i;}
    D3DVERTEXELEMENT9 decl[]={{0,0,2,0,0,0},{0,16,5,0,2,0},{0xff,0,17,0,0,0}};
    unsigned cases=0;NorthlightReplayCaptureConstants::Stats stats;
    for(unsigned major=1;major<=3;++major)for(bool relative:{false,true}){
        auto words=shader(major,relative);NorthlightActorDeformation::Program program;
        assert(NorthlightActorDeformation::compile(words.data(),words.size(),program));
        const auto usage=NorthlightShaderConstants::analyze(words.data(),words.size(),2);
        assert(usage.analyzed&&usage.relativeFloat==relative&&usage.booleans.count==0&&usage.integers.count==0);
        assert(usage.floats.count==(relative?256:11));
        ConstantSource source;
        const unsigned first=relative?31:10;
        for(unsigned bone=0;bone<(relative?3u:1u);++bone){unsigned r=first+3*bone;source.constants[r*4]=1;source.constants[(r+1)*4+1]=1;source.constants[(r+2)*4+2]=1;source.constants[r*4+3]=float(bone)*13;}
        std::vector<std::unique_ptr<CapturePacket>> packets;
        for(unsigned draw=0;draw<128;++draw){
            if(draw%4==0){source.constants[first*4+3]=float(draw);source.clock.writeFloat(first,1);}
            auto packet=std::make_unique<CapturePacket>();packet->constantUsage=usage;
            const auto* previous=packets.empty()?nullptr:packets.back().get();
            assert(NorthlightReplayCaptureConstants::capture(*packet,previous,&source,ConstantSource::read,[&]{std::memcpy(packet->constantStorage+usage.floats.first*4,source.constants+usage.floats.first*4,usage.floats.count*16);return true;},&stats));
            // This is the full-size actor enqueue copy. Outside compact Usage,
            // every register is poison, including c0 overridden by shader DEF.
            float queued[1024];std::memcpy(queued,packet->constants,sizeof packet->constantStorage);
            float inverse[16]={};for(unsigned n=0;n<4;++n)inverse[n*5]=1;inverse[12]=-float(draw)*2;inverse[13]=float(draw);
            std::vector<NorthlightActorDeformation::Position> expected,actual;
            assert(NorthlightActorDeformation::worldPositions(program,mesh,decl,3,source.constants,inverse,expected));
            assert(NorthlightActorDeformation::worldPositions(program,mesh,decl,3,queued,inverse,actual));
            assert(expected.size()==actual.size()&&!std::memcmp(expected.data(),actual.data(),actual.size()*sizeof(actual[0])));
            NorthlightReplayBounds::Bounds a,b;NorthlightReplayBounds::Budget ba,bb;
            assert(NorthlightReplayBounds::calculate(program,mesh,decl,3,source.constants,inverse,ba,a)==NorthlightReplayBounds::Status::Valid);
            assert(NorthlightReplayBounds::calculate(program,mesh,decl,3,packet->constants,inverse,bb,b)==NorthlightReplayBounds::Status::Valid);
            assert(!std::memcmp(a.low,b.low,sizeof a.low)&&!std::memcmp(a.high,b.high,sizeof a.high));
            ++cases;packets.push_back(std::move(packet));
        }
    }
    assert(stats.hits==576);
    std::printf("{\"consumerCases\":%u,\"snapshotHits\":%llu,\"compactPoisonAndRelativeShaders\":6}\n",cases,(unsigned long long)stats.hits);
}
