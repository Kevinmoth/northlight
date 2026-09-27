#include "world_context.h"
#include <cassert>
#include <limits>
#include <cstdio>
using namespace NorthlightWorldContext;

static void testHandedness(float handedness) {
    float view[]={0,1,0,0,-1,0,0,0,0,0,handedness,0,30,-20,-40*handedness,1};
    float light[]={0,0,handedness,0,.1f,.2f,.3f,0,.8f,.7f,.6f,0};
    TerrainContext c;assert(decodeTerrain(view,light,c));
    float camera[]={20,30,40};assert(cameraAgrees(c,camera));
    assert(c.lightDirection[2]==1);
    for(int i=0;i<4;++i)for(int j=0;j<4;++j) {
        float value=0;
        for(int k=0;k<4;++k)value+=view[i*4+k]*c.inverseView[k*4+j];
        assert(std::fabs(value-(i==j?1.f:0.f))<.0001f);
    }
    // Camera-space projection: depth distance is positive for either sign of C.
    const float worldPoint[]={22,31,48,1};float vp[4]={};
    for(int j=0;j<4;++j)for(int k=0;k<4;++k)vp[j]+=worldPoint[k]*view[4*k+j];
    const float projectionX=1.2f,projectionY=1.8f;
    const float distance=vp[2]*handedness;
    const float ndcX=vp[0]*projectionX/distance,ndcY=vp[1]*projectionY/distance;
    float reconstructed[]={ndcX*distance/projectionX,ndcY*distance/projectionY,distance*handedness,1};
    for(int j=0;j<4;++j) {
        float value=0;
        for(int k=0;k<4;++k)value+=reconstructed[k]*c.inverseView[4*k+j];
        assert(std::fabs(value-worldPoint[j])<.0001f);
    }
    camera[0]+=1;assert(!cameraAgrees(c,camera));
    view[0]=2;assert(!decodeTerrain(view,light,c));
    view[0]=0;view[7]=.1f;assert(!decodeTerrain(view,light,c));
    view[7]=0;light[2]=0;assert(!decodeTerrain(view,light,c));
    light[2]=handedness;view[12]=std::numeric_limits<float>::quiet_NaN();
    assert(!decodeTerrain(view,light,c));
}
int main() {
    testHandedness(1);testHandedness(-1);
    std::puts("World context: RH/LH affine inverse, depth reconstruction and rejection checks passed");
}
