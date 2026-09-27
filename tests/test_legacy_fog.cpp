#include "legacy_fog.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <limits>
#include <vector>
using namespace NorthlightLegacyFog;
static bool close(float a,float b){return std::fabs(a-b)<2e-6f;}
static void same(RGB a,RGB b){for(unsigned i=0;i<3;++i)assert(close(a[i],b[i]));}

int main(int argc,char**argv){
    std::vector<uint32_t> shader={0xffff0300,
        0x0200001f,0x8000000b,0x90010004,
        0x03000002,0x80070000,0x80e40000,0xa1e40006,
        0x04000004,0x80070800,0x90000004,0x80e40000,0xa0e40006,
        0x0000ffff};
    assert(ps3FogColor6(shader.data(),shader.size()));
    assert(ps3FogColorRegister(shader.data(),shader.size())==6);
    auto patchShader=shader;patchShader[7]=0xa1e40002;patchShader[12]=0xa0e40002;
    assert(ps3FogColorRegister(patchShader.data(),patchShader.size())==2);
    auto broken=shader;broken[12]=0xa0e40007;assert(!ps3FogColor6(broken.data(),broken.size()));
    broken=shader;broken[2]=0x80000005;assert(!ps3FogColor6(broken.data(),broken.size()));
    broken=shader;broken[9]|=0x00100000;assert(!ps3FogColor6(broken.data(),broken.size()));
    broken=shader;broken.insert(broken.end()-1,{0x02000001,0x800f0800,0x80e40000});
    assert(!ps3FogColor6(broken.data(),broken.size()));
    broken=shader;broken.insert(broken.begin()+4,{0x05000051,0xa00f0006,0,0,0,0});
    assert(!ps3FogColor6(broken.data(),broken.size()));
    broken=shader;broken.insert(broken.end()-1,{0x02000001,0x80080800,0xa0ff0000});
    assert(ps3FogColor6(broken.data(),broken.size()));
    assert(!ps3FogColor6(shader.data(),shader.size()-1));
    assert(!ps3FogColor6(nullptr,0));

    float rh[]={.0025f,1.25f,1,0},lh[]={-.0025f,1.25f,1,0},fog[]={.6f,.7f,.8f,0};
    Constants c;
    assert(decode(3,true,false,0,rh,fog,0,-1,c));
    assert(c.parameters[3]==1&&close(visibility(c,-100),1)&&close(visibility(c,-300),.5)&&close(visibility(c,-500),0));
    assert(decode(3,true,false,0,lh,fog,0,1,c));
    assert(close(visibility(c,100),1)&&close(visibility(c,300),.5)&&close(visibility(c,500),0));
    assert(!decode(3,false,true,0,lh,fog,0,1,c)&&visibility(c,500)==1&&c.color[0]==0);
    assert(!decode(3,true,true,0,rh,fog,0,1,c)); // Wrong signed-view direction.
    assert(!decode(2,false,true,1,rh,nullptr,0xff336699,-1,c)); // Table fog unsupported.
    assert(decode(2,false,false,1,nullptr,nullptr,0,-1,c)&&visibility(c,-500)==1);
    assert(decode(2,false,true,0,rh,nullptr,0xff336699,-1,c));
    assert(close(c.color[0],.2)&&close(c.color[1],.4)&&close(c.color[2],.6));
    float bad[]={.0025f,1.25f,std::numeric_limits<float>::quiet_NaN(),0};
    assert(!decode(3,true,true,0,bad,fog,0,-1,c)&&c.parameters[3]==0);

    RGB F={.6f,.7f,.8f},surface={.2f,.3f,.4f};
    unsigned fullFogCases=0,continuityCases=0,floorCases=0,volumeCases=0;
    for(unsigned normal=0;normal<=20;++normal){
        float diffuse=float(normal)/20;
        RGB oldLight={.2f+.6f*diffuse,.25f+.7f*diffuse,.3f+.8f*diffuse};
        RGB delta={-.6f*diffuse,-.7f*diffuse,-.8f*diffuse};
        same(relight(F,oldLight,delta,0,F),F);++fullFogCases;
        for(float T:{0.f,.00001f,.01f,.5f,.99999f,1.f}){
            RGB input;
            for(unsigned i=0;i<3;++i)input[i]=T*surface[i]+(1-T)*F[i];
            auto lit=relight(input,oldLight,delta,T,F);
            for(unsigned i=0;i<3;++i){
                assert(lit[i]+2e-6f>=.55f*T*surface[i]+(1-T)*F[i]);
                if(T<.00002f)assert(std::fabs(lit[i]-F[i])<.00002f);
            }
            ++continuityCases;
        }
    }
    for(float blackness:{0.f,.00001f,.01f,.2f,1.f}){
        RGB input={blackness,blackness,blackness};
        auto lit=relight(input,{.8f,.8f,.8f},{-10,-10,-10},1,{0,0,0});
        same(lit,{blackness*.55f,blackness*.55f,blackness*.55f});++floorCases;
    }
    same(relight({0,0,0},{.2f,.2f,.2f},{100,100,100},1,F),{0,0,0});
    for(float T:{0.f,.01f,.5f,1.f})for(float Tv:{0.f,.01f,.5f,1.f}){
        RGB input,scatter;
        for(unsigned i=0;i<3;++i){input[i]=T*surface[i]+(1-T)*F[i];scatter[i]=(1-Tv)*surface[i];}
        same(composeVolume(input,input,T,F,Tv,scatter),input); // Equilibrium source.
        same(composeVolume(input,input,T,F,1,{0,0,0}),input); // Empty added medium.
        same(composeVolume(F,F,0,F,Tv,{.9f,.1f,.3f}),F); // Original fog preserved.
        ++volumeCases;
    }
    RGB darkInterior={.02f,.03f,.04f};
    // A scene-level terrain estimate cannot invent atmospheric color on an
    // interior darker than the estimated fog. Zero added scatter stays dark.
    same(composeVolume(darkInterior,darkInterior,.1f,F,.5f,{0,0,0}),darkInterior);
    same(composeVolume({0,0,0},{0,0,0},.1f,F,.5f,{0,0,0}),{0,0,0});
    // Equivalence of the optimized five cross taps and zero-weight cascades.
    const int offsets[5][2]={{0,0},{-2,0},{2,0},{0,-2},{0,2}};
    for(int i=0;i<5;++i){float axis=std::clamp(float(i-2),0.f,1.f),parity=i-std::floor(i*.5f)*2;
        float extent=std::min(i,1)*(2-4*parity);
        assert((1-axis)*extent==offsets[i][0]&&axis*extent==offsets[i][1]);}
    for(float blend:{0.f,.01f,.5f,.99f,1.f}){
        float skipped=0;if(1-blend>0)skipped+=(1-blend)*.3f;if(blend>0)skipped+=blend*.8f;
        assert(close(skipped,(1-blend)*.3f+blend*.8f));
    }
    std::printf("PASS full_fog=%u continuity=%u floor=%u volume=%u\n",fullFogCases,continuityCases,floorCases,volumeCases);
    for(int i=1;i<argc;++i){
        std::ifstream file(argv[i],std::ios::binary|std::ios::ate);assert(file);
        auto bytes=file.tellg();assert(bytes>0&&bytes%4==0);file.seekg(0);
        std::vector<uint32_t> words(size_t(bytes)/4);file.read(reinterpret_cast<char*>(words.data()),bytes);assert(file);
        std::printf("PS3_FOG_REGISTER %d %s\n",ps3FogColorRegister(words.data(),words.size()),argv[i]);
    }
}
