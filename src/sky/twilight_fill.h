#pragma once
#include <algorithm>
#include <cmath>

// Small surface-ambient bridge across sunset, driven by the same smoothed
// renderer-owned source directions as light/shadows. Does not change sources,
// probes, sky, fog or their cache signatures.
namespace NorthlightTwilightFill {
inline constexpr float MaximumGain=.10f;
inline constexpr float SunStartZ=.1045284633f; // sin(6 degrees)
inline constexpr float MoonEndZ=.1391731010f; // sin(8 degrees)
inline float smooth(float v){v=std::clamp(v,0.f,1.f);return v*v*(3-2*v);}
inline float gain(bool valid,double day,float sunZ,float moonZ){
    // Evening only. The ordinary orbit already makes this zero well before
    // midnight/noon, so the half-day guard introduces no visible time seam.
    if(!valid||!std::isfinite(day)||day<.5||day>=1||
       !std::isfinite(sunZ)||!std::isfinite(moonZ)||
       std::fabs(sunZ)>1||std::fabs(moonZ)>1)return 0;
    return MaximumGain*(1-smooth(sunZ/SunStartZ))*(1-smooth(moonZ/MoonEndZ));
}
}
