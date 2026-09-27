// Offline check of the 0.3.137 direction prebuild (static_plan_prebuild.h):
// along sampled celestial arcs at 60 fps, the scheduler's early plan for the
// next lattice step must be the step frame's exact matrix (bit-identical), and
// wasted builds stay capped, including with a moving pivot (no builds).
#include "static_plan_prebuild.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <set>
#include <string>
using NorthlightGI::Vec3;
static uint32_t rng=0x1337;
static float random(float lo,float hi){rng=rng*1664525u+1013904223u;return lo+(hi-lo)*float(rng>>8)/16777216.f;}
static void cached(Vec3 pivot,int cascade,Vec3 direction,float* m){const float radius=cascade==0?48.f:192.f;NorthlightWorldMath::shadowMatrixFrom(NorthlightWorldMath::shadowFrame(pivot,direction,radius),radius*1280.f/1024.f,m);}
int main(){
 // Lattice prediction alone, every frame.
 unsigned steps=0,exact=0;
 for(unsigned arc=0;arc<100;++arc){const float speed=(arc%4==3?random(1,50):1.f)*.25f/60000.f*.01745329252f,tilt=random(-.6f,.6f),phase=random(-.2f,1.2f);
  auto at=[&](double ms){const double a=phase+ms*speed;return NorthlightGI::normalized(Vec3(float(std::cos(a)),float(std::sin(tilt)*std::sin(a)),float(std::cos(tilt)*std::sin(a))));};
  Vec3 current=NorthlightWorldMath::quantizeDirection(at(0)),pending,sample=at(0),velocity;double sampleMs=0;bool has=false,valid=false;
  for(double ms=16;ms<300000;ms+=16.6667){const Vec3 raw=at(ms),q=NorthlightWorldMath::quantizeDirection(raw);
   if(std::memcmp(&q,&current,sizeof q)){++steps;exact+=has&&!std::memcmp(&pending,&q,sizeof q);current=q;}
   if(ms-sampleMs>=1000){velocity=(raw-sample)*(1.f/float(ms-sampleMs));sample=raw;sampleMs=ms;valid=true;}
   if(valid)has=NorthlightStaticPrebuild::nextQuantizedDirection(raw,velocity,4000,pending);}
 }
 assert(exact*10>=steps*9);
 // Scheduler with a fake plan cache: builds, hits and waste per scenario.
 for(int moving=0;moving<2;++moving){unsigned hits=0,rerenders=0,builds=0,discards=0;
  for(unsigned arc=0;arc<60;++arc){const float speed=.25f/60000.f*.01745329252f,tilt=random(-.6f,.6f),phase=random(0,1);
   auto at=[&](double ms){const double a=phase+ms*speed;return NorthlightGI::normalized(Vec3(float(std::cos(a)),float(std::sin(tilt)*std::sin(a)),float(std::cos(tilt)*std::sin(a))));};
   NorthlightStaticPrebuild::Scheduler scheduler;std::set<std::string> plans,live;
   auto discard=[&](const float* m){const std::string k((const char*)m,64);assert(!live.count(k));plans.erase(k);++discards;};Vec3 pivot{random(-9000,9000),random(-9000,9000),random(0,200)};
   Vec3 key=NorthlightWorldMath::quantizeDirection(at(0));const bool active[2]={true,false};
   for(double ms=0;ms<600000;ms+=16.6667){if(moving)pivot.x+=.01f;const Vec3 raw=at(ms);Vec3 quantized[2]={NorthlightWorldMath::quantizeDirection(raw),Vec3(0,0,1)};
    scheduler.observe(0,true,raw,uint32_t(ms));scheduler.observe(1,false,Vec3(0,0,1),uint32_t(ms));
    bool rendered=false;
    if(std::memcmp(&quantized[0],&key,sizeof key)){key=quantized[0];rendered=true;
     for(int cascade=0;cascade<2;++cascade){float m[16];cached(pivot,cascade,key,m);const std::string k((const char*)m,64);const bool ready=plans.count(k)>0;scheduler.rerender(0,cascade,m,true,ready,discard);hits+=ready;++rerenders;plans.insert(k);live.insert(k);}}
    scheduler.frame(pivot,quantized,active,!rendered,uint32_t(ms),[&](int,int c,Vec3 d,float* m){cached(pivot,c,d,m);},[&](const float* m){return plans.count(std::string((const char*)m,64))>0;},[&](const float* m){plans.insert(std::string((const char*)m,64));++builds;return true;},discard);
    assert(plans.size()<=live.size()+2); /* at most one unused prebuilt plan per cascade */
   }
  }
  std::cout<<(moving?"moving":"still")<<" pivot: "<<rerenders<<" step re-renders, "<<hits<<" prebuilt hits, "<<builds<<" prebuilds ("<<(builds>hits?builds-hits:0)<<" unused, "<<discards<<" discarded)\n";
  if(moving)assert(builds==0);else assert(hits*10>=rerenders*9&&builds<=hits+hits/10+4);
 }
 std::cout<<"direction prebuild: "<<steps<<" lattice steps, "<<exact<<" predicted bit-identically\n";
 // ShadowDirectionSteps != 2048 (northlight-quality.ini): the prebuild follows the renderer's lattice.
 for(float lattice:{2048.f,1024.f,1000.f,512.f,300.f,256.f}){
  for(unsigned i=0;i<200000;++i){ /* a lattice point built from k is bit-identical to quantizing any raw direction rounding to k */
   const Vec3 raw=NorthlightGI::normalized(Vec3(random(-1,1),random(-1,1),random(-1,1))),q=NorthlightWorldMath::quantizeDirection(raw,lattice);
   const Vec3 k(std::round(raw.x*lattice)/lattice,std::round(raw.y*lattice)/lattice,std::round(raw.z*lattice)/lattice),viaK=NorthlightWorldMath::quantizeDirection(k,lattice);
   assert(!std::memcmp(&q,&viaK,sizeof q));}
  unsigned latticeSteps=0,latticeExact=0,hits=0,rerenders=0,builds=0;
  for(unsigned arc=0;arc<40;++arc){const float speed=.25f/60000.f*.01745329252f*(lattice<1024?4.f:1.f),tilt=random(-.6f,.6f),phase=random(0,1);
   auto at=[&](double ms){const double a=phase+ms*speed;return NorthlightGI::normalized(Vec3(float(std::cos(a)),float(std::sin(tilt)*std::sin(a)),float(std::cos(tilt)*std::sin(a))));};
   NorthlightStaticPrebuild::Scheduler scheduler;scheduler.steps=lattice;std::set<std::string> plans,live;
   auto discard=[&](const float* m){const std::string k((const char*)m,64);assert(!live.count(k));plans.erase(k);};Vec3 pivot{random(-9000,9000),random(-9000,9000),random(0,200)};
   Vec3 key=NorthlightWorldMath::quantizeDirection(at(0),lattice),pending,sample=at(0),velocity;const bool active[2]={true,false};double sampleMs=0;bool has=false,valid=false;
   for(double ms=0;ms<400000;ms+=16.6667){const Vec3 raw=at(ms);Vec3 quantized[2]={NorthlightWorldMath::quantizeDirection(raw,lattice),Vec3(0,0,1)};
    scheduler.observe(0,true,raw,uint32_t(ms));scheduler.observe(1,false,Vec3(0,0,1),uint32_t(ms));bool rendered=false;
    if(std::memcmp(&quantized[0],&key,sizeof key)){++latticeSteps;latticeExact+=has&&!std::memcmp(&pending,&quantized[0],sizeof pending);key=quantized[0];rendered=true;
     for(int cascade=0;cascade<2;++cascade){float m[16];cached(pivot,cascade,key,m);const std::string k((const char*)m,64);const bool ready=plans.count(k)>0;scheduler.rerender(0,cascade,m,true,ready,discard);hits+=ready;++rerenders;plans.insert(k);live.insert(k);}}
    if(ms-sampleMs>=1000){velocity=(raw-sample)*(1.f/float(ms-sampleMs));sample=raw;sampleMs=ms;valid=true;}
    if(valid)has=NorthlightStaticPrebuild::nextQuantizedDirection(raw,velocity,4000,pending,lattice);
    scheduler.frame(pivot,quantized,active,!rendered,uint32_t(ms),[&](int,int c,Vec3 d,float* m){cached(pivot,c,d,m);},[&](const float* m){return plans.count(std::string((const char*)m,64))>0;},[&](const float* m){plans.insert(std::string((const char*)m,64));++builds;return true;},discard);
    assert(plans.size()<=live.size()+2);
   }
  }
  std::cout<<"steps="<<lattice<<": "<<latticeSteps<<" lattice steps, "<<latticeExact<<" predicted exactly, "<<rerenders<<" re-renders, "<<hits<<" prebuilt hits, "<<builds<<" prebuilds\n";
  assert(latticeSteps&&latticeExact*10>=latticeSteps*9&&hits*10>=rerenders*9&&builds<=hits+hits/10+4);
 }
}
