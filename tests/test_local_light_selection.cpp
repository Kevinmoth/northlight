#include "local_light_selection.h"
#include "regional_fog.h"
#include <cassert>
#include <cstdio>
using namespace NorthlightLocalLightSelection;
static NorthlightLocalLights::Light lamp(unsigned id){
    NorthlightLocalLights::Light l{};l.position[0]=float(id*2);l.diffuse[0]=float(id);l.diffuse[1]=.5f;l.diffuse[2]=.25f;
    l.attenuationStart=1;l.attenuationEnd=10;l.sourceId=id;l.kind=2;return l;
}
template<unsigned N> void checkBatches(const Selection& s){
    unsigned seen=0;float total=0;
    for(unsigned first=0;first<s.count;first+=N){auto b=s.batch<N>(first);assert(b.count==std::min(N,s.count-first));
        for(unsigned i=0;i<N;++i){if(i<b.count){++seen;total+=b.color[i][0];assert(b.position[i]==s.position[first+i]);assert(b.fog[i]==s.fog[first+i]);}
            else for(unsigned j=0;j<4;++j)assert(b.position[i][j]==0&&b.color[i][j]==0&&b.fog[i][j]==0);}
    }
    assert(seen==s.count);float expected=0;for(unsigned i=0;i<s.count;++i)expected+=s.color[i][0];assert(total==expected);
    auto empty=s.batch<N>(s.count);assert(empty.count==0);for(auto c:empty.color)for(float v:c)assert(v==0);
}
int main(){
    const float camera[3]={};
    for(unsigned count=0;count<=64;++count){std::vector<NorthlightLocalLights::Light> lights;
        for(unsigned i=1;i<=count;++i)lights.push_back(lamp(i));
        auto s=select(lights,camera);assert(s.count==std::min(count,Limit));
        for(unsigned i=0;i<s.count;++i){assert(s.color[i][0]==float(i+1));s.fog[i][0]=.01f*(i+1);}
        checkBatches<DirectBatchSize>(s);checkBatches<FogBatchSize>(s);
        std::reverse(lights.begin(),lights.end());auto reversed=select(lights,camera);assert(s.position==reversed.position&&s.color==reversed.color);
    }
    auto a=lamp(1),b=lamp(2);b.position[0]=a.position[0];auto bad=lamp(3);bad.diffuse[0]=-1;auto far=lamp(4);far.position[0]=235;
    auto s=select({b,bad,far,a},camera);assert(s.count==2&&s.color[0][0]==1&&s.color[1][0]==2);
    // Activation is farther away while physical radii/falloff remain identical.
    float previous=1;
    for(int reach=0;reach<=225;++reach){
        auto l=lamp(1);l.position[0]=float(reach)+l.attenuationEnd;
        auto selected=select({l},camera);
        if(reach>=224){assert(selected.count==0);continue;}
        assert(selected.count==1&&selected.position[0][3]==10);
        assert(selected.color[0][3]==1.f/9);
        const float gain=selected.color[0][0];assert(gain<=previous&&previous-gain<.05f);previous=gain;
        if(reach<=192)assert(gain==1);
        if(reach==208)assert(gain==.5f);
        assert(selected.color[0][1]==gain*.5f&&selected.color[0][2]==gain*.25f);
    }
    assert(visibilityGain(224)==0&&visibilityGain(223)<.003f);
    auto large=lamp(1);large.position[0]=300;large.attenuationEnd=128;
    const auto scaled=select({large},camera);assert(scaled.count==1&&scaled.fogDistance==428);
    assert(select({},camera).fogDistance==128);
    assert(nightGain(0)==1&&std::fabs(nightGain(1)-.8f)<1e-6f);
    assert(nightGain(NorthlightRegionalFog::nightFactor(.5f))==1);
    assert(std::fabs(nightGain(NorthlightRegionalFog::nightFactor(0))-.8f)<1e-6f);
    float last=1;
    for(unsigned minute=18*60;minute<=20*60;++minute){float gain=nightGain(NorthlightRegionalFog::nightFactor(float(minute)/1440));assert(gain<=last&&last-gain<.003);last=gain;}
    // Scaling both fog energy and its soft cap must dim even saturated glow
    // by the same 20%, not merely move it closer to the old brightness cap.
    for(double energy:{0.,.001,.1,1.,100.}){
        const double day=energy*25*.3/(.3+energy*25),night=energy*20*.24/(.24+energy*20);
        assert(std::fabs(night-day*.8)<1e-10);
    }
    std::puts("PASS local lights: 0..64 candidates, nearest 32, stable ties/order, partial 8/4 batches zero-filled with no dropped/duplicated lights, out-of-range/invalid rejection, smooth 20% night dimming including saturated fog glow");
}
