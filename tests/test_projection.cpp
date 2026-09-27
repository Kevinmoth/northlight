#include "projection.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>
int main() {
    for (float handedness:{-1.f,1.f}) {
        float n=.3f,f=800.f;
        float p[]={1.2f,0,0,0,0,1.8f,0,0,0,0,handedness*f/(f-n),handedness,0,0,-n*f/(f-n),0};
        Projection q;
        assert(decodeProjection(p,q));
        assert(std::fabs(q.nearZ-n)<1e-5f);
        assert(std::fabs(q.farZ-f)/f<.001f);
        float columns[16],rows[16];
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)columns[4*r+c]=p[4*c+r];
        Projection wmo;
        assert(decodeColumnProjection(columns,wmo,rows));
        assert(wmo.nearZ==q.nearZ&&wmo.farZ==q.farZ&&wmo.scaleX==q.scaleX&&wmo.scaleY==q.scaleY);
        for(unsigned i=0;i<16;++i)assert(rows[i]==p[i]);
        columns[15]=1;rows[0]=1234;
        assert(!decodeColumnProjection(columns,wmo,rows)&&rows[0]==1234);
        for (float z:{.3f,1.f,20.f,200.f,799.f}) {
            float depth=(p[10]*(handedness*z)+p[14])/z;
            float restored=q.nearZ*q.farZ/(q.farZ-depth*(q.farZ-q.nearZ));
            assert(std::fabs(restored-z)/z<.001f);
            for(float maxDepth:{.94f,.998046875f,1.f}) {
                for(float minDepth:{0.f,.1f}) {
                    float raw=minDepth+depth*(maxDepth-minDepth);
                    float normalized=(raw-minDepth)/(maxDepth-minDepth);
                    float recovered=q.nearZ*q.farZ/(q.farZ-normalized*(q.farZ-q.nearZ));
                    assert(std::fabs(recovered-z)/z<.001f);
                }
            }
        }
        p[15]=1;assert(!decodeProjection(p,q));p[15]=0;
        p[8]=.1f;assert(!decodeProjection(p,q));p[8]=0;
        p[10]=.5f*handedness;assert(!decodeProjection(p,q));
        p[10]=std::numeric_limits<float>::quiet_NaN();assert(!decodeProjection(p,q));
    }
    puts("Projection reconstruction: LH/RH, depth round trips including WoW .94/.998046875 viewport ranges, orthographic/off-axis/reversed/NaN rejection PASS");
}
