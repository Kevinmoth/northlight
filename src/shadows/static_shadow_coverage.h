#pragma once
#include "world_gi.h"
#include <cmath>
namespace StaticShadow {
// A replay may be omitted only if its ENTIRE proven geometry was inside the
// cached map's original clipping volume. Intersection alone is insufficient:
// cacheOffset can shift depth while an earlier caster was near-plane clipped.
inline bool containsBounds(const float* matrix,NorthlightGI::Vec3 low,NorthlightGI::Vec3 high){
    if(low.x>high.x||low.y>high.y||low.z>high.z)return false;
    for(unsigned c=0;c<8;++c){
        const float x=c&1?high.x:low.x,y=c&2?high.y:low.y,z=c&4?high.z:low.z;
        float q[4];for(unsigned i=0;i<4;++i)q[i]=x*matrix[i]+y*matrix[4+i]+z*matrix[8+i]+matrix[12+i];
        for(float v:q)if(!std::isfinite(v))return false;
        if(q[3]<=0||q[0]<-q[3]||q[0]>q[3]||q[1]<-q[3]||q[1]>q[3]||q[2]<0||q[2]>q[3])return false;
    }
    return true;
}
}
