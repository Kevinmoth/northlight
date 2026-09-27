#pragma once
// The game's unit blob shadow texture, described analytically for
// shadow_blob_filter.h (0.3.161; the DLL carries no game texture), and the
// filter's content comparison. Pure: no D3D. tests/test_shadow_blob_model.py.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>
namespace NorthlightShadowBlobModel {
// A 32x32 grey disc centred at (15.5,15.5): alpha 255 inside radius 14.4 and 0
// outside; grey 160 up to r=6, rising linearly to 255 at r=12.5; colour 255
// where alpha is 0. Against the client's shadowblob.blp: mean alpha error 0,
// mean colour error 1.6 (the thresholds below are 6 and 24).
constexpr unsigned Width=32,Height=32;
inline std::vector<std::uint8_t> referencePixels(){
    std::vector<std::uint8_t> rgba(std::size_t(Width)*Height*4);
    for(unsigned y=0;y<Height;++y)for(unsigned x=0;x<Width;++x){
        const double r=std::hypot(x-15.5,y-15.5);
        const bool inside=r<14.4;
        const double grey=!inside||r>=12.5?255.:r<=6.?160.:160.+95.*(r-6.)/(12.5-6.);
        std::uint8_t* p=&rgba[(std::size_t(y)*Width+x)*4];
        p[0]=p[1]=p[2]=std::uint8_t(grey+.5);p[3]=inside?255:0;
    }
    return rgba;
}
// Decoded pixels of a Width x Height texture against the reference. The blob
// is a flat grey disc with a hard alpha edge: alpha must match almost exactly;
// colour is compared loosely (channel order irrelevant).
inline bool matches(const std::uint8_t* decoded,const std::uint8_t* reference,std::size_t bytes){
    std::uint64_t alphaError=0,colorError=0;
    for(std::size_t i=0;i<bytes;i+=4){
        alphaError+=std::abs(int(decoded[i+3])-int(reference[i+3]));
        for(int c=0;c<3;++c)colorError+=std::abs(int(decoded[i+c])-int(reference[i+c]));
    }
    const std::size_t pixels=bytes/4;
    return alphaError/pixels<6&&colorError/(pixels*3)<24;
}
}
