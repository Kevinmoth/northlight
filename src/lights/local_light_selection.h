#pragma once
#include "world_local_lights.h"
#include <array>
#include <cmath>

namespace NorthlightLocalLightSelection {
// Limit: the default selection (0.3.136..0.3.139). Capacity: the most a user may select
// (northlight-quality.ini LocalLightLimit up to 64); extra lights only add 8-/4-light batches.
inline constexpr unsigned Limit=32,Capacity=64,DirectBatchSize=8,FogBatchSize=4;
static_assert(Capacity%DirectBatchSize==0&&Capacity%FogBatchSize==0&&Limit<=Capacity,"whole batches");
// Viewer distance to the influence sphere, not the bulb. Keep nearby energy
// unchanged; introduce distant lights with a smooth ramp instead of a pop.
inline constexpr float VisibilityFull=192.f,VisibilityEnd=224.f;
inline float visibilityGain(float reach){
    const float t=std::clamp((VisibilityEnd-reach)/(VisibilityEnd-VisibilityFull),0.f,1.f);
    return t*t*(3.f-2.f*t);
}
using Constant=std::array<float,4>;
inline float nightGain(float night){return 1.f-.20f*std::clamp(night,0.f,1.f);}
// Lamps in the sun (30%, no key): LocalDirect lamp light keeps SunlitGain where the
// sun shines directly on the receiver; sun shadow, interiors and night are unchanged. sunlitCut
// is the share removed in full sun (c52.z), faded with the sun's source weight (its elevation
// visibility), and 0 unless the first lighting pass is an active sun with real shadows, so the
// moon's visibility (a moon-first pass) never dims lamps. Lamp fog and the point lamp are unchanged.
inline constexpr float SunlitGain=.3f;
inline float sunlitCut(float sunWeight,bool sunVisibilityInBaseline){
    return sunVisibilityInBaseline&&std::isfinite(sunWeight)?(1-SunlitGain)*std::clamp(sunWeight,0.f,1.f):0.f;}
template<unsigned N> struct Batch {
    std::array<Constant,N> position{},color{},fog{};
    unsigned count=0;
};
struct Selection {
    std::array<Constant,Capacity> position{},color{},fog{};
    unsigned count=0;float nearest=0,fogDistance=128.f;
    template<unsigned N> Batch<N> batch(unsigned first)const{
        Batch<N> b;
        for(unsigned i=0;i<N&&first+i<count;++i){
            b.position[i]=position[first+i];b.color[i]=color[first+i];b.fog[i]=fog[first+i];++b.count;
        }
        return b; // All unused shader slots must be zero, including partial batches.
    }
};
// `limit` (northlight-quality.ini LocalLightLimit, clamped to Capacity) keeps the closest lights;
// Limit (32) selects exactly what 0.3.136..0.3.139 did.
inline Selection select(const std::vector<NorthlightLocalLights::Light>& lights,const float* camera,unsigned limit=Limit){
    limit=std::min(limit,Capacity);
    struct Pick {float score;const NorthlightLocalLights::Light* light;};
    const auto before=[](const Pick& a,const Pick& b){return a.score!=b.score?a.score<b.score:a.light->sourceId<b.light->sourceId;};
    std::array<Pick,Capacity> picks{};unsigned count=0;
    for(const auto& l:lights){
        if(!NorthlightLocalLights::valid(l)||l.attenuationEnd<=.11f)continue;
        float d2=0;for(unsigned i=0;i<3;++i){const float d=l.position[i]-camera[i];d2+=d*d;}
        Pick p{std::sqrt(d2)-l.attenuationEnd,&l};if(p.score>=VisibilityEnd)continue;
        if(count<limit)picks[count++]=p;
        else if(limit){auto worst=std::max_element(picks.begin(),picks.begin()+limit,before);if(before(p,*worst))*worst=p;}
    }
    // Deterministic closest-first batches, including equal-distance ties.
    std::sort(picks.begin(),picks.begin()+count,before);
    Selection out;out.count=count;if(count)out.nearest=picks[0].score;
    for(unsigned i=0;i<count;++i){const auto& l=*picks[i].light;
        out.position[i]={l.position[0],l.position[1],l.position[2],l.attenuationEnd};
        const float gain=visibilityGain(picks[i].score);
        // Complete the selected influence sphere even for scaled bonfires.
        out.fogDistance=std::max(out.fogDistance,picks[i].score+2*l.attenuationEnd);
        out.color[i]={l.diffuse[0]*gain,l.diffuse[1]*gain,l.diffuse[2]*gain,1.f/std::max(l.attenuationEnd-std::min(l.attenuationStart,l.attenuationEnd*.9f),.05f)};
    }
    return out;
}
} // namespace NorthlightLocalLightSelection
