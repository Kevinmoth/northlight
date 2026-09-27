#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "patch_shadow_shader.h"

// Water mask variant of an original liquid shader, built at shader creation from
// the bytecode the game supplies (0.3.161; the DLL carries no shader bytes).
// A word-for-word port of patch() in renderer/water_shader_patch.py, which
// stays the reference oracle: the position output (VS) or oC0 (PS) is renamed
// to a spare temporary, two MOVs are appended before END and one DCL follows
// the last original DCL. Same rejection set; tests/test_water_shader_patch.py
// cross-checks both. Pure: no D3D, no logging. The caller must also match the
// result against the generated patched hash (water_shader_identities.h).
namespace NorthlightWaterShaderPatch {
using Word=std::uint32_t;
using NorthlightShadowShader::regType;
using NorthlightShadowShader::regIndex;
using NorthlightShadowShader::replaceReg;
inline int arity(unsigned op,unsigned major) {
    if(op==37)return major==3?2:4;
    switch(op) {
    case 0:case 29:return 0;
    case 65:return 1;
    case 1:case 6:case 7:case 14:case 15:case 16:case 19:case 27:case 35:case 36:case 46:case 78:case 79:case 91:case 92:return 2;
    case 2:case 3:case 5:case 8:case 9:case 10:case 11:case 12:case 13:case 17:case 20:case 21:case 22:case 23:case 24:
    case 32:case 33:case 66:case 89:case 94:case 95:return 3;
    case 4:case 18:case 34:case 88:case 90:return 4;
    case 93:return 5;
    default:return -1;
    }
}
inline Word dst(unsigned type,unsigned index,unsigned mask=15) {return replaceReg(0x80000000u|(mask<<16),type,index);}
inline Word src(unsigned type,unsigned index,unsigned swizzle=0xe4) {return replaceReg(0x80000000u|(swizzle<<16),type,index);}
inline std::uint64_t fnv1a64(const Word* words,std::size_t count) {
    const auto* bytes=reinterpret_cast<const unsigned char*>(words);
    std::uint64_t h=14695981039346656037ULL;
    for(std::size_t i=0;i<count*4;++i)h=(h^bytes[i])*1099511628211ULL;
    return h;
}
// Returns nullptr and fills output on success, else the Python ValueError text.
inline const char* patch(const Word* w,std::size_t count,std::vector<Word>& output) {
    if(!w||!count)return "empty";
    if(w[0]!=0xfffe0200u&&w[0]!=0xfffe0300u&&w[0]!=0xffff0200u&&w[0]!=0xffff0300u)return "unsupported shader model";
    struct Op {std::size_t p;unsigned op;std::size_t n;};
    std::vector<Op> ops;std::size_t end=0;
    for(std::size_t p=1;;) {
        if(p>=count)return "missing END";
        const unsigned op=w[p]&65535;
        if(op==65535) {
            if(w[p]!=65535||p+1!=count)return "bad END";
            end=p;break;
        }
        const std::size_t n=op==65534?(w[p]>>16)&32767:(w[p]>>24)&15;
        if(p+n>=count)return "truncated";
        ops.push_back({p,op,n});p+=n+1;
    }
    const unsigned major=(w[0]>>8)&255;const bool vertex=(w[0]>>16)==65534;
    // Only indices below 32 decide the spare temporary and varying.
    std::uint64_t used[32]={};
    auto use=[&](unsigned type,unsigned index){if(index<64)used[type]|=std::uint64_t(1)<<index;};
    auto isUsed=[&](unsigned type,unsigned index){return index<64&&(used[type]>>index&1);};
    bool havePos=vertex?major==2:true;
    unsigned posType=vertex?4:8,posIndex=0;
    for(const Op& o:ops)
        if(o.op==31&&vertex&&major==3) {
            if(o.n<1)return "bad DCL";
            if((w[o.p+1]&15)==0) {
                if(o.n<2)return "bad DCL";
                posType=regType(w[o.p+2]);posIndex=regIndex(w[o.p+2]);havePos=true;
            }
        }
    if(!havePos)return "no position";
    std::vector<std::size_t> params;
    int depth=0;unsigned mask=0;std::size_t declEnd=1;
    for(const Op& o:ops) {
        const std::size_t p=o.p;const Word* a=w+p+1;
        if(o.op==65534)continue;
        if(w[p]&0xf0000000u)return "predicated/coissued";
        if(o.op==31) {
            if(o.n!=2)return "bad DCL";
            if((a[0]&15)==5&&((a[0]>>16)&15)==7)return "TEXCOORD7 occupied";
            use(regType(a[1]),regIndex(a[1]));declEnd=p+3;continue;
        }
        if(o.op==81||o.op==48||o.op==47)continue;
        const int formal=arity(o.op,major);
        if(formal<0)return "unsupported control/opcode";
        if(o.op==27)++depth;
        if(o.op==29&&--depth<0)return "unbalanced LOOP";
        std::size_t q=0;
        for(int operand=0;operand<formal;++operand) {
            if(q>=o.n)return "operand count";
            const Word t=a[q];const unsigned k=regType(t),i=regIndex(t);
            if(!(t&0x80000000u))return "parameter marker";
            use(k,i);params.push_back(p+1+q);
            if(operand==0&&o.op!=27&&o.op!=65) {
                if(k==posType&&i==posIndex) {
                    if(depth)return "conditional output";
                    mask|=(t>>16)&15;
                } else if(!vertex&&(k==8||k==9))return "MRT/depth write";
                if(t&0x2000)return "relative dest";
            }
            if(operand==2&&o.op>=20&&o.op<=24) {
                const unsigned rows=o.op==20||o.op==22?4:o.op==24?2:3;
                for(unsigned r=0;r<rows;++r)use(k,i+r);
            }
            ++q;
            if(t&0x2000) {
                if(q>=o.n)return "bad relative";
                const unsigned rk=regType(a[q]);
                if(rk!=3&&rk!=15)return "bad relative";
                use(rk,regIndex(a[q]));++q;
            }
        }
        if(q!=o.n)return "extra operands";
    }
    if(depth||mask!=15)return "incomplete output";
    const unsigned limit=major==3?32:12;
    unsigned spare=0;while(spare<limit&&isUsed(0,spare))++spare;
    if(spare==limit)return "no free temporary";
    unsigned kind=6,tex=7;
    if(vertex) {
        if(major!=2) {tex=0;while(tex<12&&isUsed(6,tex))++tex;if(tex==12)return "no free varying";}
    } else {
        kind=major==2?3:1;
        if(major!=2) {tex=0;while(tex<10&&isUsed(1,tex))++tex;if(tex==10)return "no free varying";}
    }
    if(isUsed(kind,tex))return "no free varying";
    std::vector<Word> out(w,w+count);
    for(std::size_t p:params)
        if(regType(out[p])==posType&&regIndex(out[p])==posIndex)out[p]=replaceReg(out[p],0,spare);
    const Word suffix[]={0x02000001,dst(posType,posIndex),src(0,spare,vertex?0xe4:0xff),0x02000001,
        vertex?dst(kind,tex):dst(8,0,1),vertex?src(0,spare):src(kind,tex,0xff),0x0000ffff};
    out.erase(out.begin()+std::ptrdiff_t(end));
    out.insert(out.end(),suffix,suffix+7);
    if(major==3||!vertex) {
        const Word decl[]={0x0200001f,major==3?0x80070005u:0x80000000u,dst(kind,tex)};
        out.insert(out.begin()+std::ptrdiff_t(declEnd),decl,decl+3);
    }
    output.swap(out);return nullptr;
}
} // namespace NorthlightWaterShaderPatch
