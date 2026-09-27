#include "twilight_fill.h"
#include "celestial_time_warp.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace NorthlightTwilightFill;
using namespace NorthlightCelestialOrbit;
static float at(double seconds){auto o=evaluate(seconds/kDaySeconds);return gain(o.valid,seconds/kDaySeconds,o.sun.direction[2],o.moon.direction[2]);}
int main(){
    assert(gain(false,.85,0,0)==0);
    for(double day:{-1.,0.,.25,1.,std::numeric_limits<double>::quiet_NaN()})assert(gain(true,day,0,0)==0);
    assert(gain(true,.85,std::numeric_limits<float>::infinity(),0)==0);
    assert(gain(true,.85,0,std::numeric_limits<float>::quiet_NaN())==0);
    assert(gain(true,.85,1.01f,0)==0);
    assert(gain(true,.85,SunStartZ,-1)==0);
    assert(gain(true,.85,-1,MoonEndZ)==0);
    assert(gain(true,.85,0,0)==MaximumGain);
    float previous=at(0),maxStep=0;unsigned nonzero=0;double first=0,last=0;
    for(unsigned second=1;second<86400;++second){
        float g=at(second);assert(g>=0&&g<=MaximumGain);
        if(second<12*3600)assert(g==0); // no new dawn/night exposure policy
        if(g>0){if(!nonzero)first=second;last=second;++nonzero;}
        maxStep=std::max(maxStep,std::fabs(g-previous));previous=g;
    }
    assert(nonzero>0&&maxStep<.0001f);
    assert(at(0)==0&&at(86399)==0&&at(12*3600)==0);
    assert(std::fabs(at(kSunsetSeconds)-MaximumGain)<1e-6f);
    previous=0;
    for(double t=19*3600;t<=kSunsetSeconds;t+=.1){float g=at(t);assert(g+1e-7f>=previous);previous=g;}
    previous=MaximumGain;
    for(double t=kSunsetSeconds;t<24*3600;t+=.1){float g=at(t);assert(g<=previous+1e-7f);previous=g;}
    // Same held-clock smoothing as source light, at multiple frame rates.
    for(unsigned fps:{30u,60u,144u}){
        LightMotion filter;float lastGain=0;bool started=false;
        for(unsigned f=0;f<fps*180;++f){double day=(kSunsetSeconds-60+double(f/fps))/kDaySeconds;
            auto o=filter.update(evaluate(day),day,uint32_t(1000.*f/fps));
            float g=gain(true,day,o.sun.direction[2],o.moon.direction[2]);
            if(started)assert(std::fabs(g-lastGain)<.0001f);lastGain=g;started=true;
        }
    }
    // Shader-equation reference: boost effective ambient, not direct light;
    // black stays black and missing probes keep the native color ratio.
    for(float ambient:{0.f,.01f,.2f,1.f})for(float correction:{-.5f,-.01f,0.f,.03f,.5f}){
        const float effective=std::max(ambient+correction,0.f);
        const float boosted=correction+effective*MaximumGain;
        assert(boosted>=correction&&boosted<=correction+effective*.100001f);
        if(effective==0)assert(boosted==correction);
    }
    std::printf("PASS twilight gain max=%.3f first=%.0f last=%.0f gameSeconds maxStep=%.8f; full-day, continuity, invalid inputs, smoothing, surface-only reference\n",MaximumGain,first,last,maxStep);
}
