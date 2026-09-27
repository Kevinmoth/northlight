// Native fake-D3D test. No game, Wine, GPU device, or graphics process starts.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <cstdio>
using HRESULT=int32_t;using UINT=uint32_t;using DWORD=uint32_t;
constexpr HRESULT S_OK=0,S_FALSE=1,E_FAIL=-1,E_OUTOFMEMORY=-2;
#define SUCCEEDED(x) ((x)>=0)
#define FAILED(x) ((x)<0)
constexpr DWORD D3DUSAGE_DYNAMIC=1,D3DUSAGE_WRITEONLY=2,D3DLOCK_DISCARD=1,D3DLOCK_NOOVERWRITE=2;
constexpr unsigned D3DPOOL_DEFAULT=0,D3DQUERYTYPE_EVENT=0,D3DISSUE_END=1;
struct Driver {
    uint64_t submitted=0,completed=0;unsigned locks=0,discards=0,queries=0,waits=0;
    size_t uploaded=0;bool querySupport=true,failIssue=false,failLock=false,failUnlock=false;
};
struct IDirect3DQuery9 {
    Driver* driver;uint64_t serial=0;
    explicit IDirect3DQuery9(Driver* d):driver(d){}
    HRESULT Issue(unsigned flags){assert(flags==D3DISSUE_END);serial=driver->submitted;return driver->failIssue?E_FAIL:S_OK;}
    HRESULT GetData(void* out,UINT bytes,DWORD flags){assert(!out&&!bytes&&!flags);return driver->completed>=serial?S_OK:S_FALSE;}
    void Release(){delete this;}
};
struct IDirect3DVertexBuffer9 {
    Driver* driver;std::vector<unsigned char> data;
    struct Read {UINT first,bytes;uint64_t serial;};std::vector<Read> readers;
    IDirect3DVertexBuffer9(Driver* d,UINT bytes):driver(d),data(bytes,0xEE){}
    HRESULT Lock(UINT first,UINT bytes,void** out,DWORD flags){
        assert(size_t(first)+bytes<=data.size());assert(flags==D3DLOCK_DISCARD||flags==D3DLOCK_NOOVERWRITE);
        if(driver->failLock)return E_FAIL;
        if(flags==D3DLOCK_DISCARD){std::fill(data.begin(),data.end(),0xEE);readers.clear();++driver->discards;}
        else for(auto r:readers)assert(r.serial<=driver->completed||first+bytes<=r.first||r.first+r.bytes<=first);
        *out=data.data()+first;++driver->locks;driver->uploaded+=bytes;return S_OK;
    }
    HRESULT Unlock(){return driver->failUnlock?E_FAIL:S_OK;}
    void Release(){delete this;}
    void read(UINT first,UINT bytes){readers.push_back({first,bytes,++driver->submitted});}
};
struct IDirect3DDevice9 {
    Driver driver;
    HRESULT CreateVertexBuffer(UINT bytes,DWORD usage,UINT fvf,unsigned pool,IDirect3DVertexBuffer9** out,void*){
        assert(usage==(D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY)&&!fvf&&pool==D3DPOOL_DEFAULT);*out=new IDirect3DVertexBuffer9(&driver,bytes);return S_OK;
    }
    HRESULT CreateQuery(unsigned kind,IDirect3DQuery9** out){assert(kind==D3DQUERYTYPE_EVENT);if(!driver.querySupport)return E_FAIL;*out=new IDirect3DQuery9(&driver);++driver.queries;return S_OK;}
};
#include "live_terrain_gpu.h"
struct Position {float x,y,z;};
struct Vertex {Position position;float normal[3]={};float uv[2]={};};
struct Snapshot {std::vector<Position> positions;std::vector<uint32_t> indices;};
using Owner=std::shared_ptr<const Snapshot>;using Cache=NorthlightLiveTerrainGPU::Cache<Snapshot,Vertex>;
Owner triangle(float value){auto s=std::make_shared<Snapshot>();s->positions={{value,1,2},{value+1,2,3},{value+2,3,4}};s->indices={0,1,2};return s;}
Vertex convert(Position p){Vertex v;v.position=p;return v;}
void allIndices(const Snapshot& s,uint32_t offset,std::vector<uint32_t>& out){for(auto i:s.indices)out.push_back(i+offset);}
bool admit(size_t){return true;}
void verify(Cache& cache,const std::vector<Owner>& active,const std::vector<uint32_t>& indices){
    size_t next=0;for(const auto& s:active)for(auto i:s->indices){assert(next<indices.size());const uint32_t actual=indices[next++];assert(actual<cache.vertexCapacity());
        Vertex v;std::memcpy(&v,cache.vertices()->data.data()+size_t(actual)*sizeof(Vertex),sizeof v);const auto p=s->positions[i];assert(!std::memcmp(&v.position,&p,sizeof p));
        for(float n:v.normal)assert(n==0);for(float uv:v.uv)assert(uv==0);
    }assert(next==indices.size());
}
void draw(Cache& cache,const std::vector<uint32_t>& indices){for(auto i:indices)cache.vertices()->read(i*sizeof(Vertex),sizeof(Vertex));}
void allocator(){
    NorthlightLiveTerrainGPU::FreeRanges free;NorthlightLiveTerrainGPU::Range a,b,c,d;
    free.reset(12);assert(free.take(3,a)&&a.first==0);assert(free.take(3,b)&&b.first==3);assert(free.take(6,c)&&c.first==6);assert(!free.take(1,d));
    free.release(a);free.release(c);free.release(b);assert(free.take(12,d)&&d.first==0);free.reset(0);assert(!free.take(1,d));
}
void partialAndOrdering(){
    IDirect3DDevice9 d;Cache cache(30*sizeof(Vertex));Owner a=triangle(10),b=triangle(20),c=triangle(30);
    std::vector<Owner> active={a,b};std::vector<uint32_t> point,directional;
    cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(cache.uploadedBytes==6*sizeof(Vertex));assert(d.driver.locks==1&&d.driver.discards==1);
    assert(cache.indices(active,1,allIndices,point,directional));assert(point==directional);verify(cache,active,point);draw(cache,point);
    cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(!cache.uploadedBytes&&cache.reusedVertices==6);assert(d.driver.locks==1);
    active={c,b};cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(cache.uploadedBytes==3*sizeof(Vertex)&&cache.reusedVertices==3);
    assert(cache.indices(active,1,allIndices,point,directional));verify(cache,active,point);assert(point[0]==6&&point[3]==3);draw(cache,point);
    active={b,c};cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(!cache.uploadedBytes);
    assert(cache.indices(active,1,allIndices,point,directional));verify(cache,active,point);assert(point[0]==3&&point[3]==6);
    // Generation/filter changes alter indices only, including point originals.
    auto filter=[](const Snapshot& s,uint32_t offset,std::vector<uint32_t>& out){if(s.positions[0].x==30)allIndices(s,offset,out);};
    assert(cache.indices(active,2,filter,point,directional));assert(point.size()==6&&directional.size()==3);verify(cache,{c},directional);assert(d.driver.locks==2);
    // LOD/topology owner replacement uploads just the new immutable capture.
    auto lod=std::make_shared<Snapshot>();lod->positions=b->positions;lod->positions[1].z+=2;lod->indices={2,1,0};active={lod,c};
    cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(cache.uploadedBytes==3*sizeof(Vertex));assert(cache.indices(active,3,allIndices,point,directional));verify(cache,active,point);
    cache.clear();assert(!cache.vertices()&&!cache.bytes());cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(cache.indices(active,3,allIndices,point,directional));verify(cache,active,point);
}
void retirement(bool queries,bool issue=true){
    IDirect3DDevice9 d;d.driver.querySupport=queries;d.driver.failIssue=!issue;Cache cache(9*sizeof(Vertex));
    Owner a=triangle(1),b=triangle(2),c=triangle(3);std::vector<Owner> active={a,b};std::vector<uint32_t> p,q;
    cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(cache.indices(active,1,allIndices,p,q));draw(cache,p);
    // Expired captures enter retirement. Their NOOVERWRITE slots are unavailable
    // while the fake GPU deliberately remains busy.
    active={b,c};a.reset();cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(cache.indices(active,1,allIndices,p,q));verify(cache,active,p);assert(p[3]==6);draw(cache,p);
    d.driver.completed=d.driver.submitted;Owner e=triangle(4);active={b,e};c.reset();
    cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(cache.indices(active,1,allIndices,p,q));verify(cache,active,p);
    if(queries&&issue){assert(!cache.rollovers);assert(p[3]==0);assert(cache.uploadedBytes==3*sizeof(Vertex));}
    else{assert(cache.rollovers==1&&d.driver.discards==2);assert(cache.uploadedBytes==6*sizeof(Vertex));}
}
void busyRolloverAndFailure(){
    IDirect3DDevice9 d;Cache cache(9*sizeof(Vertex));Owner a=triangle(1),b=triangle(2),c=triangle(3),e=triangle(4);std::vector<Owner> active={a,b};std::vector<uint32_t> p,q;
    cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(cache.indices(active,1,allIndices,p,q));draw(cache,p);
    active={b,c};a.reset();cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(cache.indices(active,1,allIndices,p,q));draw(cache,p);
    active={b,e};c.reset();cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(cache.rollovers==1&&d.driver.discards==2);assert(cache.indices(active,1,allIndices,p,q));verify(cache,active,p);
    Owner f=triangle(5);active={b,f};d.driver.failLock=true;cache.beginFrame();assert(cache.update(&d,active,admit,convert)==E_FAIL);d.driver.failLock=false;
    cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(cache.uploadedBytes==6*sizeof(Vertex));assert(cache.indices(active,1,allIndices,p,q));verify(cache,active,p);
    Owner g=triangle(6);active={b,g};d.driver.failUnlock=true;cache.beginFrame();assert(cache.update(&d,active,admit,convert)==E_FAIL);d.driver.failUnlock=false;
    cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);assert(cache.indices(active,1,allIndices,p,q));verify(cache,active,p);
}
void admissionAndBounds(){
    IDirect3DDevice9 d;Cache cache(12*sizeof(Vertex));auto a=triangle(1);std::vector<Owner> active={a};
    cache.beginFrame();assert(cache.update(&d,active,[](size_t){return false;},convert)==S_FALSE);assert(!cache.vertices());
    assert(cache.update(&d,active,[](size_t n){return n<=6*sizeof(Vertex);},convert)==S_OK);assert(cache.vertexCapacity()==6);
    std::weak_ptr<const Snapshot> weak=a;active.clear();a.reset();assert(weak.expired());cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);
    Cache tiny(2*sizeof(Vertex));active={triangle(3)};tiny.beginFrame();assert(tiny.update(&d,active,admit,convert)==E_OUTOFMEMORY);assert(!tiny.vertices());
}
void longWalk(){
    IDirect3DDevice9 d;Cache cache(128*3*sizeof(Vertex));std::vector<Owner> owners;for(unsigned i=0;i<600;++i)owners.push_back(triangle(float(i*3)));
    std::vector<uint32_t> p,q;uint64_t previous=0;
    for(unsigned frame=0;frame<500;++frame){
        // Keep old CPU owners alive: reclamation must still work under arena
        // pressure, not depend on the terrain-capture cache evicting them.
        std::vector<Owner> active(owners.begin()+frame,owners.begin()+frame+64);
        d.driver.completed=previous;previous=d.driver.submitted;
        cache.beginFrame();assert(cache.update(&d,active,admit,convert)==S_OK);
        assert(cache.uploadedBytes==(frame?3:64*3)*sizeof(Vertex));
        assert(cache.indices(active,1,allIndices,p,q));verify(cache,active,p);draw(cache,p);
        assert(!cache.rollovers&&cache.bytes()==128*3*sizeof(Vertex));
    }
    assert(d.driver.discards==1&&d.driver.queries>0);
}
int main(){allocator();partialAndOrdering();retirement(true);retirement(false);retirement(true,false);busyRolloverAndFailure();admissionAndBounds();longWalk();std::puts("live terrain GPU: geometry/order/LOD, incremental writes, fence-safe reuse, 500-frame bounded walk, busy/unsupported rollover, pressure/failure/reset passed");}
