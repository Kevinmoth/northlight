#include "test_static_shadow_fake_d3d.h"
#include "static_shadow_draw_state.h"
#include <cassert>
#include <cstdio>
int main(){
 IDirect3DVertexShader9 shader[2];IDirect3DVertexDeclaration9 decl[2];IDirect3DVertexBuffer9 instances(4096);
 IDirect3DDevice9 old,current;StaticShadow::DrawMode cache;
 for(unsigned draw=0;draw<20000;++draw){
  const bool instanced=(draw/40)%3!=0;const unsigned count=instanced?2+(draw/4)%20:1,offset=(draw%8)*48;
  old.SetVertexShader(&shader[instanced]);old.SetVertexDeclaration(&decl[instanced]);
  if(instanced)old.SetStreamSource(1,&instances,offset,48);
  old.SetStreamSourceFreq(0,instanced?(D3DSTREAMSOURCE_INDEXEDDATA|count):1);
  old.SetStreamSourceFreq(1,instanced?(D3DSTREAMSOURCE_INSTANCEDATA|1):1);
  assert(cache.apply(&current,instanced,count,offset,&shader[instanced],&decl[instanced],&instances,48));
  assert(current.currentVertex==old.currentVertex&&current.currentDeclaration==old.currentDeclaration);
  assert(current.frequencies[0]==old.frequencies[0]&&current.frequencies[1]==old.frequencies[1]);
  if(instanced){assert(current.streams[1]==old.streams[1]&&current.streamOffsets[1]==old.streamOffsets[1]&&current.streamStrides[1]==48);}
 }
 assert(current.modeCalls<old.modeCalls/3);
 for(unsigned failure=1;failure<=5;++failure){
  IDirect3DDevice9 d;StaticShadow::DrawMode state;d.failModeAt=failure;
  assert(!state.apply(&d,true,10,48,&shader[1],&decl[1],&instances,48));
  d.failModeAt=0;assert(state.apply(&d,false,1,0,&shader[0],&decl[0],&instances,48));
  assert(d.currentVertex==&shader[0]&&d.currentDeclaration==&decl[0]&&d.frequencies[0]==1&&d.frequencies[1]==1);
  assert(state.apply(&d,true,7,144,&shader[1],&decl[1],&instances,48));
  state.invalidate(); // An instanced draw failure forces complete fallback setup.
  assert(state.apply(&d,false,1,0,&shader[0],&decl[0],&instances,48));
  assert(d.frequencies[0]==1&&d.frequencies[1]==1);
 }
 std::printf("PASS 20000 draw-state comparisons; calls %u -> %u; all five setup failure points and draw fallback restore state\n",old.modeCalls,current.modeCalls);
}
