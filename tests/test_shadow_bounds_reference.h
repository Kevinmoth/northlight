// Frozen 0.3.105 corner oracle; deliberately independent of production interval bounds.
#pragma once
#include "world_gi.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ReferenceShadowBounds {
// Conservative culling for the renderer's affine world->light clip matrices:
// row-vector convention, X/Y in [-1,1], Z in [0,1], W identically one.
// Never camera-frustum cull shadow casters: offscreen geometry still casts light.
// Returns false (draw) for invalid bounds/matrices or unsupported perspective.
inline bool clipReject(NorthlightGI::Vec3 low,NorthlightGI::Vec3 high,
                       const float* matrix,float epsilon=1e-4f,bool retainUpstream=false) {
    if(!matrix||!std::isfinite(epsilon)||epsilon<0)return false;
    const float lo[]={low.x,low.y,low.z},hi[]={high.x,high.y,high.z};
    for(unsigned axis=0;axis<3;++axis)
        if(!std::isfinite(lo[axis])||!std::isfinite(hi[axis])||lo[axis]>hi[axis])return false;
    for(unsigned i=0;i<16;++i)if(!std::isfinite(matrix[i]))return false;
    if(matrix[3]!=0||matrix[7]!=0||matrix[11]!=0||matrix[15]!=1)return false;
    const double infinity=std::numeric_limits<double>::infinity();
    double minimum[]={infinity,infinity,infinity},maximum[]={-infinity,-infinity,-infinity};
    for(unsigned corner=0;corner<8;++corner) {
        const double p[]={double(corner&1?hi[0]:lo[0]),double(corner&2?hi[1]:lo[1]),double(corner&4?hi[2]:lo[2])};
        for(unsigned axis=0;axis<3;++axis) {
            double value=matrix[12+axis],magnitude=std::fabs(value);
            for(unsigned row=0;row<3;++row){const double term=p[row]*matrix[row*4+axis];value+=term;magnitude+=std::fabs(term);}
            // Expand for the GPU's float multiply/add roundoff, including large
            // world-coordinate translation cancellation. Double CPU arithmetic
            // alone would not conservatively bound the shader's float result.
            const double margin=double(epsilon)+magnitude*(8.0*std::numeric_limits<float>::epsilon());
            minimum[axis]=std::min(minimum[axis],value-margin);
            maximum[axis]=std::max(maximum[axis],value+margin);
        }
    }
    return maximum[0]<-1||minimum[0]>1||maximum[1]<-1||minimum[1]>1||(!retainUpstream&&maximum[2]<0)||minimum[2]>1;
}
// Fast depth output is permitted only when every corner stays strictly inside
// BOTH depth planes, including conservative CPU/GPU float roundoff. Unknown or
// perspective inputs use the raw-depth fragment path. No XY/selection change.
inline bool depthFullyInside(NorthlightGI::Vec3 low,NorthlightGI::Vec3 high,
                             const float* matrix,float epsilon=1e-4f){
    if(!matrix||!std::isfinite(epsilon)||epsilon<0)return false;
    const float lo[]={low.x,low.y,low.z},hi[]={high.x,high.y,high.z};
    for(unsigned axis=0;axis<3;++axis)if(!std::isfinite(lo[axis])||!std::isfinite(hi[axis])||lo[axis]>hi[axis])return false;
    for(unsigned i=0;i<16;++i)if(!std::isfinite(matrix[i]))return false;
    if(matrix[3]!=0||matrix[7]!=0||matrix[11]!=0||matrix[15]!=1)return false;
    double minimum=matrix[14],maximum=minimum,magnitude=std::fabs(minimum);
    for(unsigned axis=0;axis<3;++axis){
        const double coefficient=matrix[axis*4+2],a=double(lo[axis])*coefficient,b=double(hi[axis])*coefficient;
        minimum+=std::min(a,b);maximum+=std::max(a,b);magnitude+=std::max(std::fabs(a),std::fabs(b));
    }
    // 32 eps also covers the preceding local->world float transform and
    // its AABB center/extent computation, including large-coordinate
    // cancellation. Conservative rejection costs speed, never coverage.
    const double margin=double(epsilon)+magnitude*(32.0*std::numeric_limits<float>::epsilon());
    if(!(minimum-margin>0&&maximum+margin<1))return false;
    return true;
}
// Only for the persistent directional pass whose shader clamps upstream
// depth. Receivers inside the volume can still be occluded from before z=0.
inline bool directionalClipReject(NorthlightGI::Vec3 low,NorthlightGI::Vec3 high,const float* matrix){
    return clipReject(low,high,matrix,1e-4f,true);
}
} // namespace ReferenceShadowBounds
