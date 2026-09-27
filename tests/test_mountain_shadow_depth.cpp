#include "world_math.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <algorithm>
using namespace NorthlightGI;
static Vec3 project(Vec3 p,const float* m){return {p.x*m[0]+p.y*m[4]+p.z*m[8]+m[12],p.x*m[1]+p.y*m[5]+p.z*m[9]+m[13],p.x*m[2]+p.y*m[6]+p.z*m[10]+m[14]};}
int main(int argc,char**argv){
    assert(argc==2);std::ifstream f(argv[1]);assert(f);std::string line;std::getline(f,line);
    const Vec3 center(-7106.68f,-3873.09f,12.49f),dir=normalized(Vec3(.656f,.656f,.374f));
    unsigned cases=0,oldClipped=0,oldToggled=0,retained=0;
    while(std::getline(f,line)){
        std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);
        Vec3 p,hit;unsigned found,terrain,inOldCrop;float distance,delta;
        assert(bool(row>>p.x>>p.y>>p.z>>found>>distance>>hit.x>>hit.y>>hit.z>>terrain>>delta>>inOldCrop));
        if(!found||!terrain||!inOldCrop)continue;
        ++cases;bool wasClipped=false,wasInside=false;
        for(int step=-40;step<=40;step+=2){
            Vec3 pivot=center+Vec3(float(step),float(step),0);float matrix[16];
            NorthlightWorldMath::shadowMatrix(pivot,dir,192,matrix);
            Vec3 receiver=project(p,matrix),caster=project(hit,matrix);
            const float oldDepth=.5f-dot(hit-pivot,dir)/640;
            wasClipped|=oldDepth<=0;wasInside|=oldDepth>0;
            if(oldDepth<=0)++oldClipped;
            assert(caster.z>0&&caster.z<1&&receiver.z>caster.z);
            assert(std::fabs(receiver.x-caster.x)<.00003f&&std::fabs(receiver.y-caster.y)<.00003f);
            // A caster on the ray shares the receiver's XY; a projection change
            // must not remove it by near-plane clipping while walking.
            if(std::fabs(receiver.x)<.88f&&std::fabs(receiver.y)<.88f)++retained;
        }
        oldToggled+=wasClipped&&wasInside;
    }
    assert(cases>200&&oldClipped>0&&oldToggled>0&&retained>0);
    // Bias/filter depth units scale with the projection, preserving the same
    // world-space offset (near .0512u, far .1024u, fog .2048u).
    for(float factor:{1.f,2.f,4.f})assert(std::fabs(NorthlightWorldMath::ShadowBiasWorld*NorthlightWorldMath::InverseShadowDepth*factor*NorthlightWorldMath::ShadowDepthSpan-.0512f*factor)<1e-7f);
    printf("PASS actual Tanaris terrain: %u cached mountain ray hits; %u old clipped camera samples, %u fixed-world hits toggled with player movement; all retained by new depth span (%u samples inside far-map central XY coverage)\n",cases,oldClipped,oldToggled,retained);
}
