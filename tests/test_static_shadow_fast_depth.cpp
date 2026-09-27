// Portable mathematical gate for the optional no-DEPTH-output shadow path.
#include "shadow_bounds.h"
#include <cassert>
#include <cstdint>
#include <iostream>
using NorthlightGI::Vec3;
static uint32_t rng=0x329675ab;
float random(float lo,float hi){rng=rng*1664525u+1013904223u;return lo+(hi-lo)*(float(rng>>8)/16777216.f);}
float depth(Vec3 p,const float* m){volatile float x=p.x*m[2],y=p.y*m[6],z=p.z*m[10];volatile float a=x+y,b=a+z;return b+m[14];}
int main(){float m[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 assert(NorthlightShadowBounds::depthFullyInside({-1,-1,.1f},{1,1,.9f},m));
 for(float z:{-1.f,0.f,.00005f,.99995f,1.f,2.f})assert(!NorthlightShadowBounds::depthFullyInside({0,0,z},{0,0,z},m));
 assert(!NorthlightShadowBounds::depthFullyInside({0,0,1},{0,0,0},m));
 m[3]=.1f;assert(!NorthlightShadowBounds::depthFullyInside({0,0,.5f},{0,0,.5f},m));m[3]=0;
 m[14]=NAN;assert(!NorthlightShadowBounds::depthFullyInside({0,0,.5f},{0,0,.5f},m));m[14]=0;
 unsigned accepted=0,rejected=0;
 // Include the low-angle sun and 17000-unit coordinate cancellation from the
 // actual world. Check both separately rounded and fused affine arithmetic.
 for(unsigned trial=0;trial<50000;++trial){
  const float elevation=(trial%3==0?2.f:random(2,85))*.017453292519943295f,azimuth=random(-3.14159f,3.14159f);
  m[2]=-std::cos(elevation)*std::cos(azimuth)/1280.f;m[6]=-std::cos(elevation)*std::sin(azimuth)/1280.f;m[10]=-std::sin(elevation)/1280.f;
  Vec3 c{random(-17000,17000),random(-17000,17000),random(-3000,3000)},e{random(0,200),random(0,200),random(0,1200)};
  Vec3 low{c.x-e.x,c.y-e.y,c.z-e.z},high{c.x+e.x,c.y+e.y,c.z+e.z};
  m[14]=random(-.15f,1.15f)-(c.x*m[2]+c.y*m[6]+c.z*m[10]);
  if(!NorthlightShadowBounds::depthFullyInside(low,high,m)){++rejected;continue;}++accepted;
  float z[8];for(unsigned j=0;j<8;++j){Vec3 p{j&1?high.x:low.x,j&2?high.y:low.y,j&4?high.z:low.z};z[j]=depth(p,m);float fused=std::fma(p.x,m[2],std::fma(p.y,m[6],std::fma(p.z,m[10],m[14])));assert(z[j]>0&&z[j]<1&&fused>0&&fused<1);}
  for(unsigned j=0;j<20;++j){float a=random(0,1),b=random(0,1-a),cweight=1-a-b;
   float raw=a*z[j%8]+b*z[(j+1)%8]+cweight*z[(j+3)%8];
   // .95: raster vertices saturate, PS rejects raw>1 and writes max(raw,0).
   // Fast: same raster interpolation, same COLOR, fixed-function depth.
   float oldFragmentDepth=std::max(raw,0.f),fastDepth=raw;assert(raw<=1&&oldFragmentDepth==fastDepth);
   for(float alpha:{0.f,.1f,.5f,1.f})for(float cutoff:{0.f,.5f,1.f}){
    const bool oldKeep=alpha-cutoff>=0&&1-raw>=0,newKeep=alpha-cutoff>=0;assert(oldKeep==newKeep);
    if(cutoff<=0)assert(oldKeep); // opaque shortcut is valid even at alpha=0
   }
  }
 }
 assert(accepted>10000&&rejected>10000);
 std::cout<<"fast directional depth: "<<accepted<<" certified boxes, "<<rejected<<" conservative fallbacks; both-plane/2-degree/large-world tests passed\n";
}
