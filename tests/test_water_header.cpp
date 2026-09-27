// Cross-compile only. No game/device is constructed or run by this harness.
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdarg>
static void logf(const char*,...){}
template<class T> static void drop(T*&p){if(p){p->Release();p=nullptr;}}
#include "saved_state.h"
#include "water_renderer.h"
void compilePublicAPI(IDirect3DDevice9* d,IDirect3DVertexShader9* vs,IDirect3DPixelShader9* ps,IDirect3DSurface9* target,IDirect3DTexture9* depth){
    NorthlightWaterRenderer water(d);NorthlightWaterContext context;
    water.registerVertex(vs,0,nullptr,0);water.registerPixel(ps,0,nullptr,0);
    D3DVIEWPORT9 vp={0,0,1920,1080,0,1};
    water.capture(vs,ps,1920,1080,target,vp,NorthlightWaterRenderer::NoUserPointer,0,[&](){return d->DrawPrimitive(D3DPT_TRIANGLELIST,0,1);});
    water.maskTextureForDepth(0,1);
    water.render(target,depth,1920,1080,D3DFMT_A8R8G8B8,context);water.endFrame();water.recover();water.reset();
}
