#include "celestial_glare_native.h"
#include <cassert>
#include <unordered_map>
#include <iostream>
int main(){
    using namespace NorthlightCelestialGlare;
    std::unordered_map<std::uintptr_t,unsigned char> memory;
    auto put=[&](std::uintptr_t address,std::uint32_t value){for(unsigned i=0;i<4;++i)memory[address+i]=static_cast<unsigned char>(value>>(8*i));};
    auto read=[&](std::uintptr_t address,void* output,std::size_t count){for(std::size_t i=0;i<count;++i){auto p=memory.find(address+i);if(p==memory.end())return false;static_cast<unsigned char*>(output)[i]=p->second;}return true;};
    auto record=[&](unsigned body){unsigned handle=0x20000+body*0x1000,gx=0x30000+body*0x1000;
        put((body?0xd38f58:0xd38ea8)+0x1c,handle);put((body?0xd38f58:0xd38ea8)+0xa8,body?0xd38e48:0xd38e28);
        for(unsigned i=0;i<0x40;++i)memory[handle+0x20+i]=0;
        put(handle+0x44,gx);put(gx+0x38,0x40000+body*0x1000);
    };
    record(0);record(1);auto initial=readIdentities(read);assert(initial.valid==3);
    assert(claim(initial,initial,0x41000,2)==1&&claim(initial,initial,0x40000,1)==0);
    assert(claim(initial,initial,0x41000,0)==-1&&claim(initial,initial,0x41000,1)==-1);
    for(unsigned ptr=0x50000;ptr<0x60000;ptr+=4)assert(claim(initial,initial,ptr,3)==-1);
    // Streaming pointer reuse, null/lazy allocation, wrong owner, torn records.
    put(0x31038,0x42000);auto changed=readIdentities(read);assert(claim(initial,changed,0x41000,2)==-1);assert(claim(initial,changed,0x42000,2)==-1);
    put(0xd39000,0xd38e68);assert(!(readIdentities(read).valid&2));record(1);
    put(0x21044,0);auto lazy=readIdentities(read);assert(lazy.valid==3&&!lazy.texture[1]);assert(claim(initial,lazy,0x41000,2)==-1);record(1);
    put(0x21028,4);put(0x2105c,0x50000);put(0x50018,0x51000);put(0x51038,0x52000);auto variant=readIdentities(read);assert(variant.texture[1]==0x52000);record(1);
    unsigned reads=0;auto torn=[&](std::uintptr_t address,void* out,std::size_t size){if(address==0x31038&&++reads==2)put(address,0x42000);return read(address,out,size);};
    assert(!(readIdentities(torn).valid&2));record(1);
    auto ambiguous=initial;ambiguous.texture[1]=ambiguous.texture[0];assert(claim(ambiguous,ambiguous,ambiguous.texture[0],3)==-1);
    // Every audited byte is essential to the runtime gate.
    std::size_t n=0;auto* common=NorthlightCelestialDisc::identitySignatures(n);
    for(std::size_t i=0;i<n;++i)for(std::size_t k=0;k<common[i].size;++k)memory[common[i].address+k]=common[i].bytes[k];
    auto* table=signatures(n);for(std::size_t i=0;i<n;++i)for(std::size_t k=0;k<table[i].size;++k)memory[table[i].address+k]=table[i].bytes[k];
    assert(verify(read));unsigned mutations=0;
    for(std::size_t i=0;i<n;++i)for(std::size_t k=0;k<table[i].size;++k){memory[table[i].address+k]^=1;assert(!verify(read));memory[table[i].address+k]^=1;++mutations;}
    std::cout<<"PASS glare: 4096 unrelated textures retained; ownership, off/no-replacement, streaming, wrong body, lazy/variant/torn reads; "<<mutations<<" signature byte mutations rejected\n";
}
