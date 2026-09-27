#pragma once
#include <cmath>
#include <initializer_list>
// Perspective projection alone does not identify a world caster: sky models
// reuse the model shaders/projection but render in a separate depth interval.
namespace NorthlightWorldDrawDomain {
inline bool accepts(bool projectionValid,bool depthEnabled,float worldMin,float worldMax,float drawMin,float drawMax){
    if(!projectionValid||!depthEnabled)return false;
    for(float value:{worldMin,worldMax,drawMin,drawMax})if(!std::isfinite(value))return false;
    if(worldMin<0||worldMax>1||worldMax<=worldMin||drawMin<0||drawMax>1||drawMax<=drawMin)return false;
    return std::fabs(drawMin-worldMin)<=1e-6f&&std::fabs(drawMax-worldMax)<=1e-6f;
}
}
