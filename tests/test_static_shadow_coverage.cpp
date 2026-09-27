#include "static_shadow_coverage.h"
#include "world_math.h"
#include <cassert>
#include <cstdio>
int main(){
    using namespace NorthlightGI;
    for(Vec3 light:{normalized(Vec3(1,1,.035f)),normalized(Vec3(-1,1,.25f)),Vec3(0,0,1)}){
        float m[16];const Vec3 center(100,-200,30);
        NorthlightWorldMath::shadowMatrix(center,light,240,m);
        assert(StaticShadow::containsBounds(m,center-Vec3(1,1,1),center+Vec3(1,1,1)));
        const Vec3 clipped=center+light*650;
        assert(!StaticShadow::containsBounds(m,clipped-Vec3(1,1,1),clipped+Vec3(1,1,1)));
        assert(!StaticShadow::containsBounds(m,center-Vec3(1000,1,1),center+Vec3(1000,1,1)));
        auto frame=NorthlightWorldMath::shadowFrame(center,light,192);
        auto moved=NorthlightWorldMath::shadowFrame(center+light*40,light,192);
        long x,y;float z;assert(NorthlightWorldMath::cacheOffset(moved,frame,x,y,z));
        // New depth slab contains this object but the old cached one did not.
        Vec3 edge=center+light*660;
        NorthlightWorldMath::shadowMatrixFrom(frame,240,m);
        assert(!StaticShadow::containsBounds(m,edge-Vec3(1,1,1),edge+Vec3(1,1,1)));
        NorthlightWorldMath::shadowMatrixFrom(moved,240,m);
        assert(StaticShadow::containsBounds(m,edge-Vec3(1,1,1),edge+Vec3(1,1,1)));
    }
    puts("PASS static coverage: full XY/depth containment, grazing light and cached-depth translation");
}
