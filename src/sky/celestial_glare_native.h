#pragma once
#include "celestial_disc_native.h"
// Audited separate native sunGlare/moonGlare records. Their positions follow
// the UNWARPED body (moon +0x0c = d38f64), even after we replace its disc.
// Do not suppress arbitrary additive particles or change client memory.
namespace NorthlightCelestialGlare {
using NorthlightCelestialDisc::IdentitySignature;
inline const IdentitySignature* signatures(std::size_t& count){
    static const unsigned char sunInit[]={0x68,0xc4,0x1b,0xa4,0x00,0xb9,0xa8,0x8e,0xd3,0x00,0xe8,0xa1,0xee,0x1b,0x00};
    static const unsigned char moonInit[]={0x68,0xec,0x1b,0xa4,0x00,0xb9,0x58,0x8f,0xd3,0x00,0xe8,0xc1,0xed,0x1b,0x00};
    static const unsigned char handleStore[]={0x89,0x46,0x1c};
    static const unsigned char handleDraw[]={0x8b,0x46,0x1c,0x53,0x6a,0x00,0x6a,0x00,0x50,0xe8,0x75,0xa8,0xb0,0xff};
    static const unsigned char sunBody[]={0xc7,0x05,0x50,0x8f,0xd3,0x00,0x28,0x8e,0xd3,0x00};
    static const unsigned char moonBody[]={0xc7,0x05,0x00,0x90,0xd3,0x00,0x48,0x8e,0xd3,0x00};
    static const IdentitySignature table[]={
        {0x7ee150,sunInit,sizeof sunInit},{0x7ee230,moonInit,sizeof moonInit},
        {0x9ad060,handleStore,sizeof handleStore},{0x9ac42d,handleDraw,sizeof handleDraw},
        {0x7ee172,sunBody,sizeof sunBody},{0x7ee256,moonBody,sizeof moonBody}};
    count=sizeof table/sizeof table[0];return table;
}
template<class Read>inline bool verify(Read read){
    if(!NorthlightCelestialDisc::verifyIdentityCode(read))return false;
    std::size_t n=0;const auto* table=signatures(n);unsigned char actual[32];
    for(std::size_t i=0;i<n;++i)if(!read(table[i].address,actual,table[i].size)||std::memcmp(actual,table[i].bytes,table[i].size))return false;
    return true;
}
template<class Read>inline NorthlightCelestialDisc::Identities readIdentities(Read read){
    NorthlightCelestialDisc::Identities result;
    for(unsigned body=0;body<2;++body){
        const std::uint32_t record=body?0xd38f58:0xd38ea8,expectedBody=body?0xd38e48:0xd38e28;
        std::uint32_t linkA=0,linkB=0,a=0,b=0;
        if(read(record+0xa8,&linkA,4)&&linkA==expectedBody&&
           NorthlightCelestialDisc::readTextureIdentityAt(read,record+0x1c,a)&&
           NorthlightCelestialDisc::readTextureIdentityAt(read,record+0x1c,b)&&a==b&&
           read(record+0xa8,&linkB,4)&&linkA==linkB){result.valid|=1u<<body;result.texture[body]=a;}
    }return result;
}
// Only own a native glare if this frame successfully replaced the corresponding
// body, including a validated placement outside the viewport. Ambiguous shared identities and changing records fail open.
inline int claim(const NorthlightCelestialDisc::Identities& start,const NorthlightCelestialDisc::Identities& now,
                 std::uintptr_t bound,unsigned replacedBodies){
    if(!bound)return -1;
    if(start.texture[0]&&start.texture[0]==start.texture[1])return -1;
    for(unsigned body=0;body<2;++body)if((replacedBodies&start.valid&now.valid&(1u<<body))&&
        start.texture[body]==bound&&now.texture[body]==bound)return int(body);
    return -1;
}
}
