    // 0.3.141 persistent casters (persistent_casters.h): render-thread side.
    // Observation, packet copies and GPU uploads are bounded per frame; the
    // vertex evaluation runs on its own background thread.
    struct PersistentGpu {
        IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;UINT vertices=0;bool index16=true;size_t bytes=0;
        std::vector<NorthlightPersistentCasters::Batch> batches;std::vector<IDirect3DTexture9*> textures;std::vector<float> cutoffs;std::vector<DWORD> addressU,addressV;
        PersistentGpu()=default;PersistentGpu(const PersistentGpu&)=delete;PersistentGpu& operator=(const PersistentGpu&)=delete;
        ~PersistentGpu(){if(vb)vb->Release();if(ib)ib->Release();for(auto* t:textures)if(t)t->Release();}
    };
    struct PersistentUpload {std::uint32_t id=0;std::shared_ptr<const NorthlightPersistentCasters::Mesh> mesh;std::unique_ptr<PersistentGpu> gpu;size_t vertexDone=0,indexDone=0;};
    static constexpr size_t PersistentUploadBytes=256u<<10; /* per frame */
    NorthlightPersistentCasters::Registry persistentCasters;std::unique_ptr<NorthlightPersistentCasters::Worker> persistentWorker;
    std::unordered_map<std::uint32_t,std::unique_ptr<PersistentGpu>> persistentGpu;PersistentUpload persistentUpload;
    std::vector<NorthlightPersistentCasters::Observation> persistentObservations;std::vector<size_t> persistentCandidates;
    std::vector<NorthlightPersistentCasters::Result> persistentResults;std::vector<std::uint32_t> persistentRemoved;std::vector<NorthlightPersistentCasters::Registry::Event> persistentEvents;
    std::vector<signed char> persistentRigid;std::unordered_map<IDirect3DVertexShader9*,unsigned char> persistentAudited; /* BLENDINDICES lanes read: 4, 1 (one-influence), 0 not audited */
    // Palette slots referenced (weight > 0) by one immutable snapshot: owner-checked, bounded.
    struct PersistentBones {std::weak_ptr<const NorthlightDrawSnapshot::Mesh> owner;const void* decl=nullptr;unsigned lanes=4;std::vector<unsigned char> bones;bool valid=false;};
    std::unordered_map<const NorthlightDrawSnapshot::Mesh*,PersistentBones> persistentBones;std::vector<float> persistentPoses;std::vector<size_t> persistentPoseAt;std::vector<unsigned char> persistentUsed;
    unsigned persistentRerenders=0;size_t persistentScanLeft=0;
    static constexpr size_t PersistentScanVisits=65536; /* index visits per frame for new bone lists (~0.2 ms) */
    std::string persistentMap;bool captureShortfall=false,persistentDisabled=false;unsigned persistentFrames=0,persistentFailures=0,persistentLogged=0;
    unsigned long long persistentSkipped=0,persistentMarked=0;double persistentFramePeakMs=0,persistentUploadPeakMs=0;
    // The slowest upload step since the last PERSISTENT line, by phase (0.3.150 log detail: a 51 ms step was seen once).
    struct PersistentUploadPeak {std::uint32_t id=0;bool begin=false;unsigned textures=0;unsigned long long fullScans=0,regionQueries=0;double admitMs=0,createMs=0,textureMs=0,copyMs=0;size_t copyBytes=0,vertexBytes=0,indexBytes=0;} persistentUploadPeak;
    bool persistentEnabled()const{return NorthlightPersistentCasters::Enabled&&quality.persistentCasters&&!persistentDisabled;}
    uint64_t persistentSignature(const float* matrix)const{return persistentEnabled()?persistentCasters.signature(matrix):0;}
    void releasePersistentGPU(){persistentBones.clear();persistentGpu.clear();persistentUpload=PersistentUpload{};persistentCasters.clear();persistentCasters.takeRemoved(persistentRemoved);persistentRemoved.clear();persistentAudited.clear();}
    // Audited palette programs (c31.. rows, 3 per bone): the four-weight blend
    // (SkinEnvelope layout) and the one-influence program (oneBoneTemplate: every
    // boneInfluences=1 skin, e.g. signs and other rigid props); root and bone
    // semantics are proven for both. Cached per original shader. lanes: the
    // BLENDINDICES lanes it reads (4, or 1: x only, weight 1).
    const NorthlightActorDeformation::Program* persistentProgram(IDirect3DVertexShader9* shader,unsigned* lanes=nullptr){
        auto program=actorPrograms.find(shader);if(program==actorPrograms.end())return nullptr;
        auto audited=persistentAudited.find(shader);const auto& p=program->second;
        if(audited==persistentAudited.end())audited=persistentAudited.emplace(shader,(unsigned char)(NorthlightReplayBounds::SkinEnvelope::supports(p)&&p.paletteBase==31?4:NorthlightPersistentCasters::oneBoneTemplate(p)?1:0)).first;
        if(lanes)*lanes=audited->second;return audited->second?&p:nullptr;
    }
    static std::uint64_t persistentDrawKey(const Replay& p){
        std::uint64_t key=14695981039346656037ull;auto mix=[&](uint64_t n){key=(key^n)*1099511628211ull;};
        mix(reinterpret_cast<uintptr_t>(p.originalShader));mix(reinterpret_cast<uintptr_t>(p.decl));
        if(p.shared)mix(reinterpret_cast<uintptr_t>(p.shared.get()));else{mix(p.mesh().vertexCount);mix(p.mesh().primitiveCount);mix(p.mesh().byteSize());}
        return key;
    }
    static bool persistentEligible(const Replay& p){return p.shadowSelected&&p.shadowSkinned;}
    // Referenced palette slots of one draw (audited layout: BLENDINDICES 0..74,
    // BLENDWEIGHT lanes > 0; the one-influence program: lane x, no weight).
    // Cached per immutable snapshot; owned snapshots (UP/dynamic misses) are
    // never cached and give no pose.
    const PersistentBones* persistentDrawBones(const Replay& p){
        if(!p.shared)return nullptr;const auto* mesh=p.shared.get();unsigned lanes=4;persistentProgram(p.originalShader,&lanes);if(lanes!=1)lanes=4;
        auto found=persistentBones.find(mesh);
        if(found!=persistentBones.end()){auto owner=found->second.owner.lock();if(owner.get()==mesh&&found->second.decl==p.decl&&found->second.lanes==lanes)return found->second.valid?&found->second:nullptr;persistentBones.erase(found);}
        // Scans are bounded per frame (arriving at an event makes many actors root-still at
        // once): over budget the pose is simply unavailable this frame and retried later.
        const size_t visits=mesh->indexed?mesh->indices.size():mesh->vertexCount;
        if(visits>persistentScanLeft)return nullptr;persistentScanLeft-=visits;
        if(persistentBones.size()>=4096)persistentBones.clear();
        PersistentBones entry;entry.owner=p.shared;entry.decl=p.decl;entry.lanes=lanes;
        const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;
        if(declarationCache.get(p.decl,elements,count)){const D3DVERTEXELEMENT9* index=nullptr;const D3DVERTEXELEMENT9* weight=nullptr;bool ok=true;
            for(UINT j=0;j<count;++j){const auto& e=elements[j];if(e.Stream==0xff)break;if(e.UsageIndex)continue;if(e.Usage==D3DDECLUSAGE_BLENDINDICES)index=&e;if(e.Usage==D3DDECLUSAGE_BLENDWEIGHT)weight=&e;}
            auto fits=[&](const D3DVERTEXELEMENT9* e){if(!e)return true;if(e->Stream>=4)return false;const auto& st=mesh->streams[e->Stream];const unsigned size=NorthlightDrawSnapshot::declarationBytes(e->Type);
                return size&&unsigned(e->Offset)+size<=st.stride&&std::uint64_t(mesh->vertexCount)*st.stride<=st.bytes.size();};
            if(lanes==1)weight=nullptr; /* the one-influence program reads no weight */
            ok=index&&fits(index)&&fits(weight)&&mesh->vertexCount<=65536;
            unsigned char used[75]={}; /* not persistentUsed: the caller's union is being built */
            auto visit=[&](UINT v){if(!ok||v>=mesh->vertexCount)return;NorthlightActorDeformation::Four i,w={1,1,1,1};
                const auto& si=mesh->streams[index->Stream];if(!NorthlightActorDeformation::decodeElement(si.bytes.data()+size_t(v)*si.stride+index->Offset,index->Type,i)){ok=false;return;}
                if(weight){const auto& sw=mesh->streams[weight->Stream];if(!NorthlightActorDeformation::decodeElement(sw.bytes.data()+size_t(v)*sw.stride+weight->Offset,weight->Type,w)){ok=false;return;}}
                ok=NorthlightPersistentCasters::referencedSlots(i,w,lanes,[&](unsigned slot){used[slot]=1;});};
            if(ok){if(mesh->indexed)for(auto v:mesh->indices)visit(v);else for(UINT v=0;v<mesh->vertexCount;++v)visit(v);}
            if(ok)for(unsigned b=0;b<75;++b)if(used[b])entry.bones.push_back((unsigned char)b);
            entry.valid=ok&&!entry.bones.empty();}
        auto& slot=persistentBones[mesh];slot=std::move(entry);return slot.valid?&slot:nullptr;
    }
    // World matrices (basis 9 + origin 3) of every palette slot the group's draws
    // reference, appended to persistentPoses. False: some draw cannot tell.
    bool persistentPose(const NorthlightPersistentCasters::Observation& o,const Replay& head,unsigned& bones){
        persistentUsed.assign(75,0);
        for(size_t i=o.first;i<o.end;++i){const auto& p=*replays[i];if(!persistentEligible(p))continue;const auto* b=persistentDrawBones(p);if(!b)return false;for(auto x:b->bones)persistentUsed[x]=1;}
        bones=0;const float* v=context.inverseView;
        for(unsigned b=0;b<75;++b){if(!persistentUsed[b])continue;float m[12];
            if(!NorthlightPersistentCasters::worldBone(head.constants+4*(31+3*b),v,m))return false;
            persistentPoses.insert(persistentPoses.end(),m,m+12);++bones;}
        return bones>0;
    }
    // Sampled vertex of the group's smallest-key draw: the vertex anchor.
    bool persistentVertex(const NorthlightPersistentCasters::Observation& o,float* at){
        const Replay* best=nullptr;std::uint64_t bestKey=0;
        for(size_t i=o.first;i<o.end;++i){const auto& p=*replays[i];if(!persistentEligible(p))continue;const auto k=persistentDrawKey(p);if(!best||k<bestKey){best=&p;bestKey=k;}}
        if(!best)return false;auto program=actorPrograms.find(best->originalShader);const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;float squared=0;
        return program!=actorPrograms.end()&&declarationCache.get(best->decl,elements,count)&&
            sampledVertices.distance(program->second,best->mesh(),best->shared,best->decl,elements,count,best->constants,context.inverseView,context.camera,squared,at);
    }
    bool persistentIsRigid(size_t n){
        if(persistentRigid[n]<0){const auto& o=persistentObservations[n];float bone=NAN;bool rigid=true;
            for(size_t i=o.first;i<o.end&&rigid;++i){const auto& p=*replays[i];if(!persistentEligible(p))continue;
                auto program=actorPrograms.find(p.originalShader);const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;
                const float b=program!=actorPrograms.end()&&declarationCache.get(p.decl,elements,count)?rigidBones.bone(program->second,p.mesh(),p.shared,p.decl,elements,count):NAN;
                rigid=!std::isnan(b)&&(std::isnan(bone)||b==bone);bone=b;}
            persistentRigid[n]=rigid?1:0;}
        return persistentRigid[n]==1;
    }
    // Immutable job for one actor: the same packet contents as GI actor capture
    // (snapshot owner, program, declaration, full constant bank, inverse view,
    // alpha texture copy). False: something is not reproducible on the CPU
    // (alpha: a cutout that may not (AlphaCutouts false) or cannot be reproduced).
    bool persistentJob(const NorthlightPersistentCasters::Observation& o,NorthlightPersistentCasters::Job& job,size_t& textureBytes,bool& alpha){
        textureBytes=0;alpha=false;job.packets.reserve(o.count);
        for(size_t i=o.first;i<o.end;++i){const auto& p=*replays[i];if(!persistentEligible(p))continue;
            const auto* program=persistentProgram(p.originalShader);const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;
            if(!program||!declarationCache.get(p.decl,elements,count)||!count)return false;
            NorthlightActorGeometry::Packet packet;packet.sharedMesh=p.shared;if(!packet.sharedMesh)packet.mesh=p.mesh();packet.position=*program;
            packet.elements.assign(elements,elements+count);std::memcpy(packet.constants.data(),p.constants,sizeof p.constantStorage);std::memcpy(packet.inverseView.data(),context.inverseView,64);
            if(p.cutoff>=0&&!NorthlightPersistentCasters::AlphaCutouts){alpha=true;return false;}
            // Cutout: the <=128 px copy of the replay's texture with its cutoff and address modes.
            if(p.cutoff>=0){auto uv=actorUVPrograms.find(p.originalShader);if(uv==actorUVPrograms.end()||!actorMaterial(p.texture,packet.texture)){alpha=true;return false;}
                packet.uv=uv->second;packet.hasUV=packet.alphaTest=true;packet.material.alphaCutoff=p.cutoff;packet.material.addressU=p.addressU;packet.material.addressV=p.addressV;
                textureBytes+=size_t(packet.texture.width)*packet.texture.height*4;}
            job.packets.push_back(std::move(packet));}
        return !job.packets.empty();
    }
    // Chunked upload of the nearest converted mesh (DEFAULT pool; released with the device).
    void persistentUploadStep(){
        using Clock=std::chrono::steady_clock;auto ms=[](Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();};
        const auto start=Clock::now();const DWORD now=GetTickCount();
        auto& u=persistentUpload;PersistentUploadPeak step;
        if(u.id){const auto& all=persistentCasters.entries();auto e=std::lower_bound(all.begin(),all.end(),u.id,[](const auto& x,std::uint32_t k){return x.id<k;});
            if(e==all.end()||e->id!=u.id||e->state!=NorthlightPersistentCasters::Registry::State::Converted)u=PersistentUpload{};}
        if(!u.id){const auto* e=persistentCasters.nextUpload();if(!e||!e->mesh)return;
            const auto& m=*e->mesh;auto gpu=std::make_unique<PersistentGpu>();gpu->vertices=UINT(m.vertices.size());gpu->index16=m.vertices.size()<=65535;gpu->batches=m.batches;
            const size_t vb=m.vertices.size()*sizeof(NorthlightGI::WorldVertex),ib=m.indices.size()*(gpu->index16?2:4);
            NorthlightGeometryMemory::Budget budget;budget.available=NorthlightGeometryMemory::ProcessReserve+64*NorthlightGeometryMemory::MiB+m.gpuBytes()*2;budget.largest=vb+NorthlightGeometryMemory::ChunkMargin;budget.valid=true;
            const auto& memory=admissionProbe.stats();const auto scans=memory.fullScans,queries=memory.regionQueries;const auto admitStart=Clock::now();
            bool ok=vb&&ib&&vb<=UINT_MAX&&ib<=UINT_MAX&&NorthlightGeometryMemory::admits(geometryAdmission(budget),budget);
            const auto admitted=Clock::now();
            ok=ok&&SUCCEEDED(d->CreateVertexBuffer(UINT(vb),D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&gpu->vb,nullptr))&&
                SUCCEEDED(d->CreateIndexBuffer(UINT(ib),D3DUSAGE_WRITEONLY,gpu->index16?D3DFMT_INDEX16:D3DFMT_INDEX32,D3DPOOL_DEFAULT,&gpu->ib,nullptr));
            const auto created=Clock::now();
            // Alpha materials: A8R8G8B8 with the decoded alpha (the static caster convention); <= 128x128 each.
            for(size_t k=0;ok&&k<m.materials.size();++k){const auto& mat=m.materials[k];IDirect3DTexture9* t=nullptr;
                if(mat.alphaCutoff>=0){const UINT w=std::max(1u,mat.width),h=std::max(1u,mat.height);D3DLOCKED_RECT lock{};
                    ok=SUCCEEDED(d->CreateTexture(w,h,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&t,nullptr))&&SUCCEEDED(t->LockRect(0,&lock,nullptr,0))&&lock.pBits;
                    if(ok){for(UINT y=0;y<h;++y){auto* row=reinterpret_cast<uint32_t*>(static_cast<char*>(lock.pBits)+size_t(y)*unsigned(lock.Pitch));
                            for(UINT x=0;x<w;++x){const size_t j=(size_t(y)*w+x)*4+3;row[x]=(uint32_t(j<mat.rgba.size()?mat.rgba[j]:255)<<24)|0x00ffffffu;}}
                        ok=SUCCEEDED(t->UnlockRect(0));}}
                gpu->textures.push_back(t);gpu->cutoffs.push_back(mat.alphaCutoff);gpu->addressU.push_back(mat.addressU);gpu->addressV.push_back(mat.addressV);step.textures+=mat.alphaCutoff>=0;}
            const auto textured=Clock::now();
            step.begin=true;step.admitMs=ms(admitStart,admitted);step.createMs=ms(admitted,created);step.textureMs=ms(created,textured);step.fullScans=memory.fullScans-scans;step.regionQueries=memory.regionQueries-queries;
            if(!ok){persistentCasters.uploadFailed(e->id,now);return;}
            gpu->bytes=m.gpuBytes();u.id=e->id;u.mesh=e->mesh;u.gpu=std::move(gpu);}
        const auto& m=*u.mesh;auto& g=*u.gpu;size_t budget=PersistentUploadBytes;
        const size_t vb=m.vertices.size()*sizeof(NorthlightGI::WorldVertex),ib=m.indices.size()*(g.index16?2:4);
        auto fail=[&]{persistentCasters.uploadFailed(u.id,now);u=PersistentUpload{};};
        // Disjoint chunks of fresh buffers that no draw sees before persistentGpu[u.id] (upload_lock.h).
        step.id=u.id;step.vertexBytes=vb;step.indexBytes=ib;const auto copyStart=Clock::now();
        if(u.vertexDone<vb){const size_t n=std::min(budget,vb-u.vertexDone);void* out=nullptr;
            if(FAILED(g.vb->Lock(UINT(u.vertexDone),UINT(n),&out,NorthlightUpload::FreshBufferLock))||!out)return fail();
            std::memcpy(out,reinterpret_cast<const char*>(m.vertices.data())+u.vertexDone,n);if(FAILED(g.vb->Unlock()))return fail();u.vertexDone+=n;budget-=n;step.copyBytes+=n;}
        if(u.vertexDone==vb&&u.indexDone<ib&&budget>=4){const size_t stride=g.index16?2:4;const size_t n=std::min(budget,ib-u.indexDone)/stride*stride;void* out=nullptr;
            if(FAILED(g.ib->Lock(UINT(u.indexDone),UINT(n),&out,NorthlightUpload::FreshBufferLock))||!out)return fail();
            if(g.index16){auto* dst=static_cast<uint16_t*>(out);for(size_t i=0;i<n/2;++i)dst[i]=uint16_t(m.indices[u.indexDone/2+i]);}
            else std::memcpy(out,reinterpret_cast<const char*>(m.indices.data())+u.indexDone,n);
            if(FAILED(g.ib->Unlock()))return fail();u.indexDone+=n;step.copyBytes+=n;}
        step.copyMs=ms(copyStart,Clock::now());
        if(u.vertexDone==vb&&u.indexDone==ib){persistentCasters.ready(u.id,g.bytes);persistentGpu[u.id]=std::move(u.gpu);u=PersistentUpload{};}
        const double stepMs=ms(start,Clock::now());if(stepMs>persistentUploadPeakMs){persistentUploadPeakMs=stepMs;persistentUploadPeak=step;}
    }
    // Once per frame, before shadow selection: observe still actors, retire
    // removed casters, take worker results, upload, and start at most one job.
    // Draws of matched ready casters get persistentId: the actor budget ignores
    // them and each cascade skips them where its committed cache content has them.
    void persistentFrame(){
        for(auto& p:replays)p->persistentId=0;
        if(!persistentEnabled()||!effects.shadows)return;
        const auto started=std::chrono::steady_clock::now();const DWORD now=GetTickCount();++persistentFrames;
        try{
            if(lastRequest.map!=persistentMap){if(!persistentMap.empty())persistentCasters.clear();persistentMap=lastRequest.map;}
            persistentCasters.classes(quality.persistentCasters!=0); /* settings are fixed after start-up: once */
            if(persistentWorker){persistentWorker->collect(persistentResults);
                for(auto& r:persistentResults){const auto id=r.id;const auto failure=r.failure;const unsigned bones=r.bones;const double ms=r.ms;
                    size_t vertices=r.mesh?r.mesh->vertices.size():0,triangles=r.mesh?r.mesh->indices.size()/3:0;
                    const bool accepted=persistentCasters.accept(std::move(r),now);
                    if(persistentLogged<200&&NorthlightDiagnostics::enabled()){++persistentLogged;const auto& all=persistentCasters.entries();auto e=std::lower_bound(all.begin(),all.end(),id,[](const auto& x,std::uint32_t k){return x.id<k;});
                        if(accepted&&e!=all.end()&&e->id==id)logf("PERSISTENT caster converted id=%u root=(%.1f %.1f %.1f) distance=%.1f draws=%u vertices=%zu triangles=%zu bones=%u cutouts=%zu bytes=%zu workerMs=%.2f",id,e->root[0],e->root[1],e->root[2],std::sqrt(e->distanceSquared),e->draws,vertices,triangles,bones,e->mesh?e->mesh->materials.size()-1:0,e->bytes,ms);
                        else logf("PERSISTENT caster rejected id=%u reason=%s bones=%u workerMs=%.2f",id,accepted?"gone":NorthlightPersistentCasters::failureName(failure),bones,ms);}}}
            persistentUploadStep();
            persistentScanLeft=PersistentScanVisits;
            auto& obs=persistentObservations;obs.clear();persistentPoses.clear();persistentPoseAt.clear();
            const float* pivot=actorShadowOriginValid?actorShadowOrigin:context.camera;
            for(size_t i=0;i<replays.size();){const unsigned group=replays[i]->constantGroup;size_t j=i;
                NorthlightPersistentCasters::Observation o;o.first=i;bool audited=true;const Replay* head=nullptr;
                for(;j<replays.size()&&replays[j]->constantGroup==group;++j){const auto& p=*replays[j];if(!persistentEligible(p))continue;
                    if(!persistentProgram(p.originalShader)){audited=false;continue;}
                    const auto key=persistentDrawKey(p);if(!head||key<o.key)o.key=key;if(!head)head=&p;
                    ++o.count;o.bytes+=p.mesh().byteSize();o.vertices+=p.mesh().vertexCount;o.triangles+=p.count;}
                o.end=j;i=j;
                // Unaudited groups, and audited ones without a finite root/basis: no observation.
                if(!head||!audited)continue;
                const auto* program=persistentProgram(head->originalShader);
                if(!NorthlightActorDeformation::rootWorld(*program,head->constants,context.inverseView,o.root))continue;
                // Bone 0 basis: model axis a -> view (rows c31..c33, column a) -> world.
                const float* rows=head->constants+4*31;bool finite=true;
                for(unsigned a=0;a<3;++a)for(unsigned w=0;w<3;++w){const float v=rows[a]*context.inverseView[w]+rows[4+a]*context.inverseView[4+w]+rows[8+a]*context.inverseView[8+w];o.axes[a*3+w]=v;finite=finite&&std::isfinite(v);}
                if(!finite)continue;
                float q=0;for(unsigned k=0;k<3;++k){const float t=o.root[k]-pivot[k];q+=t*t;}o.distanceSquared=q;
                // Full pose only where it matters (root-still actors and casters); offsets now, pointers below.
                size_t poseAt=SIZE_MAX;
                if(persistentCasters.needsPose(o.key,o.root)){const size_t at=persistentPoses.size();unsigned bones=0;
                    if(persistentPose(o,*head,bones)){o.poseBones=bones;poseAt=at;}else persistentPoses.resize(at);}
                obs.push_back(o);persistentPoseAt.push_back(poseAt);}
            for(size_t n=0;n<obs.size();++n)if(persistentPoseAt[n]!=SIZE_MAX)obs[n].pose=persistentPoses.data()+persistentPoseAt[n];
            persistentCasters.frame(obs,now,pivot,context.inverseView,projection,!captureShortfall,persistentCandidates);
            // Round-robin sampled-vertex check of matched ready casters (1 in 8 frames each).
            for(const auto& o:obs)if(o.caster&&((o.caster+persistentFrames)&7u)==0){float at[3];if(persistentVertex(o,at))persistentCasters.vertex(o.caster,at,now);}
            const auto& all=persistentCasters.entries();
            for(const auto& o:obs){if(!o.caster)continue;auto e=std::lower_bound(all.begin(),all.end(),o.caster,[](const auto& x,std::uint32_t k){return x.id<k;});
                if(e==all.end()||e->id!=o.caster||e->state!=NorthlightPersistentCasters::Registry::State::Ready||!persistentGpu.count(o.caster))continue;
                for(size_t i=o.first;i<o.end;++i){auto& p=*replays[i];if(!persistentEligible(p))continue;
                    p.persistentId=o.caster;p.persistentLow=e->low;p.persistentHigh=e->high;p.fateClass=NorthlightShadowFate::Persistent;++persistentMarked;}}
            // At most one new job per frame, nearest candidate first.
            if(!persistentCandidates.empty()){
                if(!persistentWorker)persistentWorker=std::make_unique<NorthlightPersistentCasters::Worker>([]{SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);});
                const auto& tuning=persistentCasters.tuning();persistentRigid.assign(obs.size(),-1);
                if(persistentWorker->inFlight()<tuning.maxPending)for(size_t c:persistentCandidates){const auto& o=obs[c];
                    if(o.count>tuning.maxDraws||o.vertices>4*tuning.maxVertices){persistentCasters.reject(o,now,true,NorthlightPersistentCasters::Failure::Oversize);continue;}
                    if(persistentCasters.attachedToActor(obs,c,[&](size_t n){return persistentIsRigid(n);})){persistentCasters.defer(o,now,false);continue;}
                    const size_t geometry=size_t(std::min<size_t>(o.vertices,tuning.maxVertices))*sizeof(NorthlightGI::WorldVertex)+size_t(o.triangles)*3*2;
                    if(!persistentCasters.room(o,geometry,now)){persistentCasters.defer(o,now,true);break;} /* before any copy */
                    auto job=std::make_shared<NorthlightPersistentCasters::Job>();size_t textureBytes=0;
                    bool alpha=false;
                    if(!persistentJob(o,*job,textureBytes,alpha)){persistentCasters.reject(o,now,alpha,alpha?NorthlightPersistentCasters::Failure::Alpha:NorthlightPersistentCasters::Failure::Empty);continue;} /* alpha: permanent (opaque only) */
                    float vertex[3];const bool hasVertex=persistentVertex(o,vertex);
                    const size_t estimate=geometry+textureBytes;
                    job->id=persistentCasters.begin(o,estimate,now,hasVertex?vertex:nullptr);
                    if(!job->id){persistentCasters.defer(o,now,true);break;}
                    job->maxVertices=tuning.maxVertices;job->bones=o.poseBones;persistentWorker->submit(std::move(job));break;}}
            persistentCasters.takeRemoved(persistentRemoved);for(auto id:persistentRemoved)persistentGpu.erase(id);persistentRemoved.clear();
            persistentCasters.takeEvents(persistentEvents);
            for(const auto& v:persistentEvents)if(persistentLogged<200&&NorthlightDiagnostics::enabled()){++persistentLogged;
                if(v.rejected){logf("PERSISTENT caster rejected reason=bones distinctBoneMatrices=%u characterBones=%u root=(%.1f %.1f %.1f)",v.bones,persistentCasters.tuning().characterBones,v.root[0],v.root[1],v.root[2]);continue;}
                logf("PERSISTENT caster removed id=%u reason=%s root=(%.1f %.1f %.1f) wasReady=%d",v.id,NorthlightPersistentCasters::removalName(v.reason),v.root[0],v.root[1],v.root[2],int(v.ready));}
        }catch(...){
            for(auto& p:replays)p->persistentId=0;releasePersistentGPU();persistentWorker.reset();persistentDisabled=true;invalidateShadowCache();
            logf("PERSISTENT casters disabled: allocation failure; replay shadows retained");return;
        }
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();persistentFramePeakMs=std::max(persistentFramePeakMs,ms);
        if(captureSampled){const auto& s=persistentCasters.stats();using R=NorthlightPersistentCasters::Registry::State;
            logf("PERSISTENT casters ready=%zu converted=%zu pending=%zu bytes=%zu budget=%zu tracks=%u observed=%zu candidates=%zu markedDraws=%llu skippedReplayDraws=%llu submitted=%llu accepted=%llu uploads=%llu rejectedBones=%llu rejectedAlpha=%llu rejectedOther=%llu deferredAttach=%llu deferredBudget=%llu removed moved=%llu unseen=%llu range=%llu inView=%llu rekey=%llu shape=%llu evicted=%llu failed=%llu cleared=%llu workerMs=%.2f workerPeakMs=%.2f frameMs=%.3f framePeakMs=%.3f uploadPeakMs=%.3f captureComplete=%d publications=%llu poseStillNow=%u attachBlockedNow=%u cacheRerendersSinceLog=%u",
                persistentCasters.count(R::Ready),persistentCasters.count(R::Converted),persistentCasters.count(R::Pending),persistentCasters.usedBytes(),persistentCasters.tuning().budgetBytes,s.tracks,persistentObservations.size(),persistentCandidates.size(),persistentMarked,persistentSkipped,
                (unsigned long long)s.submitted,(unsigned long long)s.converted,(unsigned long long)s.uploads,(unsigned long long)s.rejectedBones,(unsigned long long)s.rejectedAlpha,(unsigned long long)s.rejectedOther,(unsigned long long)s.deferredAttach,(unsigned long long)s.deferredBudget,
                (unsigned long long)s.removed[0],(unsigned long long)s.removed[1],(unsigned long long)s.removed[2],(unsigned long long)s.removed[3],(unsigned long long)s.removed[4],(unsigned long long)s.removed[5],(unsigned long long)s.removed[6],(unsigned long long)s.removed[7],(unsigned long long)s.removed[8],
                s.workerMs,s.workerPeakMs,ms,persistentFramePeakMs,persistentUploadPeakMs,int(!captureShortfall),(unsigned long long)s.publications,s.poseStill,s.attachBlocked,persistentRerenders);
            if(persistentUploadPeakMs>0){const auto& k=persistentUploadPeak;
                logf("PERSISTENT upload peak ms=%.3f id=%u begin=%d admitMs=%.3f fullScans=%llu regionQueries=%llu createMs=%.3f textures=%u textureMs=%.3f copyBytes=%zu copyMs=%.3f vertexBytes=%zu indexBytes=%zu",
                    persistentUploadPeakMs,k.id,int(k.begin),k.admitMs,k.fullScans,k.regionQueries,k.createMs,k.textures,k.textureMs,k.copyBytes,k.copyMs,k.vertexBytes,k.indexBytes);}
            persistentMarked=persistentSkipped=0;persistentRerenders=0;persistentFramePeakMs=persistentUploadPeakMs=0;persistentUploadPeak=PersistentUploadPeak{};}
    }
    // Cache-slot drawing: every visible ready caster, in id order, with the
    // static cache's exact-depth shaders; rects: the partial redraw's scissors.
    bool drawPersistentCasters(const float* matrix,const std::vector<NorthlightShadowBounds::TexelRect>* rects){
        if(!persistentEnabled()||persistentGpu.empty())return true;
        bool ok=true;const NorthlightShadowBounds::TexelRect* scissor=nullptr;std::vector<const NorthlightShadowBounds::TexelRect*> hits;
        d->SetVertexDeclaration(shadowDecl);for(int i=0;i<4;++i)d->SetStreamSourceFreq(i,1);d->SetStreamSource(1,nullptr,0,0);
        d->SetVertexShader(cachedShadowVS);d->SetVertexShaderConstantF(0,matrix,4);
        d->SetRenderState(D3DRS_ZENABLE,TRUE);d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);d->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);d->SetRenderState(D3DRS_SCISSORTESTENABLE,rects?TRUE:FALSE);
        d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE); /* the replay's sampler */
        IDirect3DPixelShader9* bound=nullptr;
        persistentCasters.forVisible(matrix,[&](const NorthlightPersistentCasters::Registry::Entry& e){if(!ok)return;
            auto found=persistentGpu.find(e.id);if(found==persistentGpu.end())return;const auto& g=*found->second;
            if(rects){hits.clear();NorthlightShadowBounds::TexelRect footprint;const bool known=NorthlightShadowBounds::texelFootprint(e.low,e.high,matrix,long(ShadowCacheSize),StaticCacheDirtyMargin,footprint);
                for(const auto& r:*rects)if(!known||NorthlightShadowBounds::intersects(footprint,r))hits.push_back(&r);if(hits.empty())return;}
            d->SetStreamSource(0,g.vb,0,sizeof(NorthlightGI::WorldVertex));d->SetIndices(g.ib);
            for(const auto& b:g.batches){const float cutoff=g.cutoffs[b.material];const bool opaque=cutoff<0;
                IDirect3DPixelShader9* ps=opaque&&cachedOpaquePS?cachedOpaquePS:cachedShadowPS;if(ps!=bound){d->SetPixelShader(ps);bound=ps;}
                float material[]={1,1,1,opaque?-2.f:cutoff};d->SetPixelShaderConstantF(0,material,1);d->SetTexture(0,opaque?nullptr:g.textures[b.material]);
                d->SetSamplerState(0,D3DSAMP_ADDRESSU,g.addressU[b.material]);d->SetSamplerState(0,D3DSAMP_ADDRESSV,g.addressV[b.material]);
                for(size_t k=0;k<(rects?hits.size():1);++k){
                    if(rects&&scissor!=hits[k]){const RECT r={LONG(hits[k]->left),LONG(hits[k]->top),LONG(hits[k]->right),LONG(hits[k]->bottom)};d->SetScissorRect(&r);scissor=hits[k];}
                    if(FAILED(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,g.vertices,b.firstIndex,b.indexCount/3))){ok=false;return;}}}});
        d->SetTexture(0,nullptr);
        if(!ok){logf("PERSISTENT caster draw failed; casters returned to replays");releasePersistentGPU();if(++persistentFailures>=3)persistentDisabled=true;}
        return ok;
    }
    // Dirty boxes of casters added/removed since the slot's recorded content.
    bool persistentChangedBounds(const ShadowCacheKey& key,const float* matrix){
        return persistentEnabled()&&persistentCasters.changedBounds(matrix,key.persistentContent,[&](NorthlightGI::Vec3 low,NorthlightGI::Vec3 high){StaticShadow::CasterBounds box;box.add(low,high);staticDirtyScratch.push_back(box);});
    }
    // Draw calls drawPersistentCasters() would issue per candidate rect set.
    void persistentDrawCalls(const float* matrix,const std::vector<const std::vector<NorthlightShadowBounds::TexelRect>*>& candidates,std::vector<size_t>& costs){
        if(!persistentEnabled())return;
        persistentCasters.forVisible(matrix,[&](const NorthlightPersistentCasters::Registry::Entry& e){auto found=persistentGpu.find(e.id);if(found==persistentGpu.end())return;const size_t calls=found->second->batches.size();
            NorthlightShadowBounds::TexelRect footprint;const bool known=NorthlightShadowBounds::texelFootprint(e.low,e.high,matrix,long(ShadowCacheSize),StaticCacheDirtyMargin,footprint);
            for(size_t k=0;k<candidates.size();++k){if(!candidates[k]){costs[k]+=calls;continue;}for(const auto& r:*candidates[k])if(!known||NorthlightShadowBounds::intersects(footprint,r))costs[k]+=calls;}});
    }
