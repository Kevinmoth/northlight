#pragma once
#include "world_gi.h"
#include <algorithm>
// Visible sky records are the sole directions of celestial shadows/rays.
// Remaining authored directional energy stays in the original framebuffer;
// it is not reassigned to an invented celestial direction or volumetric beam.
namespace NorthlightCelestialSources {
struct Source {NorthlightGI::Vec3 direction,color;float weight=0;};
struct Result {Source sources[2];float authoredFill=1;};
inline Result resolve(NorthlightGI::Vec3 authoredDirection,NorthlightGI::Vec3 direct,
                      NorthlightGI::Vec3 sun,NorthlightGI::Vec3 moon,float sunWeight,float moonWeight){
    using namespace NorthlightGI;(void)authoredDirection;
    float a=std::clamp(sunWeight,0.f,1.f),b=std::clamp(moonWeight,0.f,1.f-a);
    Result r;r.authoredFill=std::max(0.f,1.f-a-b);
    r.sources[0]={normalized(sun),direct*a,a};
    r.sources[1]={normalized(moon),direct*b,b};return r;
}
}
