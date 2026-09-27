#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include "celestial_disc.h"

// Viewer-to-sky occlusion, deliberately independent of light-to-world shadows.
namespace NorthlightCelestialTerrain {
constexpr float Near=.5f,Far=16384.f; // encloses the maximum loaded 4096u terrain square
inline void matrix(const NorthlightCelestialDisc::Disc& disc,const float* camera,
                   float extent,unsigned size,float* out){
    std::fill(out,out+16,0.f);
    const float scale=disc.tangentRadius*extent,depth=Far/(Far-Near);
    for(unsigned row=0;row<3;++row){
        out[row*4]=disc.right[row]/scale-disc.direction[row]/size;
        out[row*4+1]=disc.up[row]/scale+disc.direction[row]/size;
        out[row*4+2]=disc.direction[row]*depth;
        out[row*4+3]=disc.direction[row];
    }
    for(unsigned col=0;col<4;++col)for(unsigned row=0;row<3;++row)
        out[12+col]-=camera[row]*out[row*4+col];
    out[14]-=Near*depth;
}
// Homogeneous perspective planes. Never use the affine shadow-map culler here.
struct Frustum {
    double planes[6][4]={};bool valid=true;
    explicit Frustum(const float* m){
        for(unsigned k=0;k<16;++k)if(!std::isfinite(m[k]))valid=false;
        for(unsigned plane=0;plane<6;++plane)for(unsigned row=0;row<4;++row){
            const float* r=m+row*4;
            planes[plane][row]=plane<4?double(r[3])+(plane%2?-1.:1.)*r[plane/2]:
                              plane==4?double(r[2]):double(r[3])-r[2];
        }
    }
    template<class V> bool reject(V lo,V hi)const{
        if(!valid)return false;
        const float low[]={lo.x,lo.y,lo.z},high[]={hi.x,hi.y,hi.z};
        for(unsigned k=0;k<3;++k)if(!std::isfinite(low[k])||!std::isfinite(high[k])||low[k]>high[k])return false;
        for(const auto& p:planes){
            double upper=p[3],magnitude=std::fabs(upper);
            for(unsigned row=0;row<3;++row){
                const double a=low[row]*p[row],b=high[row]*p[row];
                upper+=std::max(a,b);magnitude+=std::max(std::fabs(a),std::fabs(b));
            }
            if(upper<-(.001+magnitude*16*std::numeric_limits<float>::epsilon()))return true;
        }
        return false;
    }
};
template<class V> bool reject(V lo,V hi,const float* m){return Frustum(m).reject(lo,hi);}
}
