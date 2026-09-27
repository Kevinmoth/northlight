#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace NorthlightActorTexture {
enum class Format { BGRA8,BGRX8,RGB565,BC1,BC2,BC3 };
inline std::array<std::uint8_t,4> rgb565(std::uint16_t value){unsigned r=value>>11,g=(value>>5)&63,b=value&31;return {std::uint8_t((r<<3)|(r>>2)),std::uint8_t((g<<2)|(g>>4)),std::uint8_t((b<<3)|(b>>2)),255};}
inline bool decode(const void* source,std::size_t sourceBytes,unsigned width,unsigned height,std::size_t pitch,Format format,std::vector<std::uint8_t>& output){
    output.clear();if(!source||!width||!height||width>128||height>128)return false;
    bool compressed=format==Format::BC1||format==Format::BC2||format==Format::BC3;
    std::size_t rows=compressed?(height+3)/4:height,rowBytes=compressed?((width+3)/4)*(format==Format::BC1?8:16):width*(format==Format::RGB565?2:4);
    if(pitch<rowBytes||rows>sourceBytes/pitch)return false;
    std::vector<std::uint8_t> rgba(std::size_t(width)*height*4);
    auto* bytes=static_cast<const std::uint8_t*>(source);
    if(!compressed){for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto* dst=rgba.data()+(y*width+x)*4;
        if(format==Format::RGB565){std::uint16_t v;std::memcpy(&v,bytes+y*pitch+x*2,2);auto c=rgb565(v);std::memcpy(dst,c.data(),4);}
        else{auto* src=bytes+y*pitch+x*4;dst[0]=src[2];dst[1]=src[1];dst[2]=src[0];dst[3]=format==Format::BGRA8?src[3]:255;}}
    }else for(unsigned by=0;by<(height+3)/4;++by)for(unsigned bx=0;bx<(width+3)/4;++bx){
        auto* block=bytes+by*pitch+bx*(format==Format::BC1?8:16);auto* color=block+(format==Format::BC1?0:8);
        std::uint16_t c0,c1;std::uint32_t selectors;std::memcpy(&c0,color,2);std::memcpy(&c1,color+2,2);std::memcpy(&selectors,color+4,4);
        std::array<std::uint8_t,4> colors[4]={rgb565(c0),rgb565(c1),{}, {}};bool four=c0>c1||format!=Format::BC1;
        for(unsigned c=0;c<3;++c){colors[2][c]=std::uint8_t(four?(2u*colors[0][c]+colors[1][c])/3:(unsigned(colors[0][c])+colors[1][c])/2);colors[3][c]=four?std::uint8_t((unsigned(colors[0][c])+2u*colors[1][c])/3):0;}
        colors[2][3]=255;colors[3][3]=four?255:0;
        std::uint8_t alpha[8]={block[0],block[1]};std::uint64_t alphaBits=0;
        if(format==Format::BC3){for(unsigned j=0;j<6;++j)alphaBits|=std::uint64_t(block[2+j])<<(8*j);
            if(alpha[0]>alpha[1])for(unsigned j=1;j<=6;++j)alpha[j+1]=std::uint8_t(((7-j)*unsigned(alpha[0])+j*unsigned(alpha[1]))/7);
            else{for(unsigned j=1;j<=4;++j)alpha[j+1]=std::uint8_t(((5-j)*unsigned(alpha[0])+j*unsigned(alpha[1]))/5);alpha[6]=0;alpha[7]=255;}}
        for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){unsigned px=bx*4+x,py=by*4+y,i=y*4+x;if(px>=width||py>=height)continue;
            auto value=colors[(selectors>>(2*i))&3];if(format==Format::BC2)value[3]=std::uint8_t(((block[i/2]>>((i&1)*4))&15)*17);
            else if(format==Format::BC3)value[3]=alpha[(alphaBits>>(3*i))&7];std::memcpy(rgba.data()+(py*width+px)*4,value.data(),4);}
    }
    output=std::move(rgba);return true;
}
} // namespace NorthlightActorTexture
