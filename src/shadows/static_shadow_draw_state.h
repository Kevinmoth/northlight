#pragma once

// A cache for ONE uninterrupted static-caster draw call. Instance buffer
// locks and draw calls do not change the shader/declaration/frequency state.
// A failed setup invalidates every field before the original fallback runs.
namespace StaticShadow {
class DrawMode {
    bool known_=false,instanced_=false;
    UINT count_=0,offset_=0;
public:
    void invalidate(){known_=false;}
    bool apply(IDirect3DDevice9* d,bool instanced,UINT count,UINT offset,
               IDirect3DVertexShader9* shader,IDirect3DVertexDeclaration9* declaration,
               IDirect3DVertexBuffer9* instances,UINT stride){
        auto failed=[&](HRESULT hr){if(FAILED(hr)){invalidate();return true;}return false;};
        const bool modeChange=!known_||instanced_!=instanced;
        if(modeChange){
            if(failed(d->SetVertexShader(shader))||failed(d->SetVertexDeclaration(declaration)))return false;
        }
        if(instanced&& (modeChange||offset_!=offset))
            if(failed(d->SetStreamSource(1,instances,offset,stride)))return false;
        if(modeChange||(instanced&&count_!=count))
            if(failed(d->SetStreamSourceFreq(0,instanced?(D3DSTREAMSOURCE_INDEXEDDATA|count):1)))return false;
        if(modeChange)
            if(failed(d->SetStreamSourceFreq(1,instanced?(D3DSTREAMSOURCE_INSTANCEDATA|1):1)))return false;
        known_=true;instanced_=instanced;count_=count;offset_=offset;return true;
    }
};
}
