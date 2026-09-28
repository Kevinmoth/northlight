// 0.3.161 analytic blob reference (src/shadows/shadow_blob_model.h). Without an
// argument: shape, a BGRA8 decode round trip through NorthlightActorTexture::decode
// (the filter's path after LockRect) and rejections. With the client's decoded
// shadowblob.blp (32x32 RGBA8 file, test_shadow_blob_client.py): the model's
// error against it and accept/reject parity between the model and that real
// texture as the reference. No D3D, game or Wine.
// 0.3.171: the 16-bit decodes (A1R5G5B5 is how the game uploads shadowblob.blp),
// the model and every variant through an A1R5G5B5 upload, and with the client
// file the real texture through that upload.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <vector>
#include "actor_texture.h"
#include "shadow_blob_model.h"

using Pixels=std::vector<std::uint8_t>;
static const std::size_t Bytes=std::size_t(NorthlightShadowBlobModel::Width)*NorthlightShadowBlobModel::Height*4;
static bool accepted(const Pixels& texture,const Pixels& reference){return NorthlightShadowBlobModel::matches(texture.data(),reference.data(),Bytes);}
static Pixels disc(double radius,double cx,double cy,std::uint8_t grey){
    Pixels p(Bytes);
    for(unsigned i=0;i<Bytes/4;++i){const bool in=std::hypot(i%32-cx,i/32-cy)<radius;p[i*4]=p[i*4+1]=p[i*4+2]=in?grey:255;p[i*4+3]=in?255:0;}
    return p;
}
// Variants a 32x32 game texture might be: the reference itself, shifted, rescaled,
// recoloured, inverted, flat and noise.
static std::vector<Pixels> variants(const Pixels& base){
    std::vector<Pixels> out{base};
    for(int dx=-3;dx<=3;++dx)for(int dy=-3;dy<=3;++dy){
        Pixels p(Bytes);
        for(int y=0;y<32;++y)for(int x=0;x<32;++x)for(int c=0;c<4;++c){int sx=x-dx,sy=y-dy;p[(y*32+x)*4+c]=sx<0||sy<0||sx>31||sy>31?(c==3?0:255):base[(sy*32+sx)*4+c];}
        out.push_back(p);
    }
    for(double r=8;r<=16;r+=.5)for(int g:{0,100,160,200,255})out.push_back(disc(r,15.5,15.5,std::uint8_t(g)));
    for(int shift:{-120,-60,-30,30,60}){Pixels p=base;for(std::size_t i=0;i<Bytes;i+=4)for(int c=0;c<3;++c)p[i+c]=std::uint8_t(std::max(0,std::min(255,p[i+c]+shift)));out.push_back(p);}
    {Pixels p=base;for(std::size_t i=3;i<Bytes;i+=4)p[i]=std::uint8_t(255-p[i]);out.push_back(p);}
    {Pixels p(Bytes,255);out.push_back(p);Pixels z(Bytes,0);out.push_back(z);}
    std::uint32_t seed=161;for(int n=0;n<64;++n){Pixels p(Bytes);for(auto& b:p){seed=seed*1664525u+1013904223u;b=std::uint8_t(seed>>24);}out.push_back(p);}
    return out;
}
using NorthlightActorTexture::Format;
static std::array<std::uint8_t,4> one(std::uint16_t value,Format format){
    Pixels out;assert(NorthlightActorTexture::decode(&value,2,1,1,2,format,out)&&out.size()==4);return {out[0],out[1],out[2],out[3]};
}
// RGBA8 pixels as a 16-bit D3D upload (BGRA-ordered little-endian words), 8-bit
// channels truncated (or rounded) to 5 or 4 bits, then back through the filter's decode.
static Pixels upload(const Pixels& rgba,Format format,bool round=false){
    std::vector<std::uint16_t> words(rgba.size()/4);
    auto bits=[&](std::uint8_t v,unsigned n){unsigned m=(1u<<n)-1;return round?(unsigned(v)*m+127)/255:unsigned(v)>>(8-n);};
    for(std::size_t i=0;i<words.size();++i){const std::uint8_t* p=&rgba[i*4];
        words[i]=std::uint16_t(format==Format::ARGB4444?bits(p[3],4)<<12|bits(p[0],4)<<8|bits(p[1],4)<<4|bits(p[2],4)
            :(format==Format::ARGB1555&&p[3]>=128?0x8000u:0u)|bits(p[0],5)<<10|bits(p[1],5)<<5|bits(p[2],5));}
    Pixels decoded;assert(NorthlightActorTexture::decode(words.data(),words.size()*2,32,32,64,format,decoded)&&decoded.size()==Bytes);return decoded;
}
static void sixteenBit(){
    using A=std::array<std::uint8_t,4>;
    assert((one(0xffff,Format::ARGB1555)==A{255,255,255,255}));
    assert((one(0x7c00,Format::ARGB1555)==A{255,0,0,0}));
    assert((one(0x7c00,Format::XRGB1555)==A{255,0,0,255}));
    assert((one(0x8000|1<<10|16<<5|2,Format::ARGB1555)==A{8,132,16,255}));  // 5-bit: (v<<3)|(v>>2)
    assert((one(0x8f31,Format::ARGB4444)==A{255,51,17,136}));
    assert((one(0x0000,Format::ARGB4444)==A{0,0,0,0}));
    for(Format f:{Format::ARGB1555,Format::XRGB1555,Format::ARGB4444,Format::RGB565})assert(NorthlightActorTexture::rowBytes(5,f)==10&&NorthlightActorTexture::rowCount(7,f)==7);
    assert(NorthlightActorTexture::rowBytes(5,Format::BGRA8)==20&&NorthlightActorTexture::rowBytes(5,Format::BC1)==16&&NorthlightActorTexture::rowBytes(5,Format::BC3)==32&&NorthlightActorTexture::rowCount(7,Format::BC2)==2);
    // Padded rows and short sources.
    std::uint16_t padded[6]={0x8000|31,0x1f,0,0x7fff,0xffff,0};Pixels out;
    assert(NorthlightActorTexture::decode(padded,sizeof padded,2,2,6,Format::ARGB1555,out)&&out.size()==16);
    assert(out[2]==255&&out[3]==255&&out[6]==255&&out[7]==0&&out[8]==255&&out[11]==0&&out[12]==255&&out[15]==255);
    assert(!NorthlightActorTexture::decode(padded,10,2,2,6,Format::ARGB1555,out)&&out.empty());
    assert(!NorthlightActorTexture::decode(padded,sizeof padded,2,2,3,Format::ARGB4444,out));
}
int main(int argc,char** argv){
    const Pixels model=NorthlightShadowBlobModel::referencePixels();
    assert(model.size()==Bytes);
    unsigned opaque=0;
    for(std::size_t i=0;i<Bytes;i+=4){assert(model[i]==model[i+1]&&model[i]==model[i+2]);assert(model[i+3]==0||model[i+3]==255);opaque+=model[i+3]==255;}
    assert(opaque==648);
    assert(model[(15*32+15)*4]==160&&model[0]==255&&model[3]==0);
    // The filter's decode path: the texture as the game's A8R8G8B8 upload (BGRA in memory).
    Pixels bgra(Bytes),decoded;
    for(std::size_t i=0;i<Bytes;i+=4){bgra[i]=model[i+2];bgra[i+1]=model[i+1];bgra[i+2]=model[i];bgra[i+3]=model[i+3];}
    assert(NorthlightActorTexture::decode(bgra.data(),bgra.size(),32,32,32*4,NorthlightActorTexture::Format::BGRA8,decoded));
    assert(decoded.size()==Bytes&&accepted(decoded,model));
    unsigned rejected=0;for(auto& v:variants(model))rejected+=!accepted(v,model);
    assert(rejected>=100);
    std::printf("model: 32x32, %u opaque, BGRA8 round trip accepted, %u of %zu variants rejected\n",opaque,rejected,variants(model).size());
    sixteenBit();
    // The model as A1R5G5B5 / A4R4G4B4 uploads is accepted; every variant keeps its
    // 8-bit verdict through an A1R5G5B5 upload (no new claims from the 16-bit path).
    for(bool round:{false,true})for(Format f:{Format::ARGB1555,Format::ARGB4444})assert(accepted(upload(model,f,round),model));
    unsigned kept=0;const auto modelVariants=variants(model);
    for(auto& v:modelVariants){const bool a=accepted(v,model),b=accepted(upload(v,Format::ARGB1555),model);if(a==b)++kept;else std::printf("A1R5G5B5 verdict differs: 8-bit=%d 16-bit=%d\n",a,b);}
    assert(kept==modelVariants.size());
    std::printf("16-bit: A1R5G5B5/X1R5G5B5/A4R4G4B4 decode exact; model accepted as A1R5G5B5 and A4R4G4B4; %u of %zu variants keep their verdict as A1R5G5B5\n",kept,modelVariants.size());
    if(argc<2)return 0;
    FILE* f=std::fopen(argv[1],"rb");assert(f);Pixels real(Bytes);assert(std::fread(real.data(),1,Bytes,f)==Bytes);std::fclose(f);
    double alpha=0,colour=0;
    for(std::size_t i=0;i<Bytes;i+=4){alpha+=std::abs(real[i+3]-model[i+3]);for(int c=0;c<3;++c)colour+=std::abs(real[i+c]-model[i+c]);}
    alpha/=Bytes/4;colour/=Bytes/4*3;
    std::printf("client shadowblob.blp vs model: mean alpha error %.3f, mean colour error %.3f (thresholds 6, 24)\n",alpha,colour);
    assert(alpha==0&&colour<4&&accepted(real,model));
    // The real texture as the game uploads it (1-bit alpha: A1R5G5B5), through the filter's decode.
    for(bool round:{false,true}){const Pixels uploaded=upload(real,Format::ARGB1555,round);double a=0,c=0;
        for(std::size_t i=0;i<Bytes;i+=4){a+=std::abs(uploaded[i+3]-model[i+3]);for(int k=0;k<3;++k)c+=std::abs(uploaded[i+k]-model[i+k]);}
        std::printf("client shadowblob.blp as A1R5G5B5 (%s): mean alpha error %.3f, mean colour error %.3f, accepted %d\n",round?"rounded":"truncated",a/(Bytes/4),c/(Bytes/4*3),int(accepted(uploaded,model)));
        assert(a==0&&accepted(uploaded,model));}
    // Parity: every variant gets the same verdict against the model as against the real texture.
    unsigned same=0,accepts=0;const auto all=[&]{auto a=variants(real);for(auto& v:variants(model))a.push_back(v);return a;}();
    for(auto& v:all){const bool a=accepted(v,real),b=accepted(v,model);if(a==b)++same;else std::printf("parity differs: real=%d model=%d\n",a,b);accepts+=b;}
    std::printf("parity: %u of %zu variants same verdict (%u accepted)\n",same,all.size(),accepts);
    assert(same==all.size());
    return 0;
}
