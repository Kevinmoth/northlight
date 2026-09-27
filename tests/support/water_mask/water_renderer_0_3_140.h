#pragma once
// Include after host SavedState/drop/logf. All borrowed inputs remain caller-owned.
#include <d3d9.h>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <unordered_map>
#include "water_shader_signatures.h"
#include "water_compiled_shaders.h"
struct NorthlightWaterContext {
    float inverseView[16]={};
    float projection[3]={};
    float nearZ=0,farZ=0,minZ=0,maxZ=1;
    float sunDirection[3]={},sunColor[3]={};
    float moonDirection[3]={},moonColor[3]={};
    float skyColor[3]={};
    float seconds=0;
    // Source 0 sun, 1 moon. Near/far cascades centered on the current camera.
    // Null pair disables that source's shadow sampling, never aliases another source.
    IDirect3DTexture9* sourceShadows[2][2]={};
    float sourceShadowMatrices[2][2][16]={};
    float sourceShadowDirection[2][3]={};
    float shadowTexel=1.f/1024;
};
class NorthlightWaterRenderer {
    IDirect3DDevice9* d;
    NorthlightStateBlockPool stateBlocks;
    std::unordered_map<IDirect3DVertexShader9*,IDirect3DVertexShader9*> vertex;
    std::unordered_map<IDirect3DPixelShader9*,IDirect3DPixelShader9*> pixel;
    IDirect3DTexture9 *mask=nullptr,*color=nullptr,*reflection=nullptr;
    IDirect3DSurface9 *maskSurface=nullptr,*colorSurface=nullptr,*reflectionSurface=nullptr,*depth=nullptr;
    IDirect3DPixelShader9 *reflectionPS=nullptr,*compositePS=nullptr;
    UINT width=0,height=0;D3DFORMAT colorFormat=D3DFMT_UNKNOWN;
    bool failed=false,captured=false;float maskMinZ=0,maskMaxZ=1;unsigned captures=0,frames=0;
    bool check(HRESULT hr,const char* where){if(SUCCEEDED(hr))return true;if(!failed)logf("WATER disabled: %s HRESULT=0x%08lx",where,(unsigned long)hr);failed=true;return false;}
    bool target(UINT w,UINT h,D3DFORMAT fmt,IDirect3DTexture9*& tex,IDirect3DSurface9*& surface){
        return check(d->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,fmt,D3DPOOL_DEFAULT,&tex,nullptr),"create target")&&check(tex->GetSurfaceLevel(0,&surface),"get surface");
    }
    bool maskResources(UINT w,UINT h){
        if(failed||!w||!h)return false;
        if(w!=width||h!=height){reset();width=w;height=h;}
        if(mask)return true;
        return target(w,h,D3DFMT_G32R32F,mask,maskSurface)&&check(d->CreateDepthStencilSurface(w,h,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,TRUE,&depth,nullptr),"mask depth");
    }
    HRESULT quad(){struct V{float x,y,z,w,u,v;} v[]={{-.5f,-.5f,0,1,0,0},{float(width)-.5f,-.5f,0,1,1,0},{-.5f,float(height)-.5f,0,1,0,1},{float(width)-.5f,float(height)-.5f,0,1,1,1}};D3DVIEWPORT9 vp={0,0,width,height,0,1};d->SetViewport(&vp);return d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(V));}
