#pragma once
#include "draw_snapshot.h"

namespace NorthlightReplayGPU {
// Immutable CPU snapshots identify GPU contents, never an original mutable VB.
// Weak owners prevent the GPU cache from keeping evicted CPU scenes alive.
class Cache {
public:
    struct Stats {
        unsigned countPressure=0,bytePressure=0,roomRejected=0,promotionCountOnly=0,promotionCountRejected=0,evictions=0;
        unsigned warmup=0,uploadDeferred=0,admissionRejected=0;
        unsigned scanPasses=0,scanVisits=0,scanMemoHits=0;
        // Exclusive reasons: the original attempt-limit check takes priority.
        unsigned attemptDeferred=0,uploadByteDeferred=0,expiredEntries=0;
        size_t expiredBytes=0;
    };
    struct Population {size_t entries=0,resident=0,probation=0;};
    // Lifetime totals survive beginFrame, including clear-before-beginFrame in
    // the caller's recursive bulk-upload fallback. Caller supplies the reason.
    struct ClearStats {uint64_t calls=0,entries=0,bytes=0;};
private:
    struct Entry {
        std::weak_ptr<const NorthlightDrawSnapshot::Mesh> owner;
        IDirect3DVertexBuffer9* vertices[4]={};IDirect3DIndexBuffer9* indices=nullptr;
        size_t bytes=0;uint64_t first=0,touched=0;
        ~Entry(){for(auto p:vertices)if(p)p->Release();if(indices)indices->Release();}
    };
    std::unordered_map<const NorthlightDrawSnapshot::Mesh*,std::unique_ptr<Entry>> entries;
    size_t bytes_=0;uint64_t frame_=0;
    size_t uploaded_=0,reused_=0;unsigned hits_=0,created_=0;size_t attemptedBytes_=0;unsigned attempts_=0;
    Stats stats_;
    ClearStats clearStats_;
    static constexpr size_t EntryLimit=2048,Limit=64u*1024u*1024u,UploadLimit=4u*1024u*1024u;
    bool noEvictable_=false;
    bool room(size_t bytes,bool promotion=false){
        const bool atCountLimit=entries.size()>=EntryLimit;
        // Promotion replaces an existing probation record and needs no slot.
        const bool countPressure=!promotion&&atCountLimit;
        const bool bytePressure=bytes_+bytes>Limit;
        stats_.countPressure+=countPressure;stats_.bytePressure+=bytePressure;
        stats_.promotionCountOnly+=promotion&&atCountLimit&&!bytePressure;
        while(bytes_+bytes>Limit||(!promotion&&entries.size()>=EntryLimit)){
            // Only a COMPLETE failed scan establishes this frame-local proof.
            // All surviving entries were touched in this frame; later inserts
            // and promotions are also current, and removals cannot invalidate
            // that fact. Always check capacity first: removals or room(0) may
            // permit progress even while the proof remains true.
            if(noEvictable_){++stats_.scanMemoHits;++stats_.roomRejected;stats_.promotionCountRejected+=promotion&&atCountLimit;return false;}
            auto oldest=entries.end();
            ++stats_.scanPasses;
            for(auto it=entries.begin();it!=entries.end();++it){
                ++stats_.scanVisits;
                if(it->second->touched!=frame_&&(oldest==entries.end()||it->second->touched<oldest->second->touched))oldest=it;}
            if(oldest==entries.end()){noEvictable_=true;++stats_.roomRejected;stats_.promotionCountRejected+=promotion&&atCountLimit;return false;}
            ++stats_.evictions;
            bytes_-=oldest->second->bytes;entries.erase(oldest);
        }return true;
    }
public:
    void clear(){++clearStats_.calls;clearStats_.entries+=entries.size();clearStats_.bytes+=bytes_;entries.clear();bytes_=0;noEvictable_=false;}
    void beginFrame(){
        ++frame_;uploaded_=reused_=0;hits_=created_=attempts_=0;attemptedBytes_=0;stats_={};noEvictable_=false;
        for(auto it=entries.begin();it!=entries.end();){
            if(it->second->owner.expired()){++stats_.expiredEntries;stats_.expiredBytes+=it->second->bytes;bytes_-=it->second->bytes;it=entries.erase(it);}else ++it;
        }
    }
    size_t bytes()const{return bytes_;}size_t uploaded()const{return uploaded_;}
    size_t reused()const{return reused_;}unsigned hits()const{return hits_;}
    static constexpr size_t entryLimit(){return EntryLimit;}
    const Stats& stats()const{return stats_;}
    const ClearStats& clearStats()const{return clearStats_;}
    Population population()const{Population p;p.entries=entries.size();for(const auto& item:entries)p.resident+=item.second->bytes!=0;p.probation=p.entries-p.resident;return p;}
    // Allocation/driver failures are optional-cache misses. Caller retains its
    // original bulk upload path and all casters. Partial allocations are freed.
    template<class Admission> bool bind(IDirect3DDevice9* device,
        const std::shared_ptr<const NorthlightDrawSnapshot::Mesh>& mesh,
        IDirect3DVertexBuffer9* (&vb)[4],IDirect3DIndexBuffer9*& ib,Admission admit){
        if(!mesh||!mesh->byteSize()||mesh->byteSize()>Limit)return false;
        try{
            auto it=entries.find(mesh.get());
            if(it!=entries.end()&&it->second->owner.lock()!=mesh){bytes_-=it->second->bytes;entries.erase(it);it=entries.end();}
            if(it==entries.end()){
                if(!room(0))return false;
                auto e=std::make_unique<Entry>();e->owner=mesh;e->first=e->touched=frame_;
                entries.emplace(mesh.get(),std::move(e));++stats_.warmup;return false; // prove cross-frame reuse before allocating
            }
            Entry& e=*it->second;e.touched=frame_;
            if(!e.bytes){
                size_t bytes=mesh->byteSize();
                if(e.first==frame_){++stats_.warmup;return false;}
                if(attempts_>=16){++stats_.uploadDeferred;++stats_.attemptDeferred;return false;}
                if(bytes>UploadLimit-attemptedBytes_){++stats_.uploadDeferred;++stats_.uploadByteDeferred;return false;}
                if(!admit(bytes)){++stats_.admissionRejected;return false;}
                if(!room(bytes,true))return false;
                ++attempts_;attemptedBytes_+=bytes;
                auto built=std::make_unique<Entry>();built->owner=mesh;built->first=e.first;built->touched=frame_;
                for(unsigned s=0;s<4;++s){const auto& data=mesh->streams[s].bytes;if(data.empty())continue;
                    if(FAILED(device->CreateVertexBuffer(UINT(data.size()),D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&built->vertices[s],nullptr))||!built->vertices[s])return false;
                    void* out=nullptr;if(FAILED(built->vertices[s]->Lock(0,UINT(data.size()),&out,0)))return false;
                    if(out)std::memcpy(out,data.data(),data.size());HRESULT hr=built->vertices[s]->Unlock();if(!out||FAILED(hr))return false;
                }
                if(!mesh->indices.empty()){
                    const UINT count=UINT(mesh->indices.size()*4);
                    if(FAILED(device->CreateIndexBuffer(count,D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,D3DPOOL_DEFAULT,&built->indices,nullptr))||!built->indices)return false;
                    void* out=nullptr;if(FAILED(built->indices->Lock(0,count,&out,0)))return false;
                    if(out)std::memcpy(out,mesh->indices.data(),count);HRESULT hr=built->indices->Unlock();if(!out||FAILED(hr))return false;
                }
                built->bytes=bytes;bytes_+=bytes;uploaded_+=bytes;++created_;
                it->second=std::move(built);
            }else{reused_+=e.bytes;++hits_;}
            const Entry& ready=*it->second;
            for(unsigned s=0;s<4;++s){if(vb[s])vb[s]->Release();vb[s]=ready.vertices[s];if(vb[s])vb[s]->AddRef();}
            if(ib)ib->Release();ib=ready.indices;if(ib)ib->AddRef();return true;
        }catch(...){return false;}
    }
};
}
