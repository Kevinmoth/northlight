// Brute-force containment proof for local_light_scissor.h: every half-res
// pixel where a float reimplementation of LocalDirect/LocalLighting (point in
// the attenuation sphere at a depth in [near,far]) or LocalFog (disc>0 &&
// t1>t0 along the Camera->endpoint ray) can be non-zero lies in the rect.
// Build: c++ -std=c++17 -O2 -Wall -Wextra test_local_light_scissor.cpp
#include "local_light_scissor.h"
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>
using namespace NorthlightLocalLightScissor;
static std::mt19937 rng(0x5c155042);
static float uniform(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
struct Scene {float iv[16]={},proj[3]={},nearZ=.1f,farZ=1000;unsigned w=0,h=0;};
static Scene camera(unsigned w,unsigned h){
    Scene s;s.w=w;s.h=h;
    // Random rotation (Gram-Schmidt), then per-entry noise inside the .004
    // orthonormality tolerance accepted by decodeTerrain.
    float r[3][3];
    for(;;){for(auto& row:r)for(float& x:row)x=uniform(-1,1);
        float n0=std::sqrt(r[0][0]*r[0][0]+r[0][1]*r[0][1]+r[0][2]*r[0][2]);if(n0<.1f)continue;for(float& x:r[0])x/=n0;
        float d=r[1][0]*r[0][0]+r[1][1]*r[0][1]+r[1][2]*r[0][2];for(int k=0;k<3;++k)r[1][k]-=d*r[0][k];
        float n1=std::sqrt(r[1][0]*r[1][0]+r[1][1]*r[1][1]+r[1][2]*r[1][2]);if(n1<.1f)continue;for(float& x:r[1])x/=n1;
        r[2][0]=r[0][1]*r[1][2]-r[0][2]*r[1][1];r[2][1]=r[0][2]*r[1][0]-r[0][0]*r[1][2];r[2][2]=r[0][0]*r[1][1]-r[0][1]*r[1][0];break;}
    for(int i=0;i<3;++i)for(int k=0;k<3;++k)s.iv[4*i+k]=r[i][k]+uniform(-.0006f,.0006f);
    const float span=rng()%3==0?20000.f:500.f;
    for(int k=0;k<3;++k)s.iv[12+k]=uniform(-span,span);s.iv[15]=1;
    const float fy=uniform(.5f,2.5f),aspect=float(w)/h;
    s.proj[0]=(rng()%4==0?-1:1)*fy/aspect;s.proj[1]=(rng()%4==0?-1:1)*fy;s.proj[2]=rng()%2?1.f:-1.f;
    s.nearZ=uniform(.05f,1.5f);s.farZ=uniform(300,3000);return s;
}
// Shader float path for half-res pixel (px,py): world ray direction (z=1).
static void pixelRay(const Scene& s,unsigned px,unsigned py,float* dir){
    const unsigned W2=s.w/2,H2=s.h/2;const float invW=1.f/s.w,invH=1.f/s.h;
    const float u=(px+.5f)/W2,v=(py+.5f)/H2;
    const float ix=std::min(std::max(std::floor(u*s.w),0.f),float(s.w-1)),iy=std::min(std::max(std::floor(v*s.h),0.f),float(s.h-1));
    const float uvx=(ix+.5f)*invW,uvy=(iy+.5f)*invH;
    const float a=((uvx-.5f*invW)*2-1)/s.proj[0],b=((uvy-.5f*invH)*-2+1)/s.proj[1];
    for(int k=0;k<3;++k)dir[k]=a*s.iv[k]+b*s.iv[4+k]+s.proj[2]*s.iv[8+k];
}
static float viewDistance(const Scene& s,float d){return s.nearZ*s.farZ/std::max(s.farZ-d*(s.farZ-s.nearZ),.00001f);}
// LocalDirect/LocalLighting: some depth sample puts the receiver inside the sphere.
static bool surfaceHit(const Scene& s,const float* dir,const float* l){
    double dd=0,dl=0;for(int k=0;k<3;++k){dd+=double(dir[k])*dir[k];dl+=double(dir[k])*(l[k]-double(s.iv[12+k]));}
    const double zc=std::min(std::max(dl/dd,double(viewDistance(s,0))),double(viewDistance(s,.99999f)));
    // Exact test slightly larger than the shader's cutoff (float receiver error).
    double e=0;for(int k=0;k<3;++k){const double p=s.iv[12+k]+zc*dir[k]-l[k];e+=p*p;}
    if(std::sqrt(e)<l[3]*(1+1e-5)+.02)return true;
    // And the literal float formula at depths around the closest approach.
    for(int j=-4;j<=4;++j){const float z=float(zc)*(1+j*.002f);
        float t[3];for(int k=0;k<3;++k)t[k]=l[k]-(z*dir[k]+s.iv[12+k]);
        const float inv=1/std::sqrt(std::max(t[0]*t[0]+t[1]*t[1]+t[2]*t[2],.0025f));
        if(z>=viewDistance(s,0)&&(l[3]-1/inv)>0)return true;}
    return false;
}
// LocalFog literal float path; larger D only grows t1, so scan receivers.
static bool fogHit(const Scene& s,const float* dir,const float* l,float fogDistance){
    const float zs[]={viewDistance(s,0),.5f,1,2,3.5f,3.6f,5,8,16,40,100,400,viewDistance(s,.99999f)};
    for(float z:zs){if(z<viewDistance(s,0))continue;
        float end[3],ray[3];for(int k=0;k<3;++k){end[k]=z*dir[k]+s.iv[12+k];ray[k]=end[k]-s.iv[12+k];}
        const float len2=ray[0]*ray[0]+ray[1]*ray[1]+ray[2]*ray[2];
        const float D=std::min(std::sqrt(len2),fogDistance);const float n=1/std::sqrt(std::max(len2,1e-12f));
        float oc[3],b=0,o2=0;for(int k=0;k<3;++k){ray[k]*=n;oc[k]=s.iv[12+k]-l[k];b+=oc[k]*ray[k];o2+=oc[k]*oc[k];}
        const float h2=std::max(o2-b*b,0.f),disc=l[3]*l[3]-h2,root=std::sqrt(std::max(disc,0.f));
        const float t0=std::max(-b-root,3.5f),t1=std::min(-b+root,D);
        if(disc>0&&t1>t0)return true;
    }
    return false;
}
struct Stats {unsigned long long cases=0,full=0,empty=0,partial=0,pixels=0,covered=0,hits=0;};
static void light(const Scene& s,float* l){
    const float R=rng()%10==0?uniform(40,128):uniform(.12f,30);
    const float* eye=s.iv+12;float off[3];
    switch(rng()%6){
    case 0: for(float& x:off)x=uniform(-.5f,.5f)*R;break; // camera inside
    case 1:{const float lat[2]={uniform(-60,60),uniform(-60,60)},fw=uniform(-R,R)*1.1f; // straddles the eye plane
        for(int k=0;k<3;++k)off[k]=lat[0]*s.iv[k]+lat[1]*s.iv[4+k]+fw*s.proj[2]*s.iv[8+k];}break;
    case 2:{const float fw=-uniform(0,250);for(int k=0;k<3;++k)off[k]=uniform(-80,80)*s.iv[k]+uniform(-80,80)*s.iv[4+k]+fw*s.proj[2]*s.iv[8+k];}break; // behind
    case 3:{const float fw=uniform(0,3)*R+uniform(0,4);for(int k=0;k<3;++k)off[k]=uniform(-2,2)*fw*s.iv[k]+uniform(-2,2)*fw*s.iv[4+k]+fw*s.proj[2]*s.iv[8+k];}break; // near front
    default:{const float fw=uniform(1,260);for(int k=0;k<3;++k)off[k]=uniform(-1.5f,1.5f)*fw/s.proj[0]*s.iv[k]+uniform(-1.5f,1.5f)*fw/s.proj[1]*s.iv[4+k]+fw*s.proj[2]*s.iv[8+k];}break;
    }
    for(int k=0;k<3;++k)l[k]=eye[k]+off[k];l[3]=R;
}
static void check(const Scene& s,const std::array<std::array<float,4>,8>& lights,unsigned count,Stats& st){
    const View v=view(s.iv,s.proj,s.nearZ,3.5f,s.w,s.h);assert(v.valid);
    const Rect r=batchRect(v,lights,count);const unsigned W2=s.w/2,H2=s.h/2;
    ++st.cases;st.pixels+=W2*H2;st.covered+=r.area();
    if(r.area()==0)++st.empty;else if(r.area()==long(W2*H2))++st.full;else ++st.partial;
    for(unsigned py=0;py<H2;++py)for(unsigned px=0;px<W2;++px){
        float dir[3];pixelRay(s,px,py,dir);bool hit=false;
        for(unsigned i=0;i<count&&!hit;++i)hit=surfaceHit(s,dir,lights[i].data())||fogHit(s,dir,lights[i].data(),1e6f);
        if(!hit)continue;++st.hits;
        if(long(px)<r.left||long(px)>=r.right||long(py)<r.top||long(py)>=r.bottom){
            std::fprintf(stderr,"miss px=%u py=%u rect=%ld,%ld,%ld,%ld size=%ux%u\n",px,py,r.left,r.top,r.right,r.bottom,s.w,s.h);
            for(unsigned i=0;i<count;++i)std::fprintf(stderr,"  light %.3f %.3f %.3f r=%.3f\n",lights[i][0],lights[i][1],lights[i][2],lights[i][3]);
            assert(false);
        }
    }
}
static void check(const Scene& s,unsigned count,Stats& st){
    std::array<std::array<float,4>,8> lights{};for(unsigned i=0;i<count;++i)light(s,lights[i].data());
    check(s,lights,count,st);
}
// City-like frame: eye ~2 units above a ground plane, near-horizontal view,
// 32 lamps (radius 3..15) closest-first as NorthlightLocalLightSelection sorts
// them, split into the real 8-light direct and 4-light glow batches.
static void city(Stats& direct,Stats& glow){
    Scene s=camera(321,181);const float yaw=uniform(0,6.2832f),pitch=uniform(-.35f,.2f);
    const float fwd[3]={std::cos(yaw)*std::cos(pitch),std::sin(yaw)*std::cos(pitch),std::sin(pitch)};
    float right[3]={std::sin(yaw),-std::cos(yaw),0};float up[3]={right[1]*fwd[2]-right[2]*fwd[1],right[2]*fwd[0]-right[0]*fwd[2],right[0]*fwd[1]-right[1]*fwd[0]};
    for(int k=0;k<3;++k){s.iv[k]=right[k];s.iv[4+k]=up[k];s.iv[8+k]=fwd[k]*s.proj[2];}
    s.proj[0]=std::fabs(s.proj[0]);s.proj[1]=std::fabs(s.proj[1]);s.nearZ=.2f;
    std::vector<std::array<float,4>> all(32);
    for(auto& l:all){const float a=uniform(0,6.2832f),r=std::sqrt(uniform(0,1))*200;
        l={s.iv[12]+r*std::cos(a),s.iv[13]+r*std::sin(a),s.iv[14]-2+uniform(0,5),uniform(3,15)};}
    std::sort(all.begin(),all.end(),[&](const auto& a,const auto& b){
        const auto score=[&](const auto& l){float d=0;for(int k=0;k<3;++k)d+=(l[k]-s.iv[12+k])*(l[k]-s.iv[12+k]);return std::sqrt(d)-l[3];};
        return score(a)<score(b);});
    for(unsigned first=0;first<32;first+=8){std::array<std::array<float,4>,8> b{};for(unsigned i=0;i<8;++i)b[i]=all[first+i];check(s,b,8,direct);}
    for(unsigned first=0;first<32;first+=4){std::array<std::array<float,4>,8> b{};for(unsigned i=0;i<4;++i)b[i]=all[first+i];check(s,b,4,glow);}
}
static void report(const char* name,const Stats& st){
    std::printf("%s: %llu batches (full=%llu partial=%llu empty=%llu), %llu shaded-candidate pixels all inside; rect coverage %.3f of target\n",
        name,st.cases,st.full,st.partial,st.empty,st.hits,double(st.covered)/double(st.pixels));
}
int main(){
    // Degenerate inputs never clip.
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},proj[3]={1,1,1},badProj[3]={1,0,1};
    const float nearLight[4]={0,0,10,1};
    {Bounds b;add(view(identity,badProj,.1f,3.5f,64,48),nearLight,b);assert(b.full);}
    {Bounds b;add(view(identity,proj,0,3.5f,64,48),nearLight,b);assert(b.full);}
    {const float nan[4]={0,0,NAN,1};Bounds b;add(view(identity,proj,.1f,3.5f,64,48),nan,b);assert(b.full);}
    {const float inside[4]={0,0,.5f,1};Bounds b;const View v=view(identity,proj,.1f,3.5f,64,48);add(v,inside,b);auto r=rect(v,b);assert(r.left==0&&r.top==0&&r.right==32&&r.bottom==24);}
    {const float behind[4]={0,0,-10,1};Bounds b;const View v=view(identity,proj,.1f,3.5f,64,48);add(v,behind,b);assert(b.empty()&&rect(v,b).area()==0);}
    {Bounds b;const View v=view(identity,proj,.1f,3.5f,64,48);add(v,nearLight,b);auto r=rect(v,b);assert(r.area()>0&&r.area()<32*24);}
    // Invalid views fail open: full rect, never empty (NaN basis, fro .3, P0=0, near 0).
    {float nanView[16];std::copy(identity,identity+16,nanView);nanView[5]=NAN;
     float skewed[16];std::copy(identity,identity+16,skewed);skewed[0]=std::sqrt(1.3f);
     const float zeroP[3]={0,1,1};
     const View bad[]={view(nanView,proj,.1f,3.5f,64,48),view(skewed,proj,.1f,3.5f,64,48),view(identity,zeroP,.1f,3.5f,64,48),view(identity,proj,0,3.5f,64,48),view(identity,proj,.1f,0,64,48)};
     for(const View& v:bad){assert(!v.valid);
        for(const Bounds& b:{Bounds{},[&]{Bounds x;add(v,nearLight,x);return x;}()}){const Rect r=rect(v,b);assert(r.left==0&&r.top==0&&r.right==32&&r.bottom==24);}}}
    const unsigned sizes[][2]={{64,48},{97,61},{160,90},{321,181},{256,256},{1728,1117}};
    Stats st;
    for(unsigned iter=0;iter<2400;++iter){
        const auto& size=sizes[iter%5];Scene s=camera(size[0],size[1]);
        check(s,1+rng()%8,st);
    }
    for(unsigned iter=0;iter<24;++iter){Scene s=camera(sizes[5][0],sizes[5][1]);check(s,1+rng()%8,st);}
    report("local-light-scissor adversarial",st);
    Stats direct,glow;for(unsigned iter=0;iter<200;++iter)city(direct,glow);
    report("local-light-scissor city direct(8)",direct);report("local-light-scissor city glow(4)",glow);
    return 0;
}
