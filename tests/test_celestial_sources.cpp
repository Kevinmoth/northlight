#include "celestial_sources.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace NorthlightGI;
int main(){Vec3 legacy=normalized({.62f,.62f,.47f}),sun=normalized({-.7f,.1f,.3f}),moon=normalized({.1f,-.7f,.6f}),color={.15f,.2f,.3f};
 auto hidden=NorthlightCelestialSources::resolve(legacy,color,sun,moon,0,0);
 assert(hidden.authoredFill==1&&hidden.sources[0].weight==0&&hidden.sources[1].weight==0);
 assert(dot(hidden.sources[0].color,hidden.sources[0].color)==0);
 for(unsigned i=0;i<=1000;++i)for(unsigned j=0;j<=10;++j){float a=i*.001f,b=(1-a)*j*.1f;
  auto q=NorthlightCelestialSources::resolve(legacy,color,sun,moon,a,b);
  Vec3 sum=q.sources[0].color+q.sources[1].color+color*q.authoredFill;
  assert(dot(sum-color,sum-color)<1e-12f);
  assert(dot(q.sources[0].direction,sun)>.99999f&&dot(q.sources[1].direction,moon)>.99999f);
  auto rotatedFill=NorthlightCelestialSources::resolve(-legacy,color,sun,moon,a,b);
  assert(dot(q.sources[0].direction-rotatedFill.sources[0].direction,q.sources[0].direction-rotatedFill.sources[0].direction)==0);
  auto next=NorthlightCelestialSources::resolve(legacy,color,sun,moon,a+1e-6f,b);
  assert(dot(q.sources[0].color-next.sources[0].color,q.sources[0].color-next.sources[0].color)<1e-10f);
 }
 std::puts("PASS 11011 exact sky-direction, fill-energy, continuity cases; no below-horizon fallback beam");
}
