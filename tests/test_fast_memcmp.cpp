// 0.3.181 (r90): NorthlightMem::compare, the DLL's memcmp, against an independent byte loop. Exact int
// values (not signs), both argument orders. Built native arm64 (the word path) and x86_64 under Rosetta
// (the SSE2 path), at -O2 and with ASan+UBSan; see test_fast_memcmp.py.
#include "northlight_mem.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

static int reference(const unsigned char* a,const unsigned char* b,std::size_t n){
    for(std::size_t i=0;i<n;++i)if(a[i]!=b[i])return int(a[i])-int(b[i]);
    return 0;
}
static unsigned long long cases=0;
static void check(const unsigned char* a,const unsigned char* b,std::size_t n){
    const int x=NorthlightMem::compare(a,b,n),y=NorthlightMem::compare(b,a,n);
    if(x!=reference(a,b,n)||y!=reference(b,a,n)){
        std::fprintf(stderr,"MISMATCH n=%zu got %d/%d want %d/%d\n",n,x,y,reference(a,b,n),reference(b,a,n));std::abort();}
    assert(NorthlightMem::byteCompare(a,b,n)==x);
    ++cases;
}
static const unsigned char pairs[][2]={{0x00,0xFF},{0xFF,0x00},{0x7F,0x80},{0x80,0x7F},{0x01,0x02},{0x00,0x80},{0xFE,0xFF}};
// Random n in 0..8192 at every offset 0..63 of each pointer; the first difference swept over 0, n-1,
// within 16/32 of both ends and random positions; single and multiple differences.
static void differential(){
    std::mt19937 random(90);
    static unsigned char left[8192+128],right[8192+128];
    for(unsigned round=0;round<600;++round){
        const std::size_t n=round<64?round:random()%8193,oa=random()%64,ob=round%64;
        unsigned char* a=left+oa;unsigned char* b=right+ob;
        for(std::size_t i=0;i<n;++i)a[i]=b[i]=static_cast<unsigned char>(random());
        check(a,b,n);
        if(!n)continue;
        std::size_t positions[]={0,n-1,n/2,std::size_t(random()%n),n>16?n-16:0,n>17?n-17:0,n>32?n-32:0,n>33?n-33:0,std::min<std::size_t>(15,n-1),std::min<std::size_t>(16,n-1),std::min<std::size_t>(31,n-1),std::min<std::size_t>(32,n-1)};
        for(std::size_t at:positions)for(const auto& pair:pairs){
            const unsigned char keepA=a[at],keepB=b[at];a[at]=pair[0];b[at]=pair[1];check(a,b,n);
            if(at+1<n){const std::size_t later=at+1+random()%(n-at-1);const unsigned char lA=a[later];a[later]^=0x5a;check(a,b,n);a[later]=lA;}
            a[at]=keepA;b[at]=keepB;
        }
    }
}
// Every n <= 80, every difference position, every signedness pair.
static void exhaustive(){
    unsigned char a[96],b[96];
    for(std::size_t n=0;n<=80;++n)for(std::size_t offset=0;offset<4;++offset){
        for(std::size_t i=0;i<n;++i)a[offset+i]=b[offset+i]=static_cast<unsigned char>(i*37+offset);
        check(a+offset,b+offset,n);
        for(std::size_t at=0;at<n;++at)for(const auto& pair:pairs){
            const unsigned char x=a[offset+at],y=b[offset+at];a[offset+at]=pair[0];b[offset+at]=pair[1];check(a+offset,b+offset,n);a[offset+at]=x;b[offset+at]=y;}
    }
}
// Buffers flush against a PROT_NONE page after their end, and starting at a page start with a PROT_NONE
// page before: any over- or under-read faults.
static void guards(){
    const std::size_t page=std::size_t(sysconf(_SC_PAGESIZE)),span=4*page;
    auto region=[&](){auto* base=static_cast<unsigned char*>(mmap(nullptr,span+2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0));assert(base!=MAP_FAILED);
        assert(!mprotect(base,page,PROT_NONE)&&!mprotect(base+page+span,page,PROT_NONE));return base+page;};
    unsigned char* x=region();unsigned char* y=region();
    for(std::size_t i=0;i<span;++i)x[i]=y[i]=static_cast<unsigned char>(i*13);
    for(std::size_t n=0;n<=200;++n){
        unsigned char* ea=x+span-n;unsigned char* eb=y+span-n; /* flush against the end */
        check(ea,eb,n);check(x,y,n);                             /* at the start */
        for(std::size_t at=0;at<n;++at){ea[at]^=0x80;check(ea,eb,n);ea[at]^=0x80;x[at]^=0x01;check(x,y,n);x[at]^=0x01;}
    }
    for(std::size_t n:{std::size_t(1000),std::size_t(4095),std::size_t(4096),span}){check(x+span-n,y+span-n,n);check(x,y,n);}
}
int main(int argc,char** argv){
    const std::string mode=argc>1?argv[1]:"all";
    if(mode=="guard"){guards();std::printf("guard cases=%llu\n",cases);return 0;}
    guards();exhaustive();differential();
#if defined(__SSE2__)
    const char* path="sse2";
#else
    const char* path="word";
#endif
    std::printf("PASS fast memcmp (%s path): %llu cases equal to the byte loop in both argument orders, guard pages clean\n",path,cases);
}
