#include "celestial_terrain.h"
#include <cassert>
#include <cstdio>
#include <random>
struct V{float x,y,z;};
static void project(V p,const float* m,double* out){
    for(unsigned c=0;c<4;++c)out[c]=double(p.x)*m[c]+double(p.y)*m[4+c]+double(p.z)*m[8+c]+m[12+c];
}
static bool cornerReject(V lo,V hi,const float* m){
    bool outside[6]={true,true,true,true,true,true};
    for(int bits=0;bits<8;++bits){double q[4];project({bits&1?hi.x:lo.x,bits&2?hi.y:lo.y,bits&4?hi.z:lo.z},m,q);
        const double planes[]={q[3]+q[0],q[3]-q[0],q[3]+q[1],q[3]-q[1],q[2],q[3]-q[2]};
        for(unsigned k=0;k<6;++k)outside[k]&=planes[k]<0;
    }
    return std::any_of(outside,outside+6,[](bool v){return v;});
}
int main(){
    using namespace NorthlightCelestialTerrain;
    std::mt19937 random(114);std::uniform_real_distribution<float> point(-6000,6000),extent(0,120);
    unsigned cases=0,retained=0;
    const float camera[]={-9000,-3500,40},color[]={1,1,1};
    for(float elevation:{.2f,2.f,5.f,15.f,43.f,85.f})for(float halo:{2.5f,10.f}){
        const float e=elevation*3.14159265f/180;float direction[]={std::cos(e)*.70710678f,std::cos(e)*.70710678f,std::sin(e)};
        NorthlightCelestialDisc::Disc disc;assert(NorthlightCelestialDisc::prepare(direction,color,1,.02f,disc));
        float m[16];matrix(disc,camera,halo,1024,m);Frustum frustum(m);
        // Distant viewer rays map to the SAME celestial pixel at every depth.
        // Include mountains beyond both the old 928 square and native far clip.
        for(float distance:{16.f,900.f,1500.f,4096.f,5800.f})for(float x:{-.95f,0.f,.95f})for(float y:{-.95f,0.f,.95f}){
            float p[3];for(int k=0;k<3;++k)p[k]=camera[k]+distance*(disc.direction[k]+disc.tangentRadius*(disc.right[k]*x+disc.up[k]*y));
            double q[4];project({p[0],p[1],p[2]},m,q);
            // D3D9's -half-texel clip translation + texture-center addressing.
            const double u=q[0]/q[3]*.5+.5+.5/1024,v=-q[1]/q[3]*.5+.5+.5/1024;
            const double tolerance=distance<100?.001:.0001; // <=1 near-mask texel; <.11 texel for distant mountains
            assert(std::fabs(u-(.5+.5*x/halo))<tolerance&&std::fabs(v-(.5-.5*y/halo))<tolerance);
            assert(q[2]>=0&&q[2]<=q[3]);
            assert(!frustum.reject(V{p[0]-.1f,p[1]-.1f,p[2]-.1f},V{p[0]+.1f,p[1]+.1f,p[2]+.1f}));++cases;
        }
        V back{camera[0]-100*direction[0],camera[1]-100*direction[1],camera[2]-100*direction[2]};assert(frustum.reject(back,back));
        V beyond{camera[0]+20000*direction[0],camera[1]+20000*direction[1],camera[2]+20000*direction[2]};assert(frustum.reject(beyond,beyond));
        for(unsigned i=0;i<20000;++i){
            V lo{camera[0]+point(random),camera[1]+point(random),camera[2]+point(random)},hi{lo.x+extent(random),lo.y+extent(random),lo.z+extent(random)};
            const bool rejected=frustum.reject(lo,hi);
            if(rejected)assert(cornerReject(lo,hi,m));else ++retained;
            ++cases;
        }
        // A near-plane-crossing box cannot disappear merely because some of
        // its corners are behind the camera. Unknown inputs fail open.
        assert(!frustum.reject(V{camera[0]-2,camera[1]-2,camera[2]-2},V{camera[0]+2,camera[1]+2,camera[2]+2}));
        float invalid[16];std::copy(m,m+16,invalid);invalid[0]=NAN;assert(!Frustum(invalid).reject(back,back));
    }
    assert(retained>0);
    std::printf("PASS celestial terrain: %u perspective/ray/culling cases, %u retained random boxes; far mountains, behind camera, near-plane crossing and invalid input checked\n",cases,retained);
}