public:
    explicit NorthlightWaterRenderer(IDirect3DDevice9* device):d(device),stateBlocks(device){}
    ~NorthlightWaterRenderer(){reset();drop(reflectionPS);drop(compositePS);for(auto& p:vertex)drop(p.second);for(auto& p:pixel)drop(p.second);}
    NorthlightWaterRenderer(const NorthlightWaterRenderer&)=delete;
    void releaseStateCache(){stateBlocks.clear();}
    void reset(){stateBlocks.clear();drop(maskSurface);drop(colorSurface);drop(reflectionSurface);drop(depth);drop(mask);drop(color);drop(reflection);width=height=0;colorFormat=D3DFMT_UNKNOWN;captured=false;captures=0;}
    void endFrame(){if(!captured||failed)stateBlocks.clear();captured=false;captures=0;}
    // Explicit retry only after host frame cleanup; no allocation or hot-path work.
    void recover(){if(!failed)return;reset();failed=false;logf("WATER explicit recovery requested");}
    bool hasCapturedWater()const{return captured&&!failed;}
    IDirect3DTexture9* maskTextureForDepth(float minZ,float maxZ)const{
        return captured&&!failed&&mask&&std::isfinite(minZ)&&std::isfinite(maxZ)&&maxZ>minZ&&
            std::fabs(minZ-maskMinZ)<.0001f&&std::fabs(maxZ-maskMaxZ)<.0001f?mask:nullptr;
    }
    bool recognizesVertex(IDirect3DVertexShader9* shader)const{return !failed&&vertex.find(shader)!=vertex.end();}
    void registerVertex(IDirect3DVertexShader9* original,std::uint64_t hash){
        auto old=vertex.find(original);if(old!=vertex.end()){drop(old->second);vertex.erase(old);}
        if(failed)return;
        for(auto& candidate:kWaterShaderVariants)if(candidate.vertex&&candidate.hash==hash){IDirect3DVertexShader9* shader=nullptr;if(check(d->CreateVertexShader(candidate.words,&shader),"water vertex mask shader")){vertex.emplace(original,shader);logf("WATER registered vertex hash=%016llx",(unsigned long long)hash);}return;}
    }
    void registerPixel(IDirect3DPixelShader9* original,std::uint64_t hash){
        auto old=pixel.find(original);if(old!=pixel.end()){drop(old->second);pixel.erase(old);}
        if(failed)return;
        for(auto& candidate:kWaterShaderVariants)if(!candidate.vertex&&candidate.hash==hash){IDirect3DPixelShader9* shader=nullptr;if(check(d->CreatePixelShader(candidate.words,&shader),"water pixel mask shader")){pixel.emplace(original,shader);logf("WATER registered pixel hash=%016llx",(unsigned long long)hash);}return;}
    }
    template<class Draw> void capture(IDirect3DVertexShader9* vs,IDirect3DPixelShader9* ps,UINT mainWidth,UINT mainHeight,Draw draw){
        if(failed)return;auto vi=vertex.find(vs);auto pi=pixel.find(ps);if(vi==vertex.end()||pi==pixel.end())return;
        D3DVIEWPORT9 vp={};if(FAILED(d->GetViewport(&vp))||vp.X||vp.Y||vp.Width!=mainWidth||vp.Height!=mainHeight)return;
        if(captured&&(vp.MinZ!=maskMinZ||vp.MaxZ!=maskMaxZ))return;
        SavedState save(d,&stateBlocks);if(!save.ok)return;D3DSURFACE_DESC desc={};if(FAILED(save.targets[0]->GetDesc(&desc))||desc.Width!=mainWidth||desc.Height!=mainHeight)return;
        if(!maskResources(mainWidth,mainHeight))return;
        d->SetDepthStencilSurface(nullptr);for(unsigned i=1;i<4;++i)d->SetRenderTarget(i,nullptr);
        // s2 may still contain last frame's mask in host state. Avoid RT/sampler alias.
        for(unsigned i=0;i<16;++i){IDirect3DBaseTexture9* texture=nullptr;if(SUCCEEDED(d->GetTexture(i,&texture))&&texture==mask)d->SetTexture(i,nullptr);drop(texture);}
        if(!check(d->SetRenderTarget(0,maskSurface),"mask target")||!check(d->SetDepthStencilSurface(depth),"mask depth bind"))return;
        d->SetViewport(&vp);
        if(!captured){if(!check(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0),"clear mask"))return;maskMinZ=vp.MinZ;maskMaxZ=vp.MaxZ;}
        d->SetRenderState(D3DRS_ZENABLE,TRUE);d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);d->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);d->SetRenderState(D3DRS_COLORWRITEENABLE,15);d->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);d->SetRenderState(D3DRS_FOGENABLE,FALSE);d->SetRenderState(D3DRS_STENCILENABLE,FALSE);
        // Keep original cull, alpha test, texture bindings, constants and geometry.
        if(!check(d->SetVertexShader(vi->second),"mask VS")||!check(d->SetPixelShader(pi->second),"mask PS")||!check(draw(),"mask draw"))return;
        captured=true;++captures;
    }
    bool render(IDirect3DSurface9* targetSurface,IDirect3DTexture9* opaqueDepth,UINT w,UINT h,D3DFORMAT format,const NorthlightWaterContext& context){
        if(failed||!captured||!targetSurface||!opaqueDepth||w!=width||h!=height||context.nearZ<=0||context.farZ<=context.nearZ||context.maxZ<=context.minZ||std::fabs(context.projection[0])<.001f||std::fabs(context.projection[1])<.001f||std::fabs(context.projection[2])<.5f||std::fabs(context.minZ-maskMinZ)>.0001f||std::fabs(context.maxZ-maskMaxZ)>.0001f)return false;
        SavedState save(d,&stateBlocks);if(!save.ok)return false;
        if(!reflectionPS&&!check(d->CreatePixelShader(kWaterReflectionShader,&reflectionPS),"reflection shader"))return false;
        if(!compositePS&&!check(d->CreatePixelShader(kWaterCompositeShader,&compositePS),"water composite shader"))return false;
        if(colorFormat!=format){drop(colorSurface);drop(color);colorFormat=format;}
        if(!color&&!target(w,h,format,color,colorSurface))return false;
        if(!reflection&&!target(w,h,D3DFMT_A16B16G16R16F,reflection,reflectionSurface))return false;
        d->SetDepthStencilSurface(nullptr);for(unsigned i=1;i<4;++i)d->SetRenderTarget(i,nullptr);
        for(unsigned i=0;i<16;++i)d->SetTexture(i,nullptr);
        if(!check(d->StretchRect(targetSurface,nullptr,colorSurface,nullptr,D3DTEXF_NONE),"copy scene"))return false;
        d->SetVertexShader(nullptr);d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);d->SetStreamSourceFreq(0,1);
        const struct{D3DRENDERSTATETYPE s;DWORD v;} states[]={{D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_ALPHATESTENABLE,FALSE},{D3DRS_ALPHABLENDENABLE,FALSE},{D3DRS_SEPARATEALPHABLENDENABLE,FALSE},{D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_COLORWRITEENABLE,15},{D3DRS_FOGENABLE,FALSE},{D3DRS_STENCILENABLE,FALSE},{D3DRS_SCISSORTESTENABLE,FALSE},{D3DRS_CLIPPLANEENABLE,0},{D3DRS_SRGBWRITEENABLE,FALSE},{D3DRS_FILLMODE,D3DFILL_SOLID},{D3DRS_MULTISAMPLEMASK,0xffffffff},{D3DRS_WRAP0,0}};
        for(auto& state:states)d->SetRenderState(state.s,state.v);
        d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
        float c[29][4]={};c[0][0]=1.f/w;c[0][1]=1.f/h;c[0][2]=context.nearZ;c[0][3]=context.farZ;
        memcpy(c[1],context.projection,12);c[1][3]=context.minZ;c[2][0]=1/(context.maxZ-context.minZ);c[2][1]=context.seconds;c[2][2]=.85f;c[2][3]=.45f;
        memcpy(c[3],context.inverseView,64);memcpy(c[7],context.sunDirection,12);memcpy(c[8],context.sunColor,12);memcpy(c[9],context.moonDirection,12);memcpy(c[10],context.moonColor,12);memcpy(c[11],context.skyColor,12);
        for(unsigned source=0;source<2;++source){float dot=0;const float* wanted=source?context.moonDirection:context.sunDirection;for(unsigned k=0;k<3;++k)dot+=wanted[k]*context.sourceShadowDirection[source][k];c[28][source]=context.sourceShadows[source][0]&&context.sourceShadows[source][1]&&dot>.98f?1.f:0.f;for(unsigned cascade=0;cascade<2;++cascade)memcpy(c[12+source*8+cascade*4],context.sourceShadowMatrices[source][cascade],64);}
        c[28][2]=context.shadowTexel;
        if(!check(d->SetPixelShaderConstantF(0,c[0],29),"water constants"))return false;
        IDirect3DTexture9* textures[]={color,opaqueDepth,mask,nullptr,context.sourceShadows[0][0],context.sourceShadows[0][1],context.sourceShadows[1][0],context.sourceShadows[1][1]};
        for(unsigned i=0;i<8;++i){d->SetTexture(i,textures[i]);d->SetSamplerState(i,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);d->SetSamplerState(i,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);d->SetSamplerState(i,D3DSAMP_MINFILTER,i==0?D3DTEXF_LINEAR:D3DTEXF_POINT);d->SetSamplerState(i,D3DSAMP_MAGFILTER,i==0?D3DTEXF_LINEAR:D3DTEXF_POINT);d->SetSamplerState(i,D3DSAMP_MIPFILTER,D3DTEXF_NONE);d->SetSamplerState(i,D3DSAMP_SRGBTEXTURE,FALSE);}
        if(!check(d->SetRenderTarget(0,reflectionSurface),"reflection target")||!check(d->SetPixelShader(reflectionPS),"reflection bind")||!check(quad(),"reflection draw"))return false;
        if(!check(d->SetRenderTarget(0,targetSurface),"composite target")||!check(d->SetTexture(3,reflection),"reflection input")||!check(d->SetPixelShader(compositePS),"composite bind")||!check(quad(),"composite draw"))return false;
        if(++frames==1||frames%600==0)logf("WATER frame=%u draws=%u method=screen-space-reflection skyFallback=1 exactLiquidMask=1 sunShadow=%d moonShadow=%d",frames,captures,int(c[28][0]),int(c[28][1]));
        return true;
    }
};
