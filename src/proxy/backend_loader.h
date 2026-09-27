#pragma once
// Guarded loading of the real D3D9 runtime behind the game-folder d3d9.dll
// proxy. Platform access goes through a Sys adapter (Win32 in renderer.cpp,
// fakes in tests). Never writes files or process memory.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "backend_policy.h"
namespace NorthlightBackendLoader {
// Compact SHA-256 for the read-only identity log lines (backend, game d3d9.dll).
struct Sha256 {
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    unsigned char block[64]={};uint64_t total=0;size_t used=0;
    static uint32_t rotr(uint32_t x,int n){return (x>>n)|(x<<(32-n));}
    void compress(const unsigned char* p){
        static const uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        uint32_t w[64],v[8];
        for(int i=0;i<16;++i)w[i]=uint32_t(p[4*i])<<24|uint32_t(p[4*i+1])<<16|uint32_t(p[4*i+2])<<8|p[4*i+3];
        for(int i=16;i<64;++i)w[i]=w[i-16]+(rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10));
        std::memcpy(v,h,sizeof v);
        for(int i=0;i<64;++i){
            const uint32_t t1=v[7]+(rotr(v[4],6)^rotr(v[4],11)^rotr(v[4],25))+((v[4]&v[5])^(~v[4]&v[6]))+k[i]+w[i];
            const uint32_t t2=(rotr(v[0],2)^rotr(v[0],13)^rotr(v[0],22))+((v[0]&v[1])^(v[0]&v[2])^(v[1]&v[2]));
            std::memmove(v+1,v,7*sizeof(uint32_t));v[4]+=t1;v[0]=t1+t2;
        }
        for(int i=0;i<8;++i)h[i]+=v[i];
    }
    void update(const void* data,size_t n){
        const unsigned char* p=static_cast<const unsigned char*>(data);total+=n;
        while(n){const size_t take=64-used<n?64-used:n;std::memcpy(block+used,p,take);used+=take;p+=take;n-=take;if(used==64){compress(block);used=0;}}
    }
    std::string hex(){
        const uint64_t bits=total*8;const unsigned char one=0x80,zero=0;update(&one,1);
        while(used!=56)update(&zero,1);
        unsigned char length[8];for(int i=0;i<8;++i)length[i]=(unsigned char)(bits>>(56-8*i));update(length,8);
        static const char digits[]="0123456789abcdef";std::string out;
        for(uint32_t x:h)for(int s=28;s>=0;s-=4)out+=digits[(x>>s)&15];
        return out;
    }
};
struct Inspection { bool read=false,ours=false,dxvk=false,dxvkConfigEnv=false; std::string dxvkVersion,sha256; };
inline size_t find(const unsigned char* d,size_t n,const void* s,size_t m,size_t from=0){
    if(!m||m>n)return SIZE_MAX;
    for(size_t i=from;i+m<=n;++i)if(d[i]==*static_cast<const unsigned char*>(s)&&!std::memcmp(d+i,s,m))return i;
    return SIZE_MAX;
}
// Byte scan of the candidate file (no code runs). Every build of ours logs
// "Northlight renderer <version>;", and every proxy build also carries its
// PROXY log format, which holds no product name. DXVK logs "DXVK: \0v<version>" and names itself
// in the version resource; DXVK_CONFIG (env) exists from DXVK 2.x, not in 1.10.x.
inline Inspection inspect(const unsigned char* d,size_t n){
    Inspection r;r.read=true;
    // Our builds also carry these DXVK needles: they are never reported as DXVK.
    static const char banner[]="Northlight renderer ",proxy[]="PROXY module=%ls root=%ls";
    if((r.ours=find(d,n,banner,sizeof banner-1)!=SIZE_MAX||find(d,n,proxy,sizeof proxy-1)!=SIZE_MAX))return r;
    for(size_t at=find(d,n,"DXVK: \0",7);at!=SIZE_MAX&&r.dxvkVersion.empty();at=find(d,n,"DXVK: \0",7,at+1)){
        size_t v=at+7;while(v<n&&v<at+16&&!d[v])++v;
        if(v<n&&d[v]=='v')for(;v<n&&d[v]>=0x20&&d[v]<0x7f&&r.dxvkVersion.size()<64;++v)r.dxvkVersion+=char(d[v]);
    }
    static const char product[]="P\0r\0o\0d\0u\0c\0t\0N\0a\0m\0e\0\0\0",dxvk[]="D\0X\0V\0K\0\0\0";
    for(size_t at=find(d,n,product,sizeof product-1);at!=SIZE_MAX&&!r.dxvk;at=find(d,n,product,sizeof product-1,at+1)){
        const size_t v=at+sizeof product-1,e=v+16<n?v+16:n;   // value follows 32-bit padding
        r.dxvk=v<e&&find(d+v,e-v,dxvk,sizeof dxvk-1)!=SIZE_MAX;
    }
    r.dxvk=r.dxvk||!r.dxvkVersion.empty();
    r.dxvkConfigEnv=r.dxvk&&find(d,n,"DXVK_CONFIG\0",12)!=SIZE_MAX;
    return r;
}
enum class Outcome { Loaded, Missing, SelfPath, SelfFile, OwnBuild, LoadFailed, SelfModule, NoFactory, FactoryInSelf, D3d9Name };
inline const char* name(Outcome o){
    switch(o){case Outcome::Loaded:return "loaded";case Outcome::Missing:return "missing";
    case Outcome::SelfPath:return "refused-self-path";case Outcome::SelfFile:return "refused-self-file";
    case Outcome::OwnBuild:return "refused-own-build";case Outcome::LoadFailed:return "load-failed";
    case Outcome::SelfModule:return "refused-self-module";case Outcome::NoFactory:return "no-Direct3DCreate9";
    case Outcome::D3d9Name:return "refused-second-d3d9.dll-name";
    default:return "refused-factory-in-self";}
}
// Loading this refusal again can only recurse into the proxy: use the system runtime.
inline bool selfRelated(Outcome o){
    return o==Outcome::SelfPath||o==Outcome::SelfFile||o==Outcome::OwnBuild||o==Outcome::SelfModule||o==Outcome::FactoryInSelf;
}
struct Attempt { std::wstring path; Outcome outcome=Outcome::LoadFailed; unsigned long error=0; Inspection info; };
template<class Module> struct Result { Module module{}; std::vector<Attempt> attempts; bool fallback=false; };
// Sys: self() full path of this module; full(path) ("" = error); identity(path,error)
// -1 missing/unopenable, 1 same file as self (hard link, 8.3, junction), 0 other;
// read(path,bytes); beforeLoad(path,info); load(path,error); isSelf(m);
// proc(m,name); ownedBySelf(address) (forwarded export into the proxy); release(m).
// A backend named d3d9.dll besides the proxy is refused (fail loudly): only the
// system runtime (Backend=native or the fallback) may use that name.
template<class Sys> typename Sys::Module attempt(Sys& sys,const std::wstring& path,Attempt& a,bool allowD3d9Name=true){
    a=Attempt{};a.path=sys.full(path);
    if(a.path.empty()){a.path=path;a.outcome=Outcome::LoadFailed;return {};}
    if(NorthlightBackend::samePath(a.path,sys.self())){a.outcome=Outcome::SelfPath;return {};}
    if(!allowD3d9Name&&NorthlightBackend::d3d9Name(a.path)){a.outcome=Outcome::D3d9Name;return {};}
    const int id=sys.identity(a.path,a.error);
    if(id<0){a.outcome=Outcome::Missing;return {};}
    if(id>0){a.outcome=Outcome::SelfFile;return {};}
    std::vector<unsigned char> bytes;
    if(sys.read(a.path,bytes)){a.info=inspect(bytes.data(),bytes.size());Sha256 h;h.update(bytes.data(),bytes.size());a.info.sha256=h.hex();}
    if(a.info.ours){a.outcome=Outcome::OwnBuild;return {};}
    sys.beforeLoad(a.path,a.info);
    auto m=sys.load(a.path,a.error);
    if(!m){a.outcome=Outcome::LoadFailed;return {};}
    a.error=0;
    if(sys.isSelf(m)){sys.release(m);a.outcome=Outcome::SelfModule;return {};}
    void* factory=sys.proc(m,"Direct3DCreate9");
    if(!factory){sys.release(m);a.outcome=Outcome::NoFactory;return {};}
    if(sys.ownedBySelf(factory)){sys.release(m);a.outcome=Outcome::FactoryInSelf;return {};}
    a.outcome=Outcome::Loaded;return m;
}
// Missing candidates advance to the next one. A missing or broken configured
// backend is an error (no silent quality change); only a candidate that would
// be this proxy falls back, once, to the system runtime.
template<class Sys> Result<typename Sys::Module> load(Sys& sys,const std::vector<std::wstring>& candidates,const std::wstring& systemPath){
    Result<typename Sys::Module> r;bool self=false;
    for(size_t i=0;i<candidates.size();++i){
        const bool system=!systemPath.empty()&&NorthlightBackend::samePath(sys.full(candidates[i]),sys.full(systemPath));
        Attempt a;auto m=attempt(sys,candidates[i],a,system);r.attempts.push_back(a);
        if(m){r.module=m;return r;}
        if(a.outcome==Outcome::Missing&&i+1<candidates.size())continue;
        self=selfRelated(a.outcome);break;
    }
    if(!self||systemPath.empty())return r;
    for(const auto& a:r.attempts)if(NorthlightBackend::samePath(a.path,sys.full(systemPath)))return r;
    r.fallback=true;Attempt a;r.module=attempt(sys,systemPath,a);r.attempts.push_back(a);
    return r;
}
// Per-thread export nesting. Depth > 1 means the backend (a proxy that
// resolved "d3d9.dll" by name) called back into this module.
struct ExportScope {
    static int& depth(){static thread_local int value=0;return value;}
    ExportScope(){++depth();}
    ~ExportScope(){--depth();}
    ExportScope(const ExportScope&)=delete;ExportScope& operator=(const ExportScope&)=delete;
    static bool reentered(){return depth()>1;}
};
}
