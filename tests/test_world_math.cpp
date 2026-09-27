#include "world_math.h"
#include "world_context.h"
#include <cassert>
#include <cstdio>
using NorthlightGI::Vec3;
void transform(const float* p,const float* m,float* out){for(int j=0;j<4;++j){out[j]=0;for(int i=0;i<4;++i)out[j]+=p[i]*m[i*4+j];}}
int main(){
    Vec3 center(-10432,145,87),sun=NorthlightGI::normalized(Vec3(.3f,-.4f,.7f));float light[16];
    NorthlightWorldMath::shadowMatrix(center,sun,48,light);
    float p[]={center.x,center.y,center.z,1},q[4];transform(p,light,q);
    assert(std::fabs(q[0])<2.f/1024+.0001f&&std::fabs(q[1])<2.f/1024+.0001f&&std::fabs(q[2]-.5f)<.0001f&&q[3]==1);
    for(int j=0;j<3;++j)p[j]+=(&sun.x)[j]*64;transform(p,light,q);assert(std::fabs(q[2]-(.5f-64/NorthlightWorldMath::ShadowDepthSpan))<.0001f);
    float view[]={0,1,0,0, -1,0,0,0, 0,0,1,0, -4,-8,-9,1};float lighting[]={0,0,1,0,.2f,.2f,.2f,0,1,1,1,0};NorthlightWorldContext::TerrainContext context;
    assert(NorthlightWorldContext::decodeTerrain(view,lighting,context));float rows[16];NorthlightWorldMath::replayProjection(context.inverseView,light,rows);
    for(int i=0;i<128;++i){float world[]={center.x+i*.15f,center.y-i*.3f,center.z+i*.05f,1},vp[4],expected[4];transform(world,view,vp);transform(world,light,expected);
        for(int r=0;r<4;++r){float actual=0;for(int k=0;k<4;++k)actual+=rows[r*4+k]*vp[k];assert(std::fabs(actual-expected[r])<.0001f);}}
    // Each PCF tap must compare against the SAME receiver plane at that tap.
    // A fixed depth causes diagonal stripes even with an exactly matching mesh.
    for(float radius:{48.f,192.f})for(Vec3 normal:{Vec3(0,0,1),NorthlightGI::normalized(Vec3(.2f,.7f,.6f)),NorthlightGI::normalized(Vec3(-.4f,.1f,.9f))}){
        NorthlightWorldMath::shadowMatrix(center,sun,radius,light);
        float ndl=NorthlightGI::dot(normal,sun);assert(std::fabs(ndl)>.15f);
        Vec3 axisX(light[0]*radius,light[4]*radius,light[8]*radius),axisY(light[1]*radius,light[5]*radius,light[9]*radius);
        float gradientX=2*radius*NorthlightGI::dot(normal,axisX)/(NorthlightWorldMath::ShadowDepthSpan*ndl);
        float gradientY=-2*radius*NorthlightGI::dot(normal,axisY)/(NorthlightWorldMath::ShadowDepthSpan*ndl);
        for(int ix=-2;ix<=2;++ix)for(int iy=-2;iy<=2;++iy){
            float du=ix/1024.f,dv=iy/1024.f;
            Vec3 delta=axisX*(2*radius*du)-axisY*(2*radius*dv);
            delta=delta-sun*(NorthlightGI::dot(normal,delta)/ndl);
            float actualDepth=-NorthlightGI::dot(delta,sun)/NorthlightWorldMath::ShadowDepthSpan;
            assert(std::fabs(actualDepth-gradientX*du-gradientY*dv)<1e-7f);
        }
    }
    // Cached static shadow maps: an integer texel shift of the light-space origin
    // and any shift along the light give exact lp.xy/lp.z deltas, and working
    // texel (t,r) lands on cached texel (t+margin+offX, r+margin-offY).
    for(float radius:{48.f,192.f})for(Vec3 dir:{sun,NorthlightGI::normalized(Vec3(-.5f,.2f,.6f)),Vec3(0,0,1),NorthlightGI::normalized(Vec3(.1f,.9f,.3f))}){
        Vec3 base(-10432.37f,145.61f,87.2f);
        auto a=NorthlightWorldMath::shadowFrame(base,dir,radius);
        for(int kx:{-128,-7,0,3,128})for(int ky:{-100,0,5,128}){
            Vec3 b=base+a.x*(kx*a.texel)+a.y*(ky*a.texel)+a.z*7.25f;
            auto bf=NorthlightWorldMath::shadowFrame(b,dir,radius);
            long offX=0,offY=0;float dz=0;
            assert(NorthlightWorldMath::cacheOffset(bf,a,offX,offY,dz));
            // dz carries the float rounding of two ~1e4-magnitude dot products (~1e-6),
            // the same rounding the single-pass matrix m[14] already has.
            assert(offX==kx&&offY==ky&&std::fabs(dz-7.25f/NorthlightWorldMath::ShadowDepthSpan)<1e-5f);
            float ma[16],mb[16],mc[16];
            NorthlightWorldMath::shadowMatrixFrom(a,radius,ma);NorthlightWorldMath::shadowMatrixFrom(bf,radius,mb);NorthlightWorldMath::shadowMatrixFrom(a,radius*1280/1024.f,mc);
            for(int i=0;i<24;++i){
                float world[]={base.x+i*1.7f-11,base.y-i*.9f+6,base.z+i*.4f,1},qa[4],qb[4],qc[4];
                transform(world,ma,qa);transform(world,mb,qb);transform(world,mc,qc);
                assert(std::fabs((qa[0]-qb[0])-kx*2.f/1024)<1e-5f&&std::fabs((qa[1]-qb[1])-ky*2.f/1024)<1e-5f&&std::fabs((qb[2]-qa[2])-dz)<1e-6f);
                double tW=(qb[0]*.5+.5)*1024,rW=(.5-qb[1]*.5)*1024,tC=(qc[0]*.5+.5)*1280,rC=(.5-qc[1]*.5)*1280;
                auto nearEdge=[](double v){double f=v-std::floor(v);return f<.01||f>.99;};
                if(nearEdge(tW)||nearEdge(rW)||nearEdge(tC)||nearEdge(rC))continue;
                assert(long(std::floor(tC))==long(std::floor(tW))+128+offX&&long(std::floor(rC))==long(std::floor(rW))+128-offY);
            }
        }
        Vec3 q=NorthlightWorldMath::quantizeDirection(dir);
        assert(std::fabs(NorthlightGI::dot(q,q)-1)<1e-5f&&std::acos(std::min(1.f,NorthlightGI::dot(q,dir)))<.03f*3.14159265f/180);
    }
    std::puts("PASS cached static shadow map texel offset, depth delta and direction quantization");
    std::puts("PASS receiver-plane PCF depth correction at near/far cascades on flat and sloped surfaces");
    std::puts("PASS shadow-map center/depth, sun-facing occlusion ordering, and dynamic-caster camera cancellation");
}
