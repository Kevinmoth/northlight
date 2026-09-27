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
    // Palette slots referenced (weight > 0) by one immutable snapshot, with the
    // model-space box of each slot's vertices (6 floats per slot; empty: no
    // decodable POSITION): owner-checked, bounded.
    struct PersistentBones {std::weak_ptr<const NorthlightDrawSnapshot::Mesh> owner;const void* decl=nullptr;unsigned lanes=4;std::vector<unsigned char> bones;std::vector<float> boxes;bool valid=false;};
    std::unordered_map<const NorthlightDrawSnapshot::Mesh*,PersistentBones> persistentBones;std::vector<float> persistentPoses;std::vector<size_t> persistentPoseAt;std::vector<unsigned char> persistentUsed;
    // Rigid props (per frame, candidate frames only): body boxes of observations, and
    // skinned groups outside the audited layout (no root, no box), located lazily.
    struct PersistentBody {signed char known=-2;float low[3]={},high[3]={};};std::vector<PersistentBody> persistentBody; /* -2: not computed yet */
    struct PersistentOther {size_t first=0,end=0;signed char located=-1;float at[3]={};};std::vector<PersistentOther> persistentOthers;bool persistentOthersOverflow=false;
    // Static-cache placements of this map's current scene, indexed PersistentIndexStep per frame.
    NorthlightPersistentCasters::PlacementIndex persistentIndex;static constexpr size_t PersistentIndexStep=2048; /* ~0.1 ms */DWORD persistentMapMs=0;
    unsigned persistentRerenders=0;size_t persistentScanLeft=0,persistentOneBone=0;
    static constexpr size_t PersistentScanVisits=65536; /* index visits per frame for new bone lists (~0.2 ms) */
    std::string persistentMap;bool captureShortfall=false,persistentDisabled=false;unsigned persistentFrames=0,persistentFailures=0,persistentLogged=0;
    unsigned long long persistentSkipped=0,persistentMarked=0;double persistentFramePeakMs=0,persistentUploadPeakMs=0,persistentDiagPeakMs=0;
    // The slowest upload step since the last PERSISTENT line, by phase (0.3.150 log detail: a 51 ms step was seen once).
    struct PersistentUploadPeak {std::uint32_t id=0;bool begin=false;unsigned textures=0;unsigned long long fullScans=0,regionQueries=0;double admitMs=0,createMs=0,textureMs=0,copyMs=0;size_t copyBytes=0,vertexBytes=0,indexBytes=0;} persistentUploadPeak;
    // Diagnostics (Diagnostics=1 with PersistentRigidProps=1 only; the PERSISTENT near lines): rigid-class tracks
    // and not-observed groups near the pivot, their state changes and their casters' cache status.
    static constexpr float PersistentDiagRadius=15.f;static constexpr size_t PersistentDiagShown=12,PersistentDiagWatch=32;
    struct PersistentWatch {std::uint32_t serial=0;std::uint64_t key=0;float root[3]={},axes[9]={},distance=0;unsigned triangles=0,draws=0,bones=0,lanes=0,marked=0;bool seen=false,hasAxes=false;int cached=-1;
        NorthlightPersistentCasters::Registry::Explained why;char cache[128]={};};
    struct PersistentUnobserved {size_t first=0;float at[3]={},distance=0;unsigned draws=0,triangles=0,selected=0,skinned=0,eligible=0,oneBone=0,unaudited=0;bool rooted=false;};
    std::vector<PersistentWatch> persistentWatch,persistentRows;std::vector<PersistentUnobserved> persistentUnobserved;
    bool persistentDiagStarted=false;unsigned persistentDiagTokens=0,persistentDiagSuppressed=0;DWORD persistentDiagRefillMs=0;
    // Key components (persistentDrawKey) of a near caster's smallest-key draw, recorded while seen: a rekey names what changed.
    struct PersistentKeyParts {const void* shader=nullptr;const void* decl=nullptr;const void* snapshot=nullptr;std::weak_ptr<const NorthlightDrawSnapshot::Mesh> owner;UINT vertices=0,primitives=0;size_t bytes=0;};
    std::unordered_map<std::uint32_t,PersistentKeyParts> persistentDiagParts;unsigned persistentDiagFrames=0,persistentDiagShortfalls=0;
    bool persistentKeyParts(const NorthlightPersistentCasters::Observation& o,PersistentKeyParts& out){const Replay* best=nullptr;std::uint64_t bestKey=0;
        for(size_t i=o.first;i<o.end;++i){const auto& p=*replays[i];if(!persistentEligible(p)||!persistentProgram(p.originalShader))continue;const auto k=persistentDrawKey(p);if(!best||k<bestKey){best=&p;bestKey=k;}}
        if(!best)return false;out.shader=best->originalShader;out.decl=best->decl;out.snapshot=best->shared.get();out.owner=best->shared;out.vertices=best->mesh().vertexCount;out.primitives=best->mesh().primitiveCount;out.bytes=best->mesh().byteSize();return true;}
    std::deque<std::string> persistentDiagLines; /* formatted, written at most 3 per frame (a list never costs one frame 13 flushes) */
    bool persistentDiag()const{return quality.persistentRigidProps&&NorthlightDiagnostics::enabled();}
    bool persistentEnabled()const{return NorthlightPersistentCasters::Enabled&&(quality.persistentCasters||quality.persistentRigidProps)&&!persistentDisabled;}
    uint64_t persistentSignature(const float* matrix)const{return persistentEnabled()?persistentCasters.signature(matrix):0;}
    void releasePersistentGPU(){persistentBones.clear();persistentGpu.clear();persistentUpload=PersistentUpload{};persistentCasters.clear();persistentCasters.takeRemoved(persistentRemoved);persistentRemoved.clear();persistentAudited.clear();}
    // Rigid props the static cache draws (staticPlacement against M2 and WMO-doodad placements).
    static std::uint64_t persistentSceneRevision(const StaticShadow::Snapshot& scene){return scene.revision*1000003ull^scene.mapGeneration;}
    void persistentIndexStep(){
        if(!staticScene||staticScene->map!=lastRequest.map)return;auto& x=persistentIndex;const auto& all=staticScene->placements;
        if(x.scene!=staticScene.get()||x.revision!=persistentSceneRevision(*staticScene))x.reset(staticScene.get(),persistentSceneRevision(*staticScene));
        for(const size_t end=std::min(all.size(),x.next+PersistentIndexStep);x.next<end;++x.next){const auto& place=all[x.next];
            if(place.category==1||place.category==3)x.add(place.translation.x,place.translation.y,place.translation.z,std::uint32_t(x.next));}
        x.complete=x.next==all.size();
    }
    // 1 covered, 0 not (or no scene for this map), -1 this scene is still being indexed.
    int persistentStaticCovered(const NorthlightPersistentCasters::Observation& o){
        if(!staticScene||staticScene->map!=lastRequest.map)return 0;const auto& x=persistentIndex;
        if(x.scene!=staticScene.get()||x.revision!=persistentSceneRevision(*staticScene)||!x.complete)return -1;
        return x.find(o.root,.05f,[&](std::uint32_t i){const auto& place=staticScene->placements[i];const float t[3]={place.translation.x,place.translation.y,place.translation.z};
            return NorthlightPersistentCasters::staticPlacement(o.root,o.axes,t,place.matrix);})?1:0;
    }
    // Audited palette programs (c31.. rows, 3 per bone): the four-weight blend
    // (SkinEnvelope layout) and the one-influence program (oneBoneTemplate: every
    // boneInfluences=1 skin, i.e. signs and other rigid props); root and bone
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
        if(declarationCache.get(p.decl,elements,count)){const D3DVERTEXELEMENT9* index=nullptr;const D3DVERTEXELEMENT9* weight=nullptr;const D3DVERTEXELEMENT9* position=nullptr;bool ok=true;
            for(UINT j=0;j<count;++j){const auto& e=elements[j];if(e.Stream==0xff)break;if(e.UsageIndex)continue;if(e.Usage==D3DDECLUSAGE_BLENDINDICES)index=&e;if(e.Usage==D3DDECLUSAGE_BLENDWEIGHT)weight=&e;if(e.Usage==D3DDECLUSAGE_POSITION)position=&e;}
            auto fits=[&](const D3DVERTEXELEMENT9* e){if(!e)return true;if(e->Stream>=4)return false;const auto& st=mesh->streams[e->Stream];const unsigned size=NorthlightDrawSnapshot::declarationBytes(e->Type);
                return size&&unsigned(e->Offset)+size<=st.stride&&std::uint64_t(mesh->vertexCount)*st.stride<=st.bytes.size();};
            if(lanes==1)weight=nullptr; /* the one-influence program reads no weight */
            ok=index&&fits(index)&&fits(weight)&&mesh->vertexCount<=65536;bool boxed=position&&fits(position);
            unsigned char used[75]={}; /* not persistentUsed: the caller's union is being built */
            float box[75][6];for(auto& b:box){b[0]=b[1]=b[2]=INFINITY;b[3]=b[4]=b[5]=-INFINITY;}
            auto visit=[&](UINT v){if(!ok||v>=mesh->vertexCount)return;NorthlightActorDeformation::Four i,w={1,1,1,1},x;
                const auto& si=mesh->streams[index->Stream];if(!NorthlightActorDeformation::decodeElement(si.bytes.data()+size_t(v)*si.stride+index->Offset,index->Type,i)){ok=false;return;}
                if(weight){const auto& sw=mesh->streams[weight->Stream];if(!NorthlightActorDeformation::decodeElement(sw.bytes.data()+size_t(v)*sw.stride+weight->Offset,weight->Type,w)){ok=false;return;}}
                if(boxed){const auto& sp=mesh->streams[position->Stream];boxed=NorthlightActorDeformation::decodeElement(sp.bytes.data()+size_t(v)*sp.stride+position->Offset,position->Type,x)&&std::isfinite(x[0])&&std::isfinite(x[1])&&std::isfinite(x[2]);}
                ok=NorthlightPersistentCasters::referencedSlots(i,w,lanes,[&](unsigned slot){used[slot]=1;
                    if(boxed){float* b=box[slot];for(unsigned a=0;a<3;++a){b[a]=std::min(b[a],x[a]);b[3+a]=std::max(b[3+a],x[a]);}}});};
            if(ok){if(mesh->indexed)for(auto v:mesh->indices)visit(v);else for(UINT v=0;v<mesh->vertexCount;++v)visit(v);}
            if(ok)for(unsigned b=0;b<75;++b)if(used[b]){entry.bones.push_back((unsigned char)b);if(boxed)entry.boxes.insert(entry.boxes.end(),box[b],box[b]+6);}
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
    // Rigid props: world box of body k (a vertex blended over palette bones lies in
    // the hull of its per-bone images, so each referenced bone's model-space box
    // through that bone's current world matrix encloses the skinned mesh). BodyLater:
    // the scan budget is spent this frame; BodyNever: an owned snapshot, a snapshot
    // without a usable palette or POSITION (heldByBody then holds within attachReach).
    int persistentBodyBounds(size_t k,float* low,float* high){
        using R=NorthlightPersistentCasters::Registry;auto& body=persistentBody[k];
        if(body.known==-2){body.known=R::BodyNever;const auto& o=persistentObservations[k];float box[75][6];unsigned char used[75]={};const Replay* head=nullptr;
            for(auto& b:box){b[0]=b[1]=b[2]=INFINITY;b[3]=b[4]=b[5]=-INFINITY;}
            for(size_t i=o.first;i<o.end;++i){const auto& p=*replays[i];if(!persistentEligible(p))continue;const auto* b=persistentDrawBones(p);
                if(!b){auto cached=p.shared?persistentBones.find(p.shared.get()):persistentBones.end(); /* cached: invalid for good; otherwise the budget */
                    if(p.shared&&(cached==persistentBones.end()||cached->second.owner.lock()!=p.shared))body.known=R::BodyLater;return body.known;}
                if(b->boxes.size()!=b->bones.size()*6)return body.known;if(!head)head=&p;
                for(size_t j=0;j<b->bones.size();++j){float* x=box[b->bones[j]];const float* y=b->boxes.data()+6*j;used[b->bones[j]]=1;
                    for(unsigned a=0;a<3;++a){x[a]=std::min(x[a],y[a]);x[3+a]=std::max(x[3+a],y[3+a]);}}}
            if(!head)return body.known;float boxes[75*6],worlds[75*12],l[3],h[3];unsigned bones=0;
            for(unsigned b=0;b<75;++b){if(!used[b])continue;if(!NorthlightPersistentCasters::worldBone(head->constants+4*(31+3*b),context.inverseView,worlds+12*bones))return body.known;std::memcpy(boxes+6*bones,box[b],24);++bones;}
            if(!NorthlightPersistentCasters::envelope(boxes,worlds,bones,l,h))return body.known;
            std::memcpy(body.low,l,12);std::memcpy(body.high,h,12);body.known=R::BodyKnown;}
        if(body.known==R::BodyKnown){std::memcpy(low,body.low,12);std::memcpy(high,body.high,12);}
        return body.known;
    }
    // Rigid props: held/worn by a body (heldByBody), or within attachReach of a skinned
    // group outside the audited layout (no root or box: its sampled vertex); a group that
    // cannot be located, or more than the recorded 4096, leaves it undecided.
    NorthlightPersistentCasters::Registry::Hold persistentBodyHeld(size_t c){const auto hold=persistentBodyHeldRule(c);if(hold!=NorthlightPersistentCasters::Registry::Free&&persistentDiag())persistentDiagHold(c,hold);return hold;}
    // Diagnostics: a hold the body rule did not decide came from the groups outside the audited layout (same order as the rule).
    void persistentDiagHold(size_t c,NorthlightPersistentCasters::Registry::Hold hold){
        using R=NorthlightPersistentCasters::Registry;using K=NorthlightPersistentCasters::HoldKind;const auto& o=persistentObservations[c];const auto* d=persistentCasters.diagnosis(o);
        if(d&&((hold==R::Held&&(d->hold==K::Box||d->hold==K::Never))||(hold==R::Undecided&&d->hold==K::Later)))return;
        if(persistentOthersOverflow&&hold==R::Undecided){persistentCasters.noteHold(o,K::Overflow,nullptr);return;}
        const float reach=persistentCasters.tuning().attachReach;
        for(const auto& g:persistentOthers){if(g.located!=1){persistentCasters.noteHold(o,K::Unlocated,nullptr);return;}
            float q=0;for(unsigned k=0;k<3;++k){const float t=g.at[k]-o.root[k];q+=t*t;}if(q<=reach*reach){persistentCasters.noteHold(o,K::Other,g.at);return;}}
    }
    NorthlightPersistentCasters::Registry::Hold persistentBodyHeldRule(size_t c){
        using R=NorthlightPersistentCasters::Registry;const auto& obs=persistentObservations;
        R::Hold hold=persistentCasters.heldByBody(obs,c,[&](size_t n){return persistentIsRigid(n);},[&](size_t k,float* low,float* high){return persistentBodyBounds(k,low,high);});
        if(hold==R::Held)return hold;if(persistentOthersOverflow)return R::Undecided;
        const float r=persistentCasters.tuning().attachReach;const float* root=obs[c].root;
        for(auto& g:persistentOthers){
            if(g.located<0){g.located=0;
                for(size_t i=g.first;i<g.end&&!g.located;++i){const auto& p=*replays[i];if(!persistentEligible(p))continue;auto program=actorPrograms.find(p.originalShader);
                    const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;float squared=0;
                    g.located=program!=actorPrograms.end()&&declarationCache.get(p.decl,elements,count)&&
                        sampledVertices.distance(program->second,p.mesh(),p.shared,p.decl,elements,count,p.constants,context.inverseView,context.camera,squared,g.at)?1:0;}}
            if(g.located!=1)return R::Undecided;
            float q=0;for(unsigned k=0;k<3;++k){const float t=g.at[k]-root[k];q+=t*t;}if(q<=r*r)return R::Held;}
        return hold;
    }
    // Immutable job for one actor: the same packet contents as GI actor capture
    // (snapshot owner, program, declaration, full constant bank, inverse view,
    // alpha texture copy). False: something is not reproducible on the CPU
    // (alpha: a cutout that cannot be (cutouts false: may not be) reproduced).
    bool persistentJob(const NorthlightPersistentCasters::Observation& o,NorthlightPersistentCasters::Job& job,size_t& textureBytes,bool& alpha,bool cutouts){
        textureBytes=0;alpha=false;job.packets.reserve(o.count);
        for(size_t i=o.first;i<o.end;++i){const auto& p=*replays[i];if(!persistentEligible(p))continue;
            const auto* program=persistentProgram(p.originalShader);const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;
            if(!program||!declarationCache.get(p.decl,elements,count)||!count)return false;
            NorthlightActorGeometry::Packet packet;packet.sharedMesh=p.shared;if(!packet.sharedMesh)packet.mesh=p.mesh();packet.position=*program;
            packet.elements.assign(elements,elements+count);std::memcpy(packet.constants.data(),p.constants,sizeof p.constantStorage);std::memcpy(packet.inverseView.data(),context.inverseView,64);
            if(p.cutoff>=0&&!NorthlightPersistentCasters::AlphaCutouts&&!cutouts){alpha=true;return false;}
            // Cutout (rigid props): the <=128 px copy of the replay's texture with its cutoff and address modes.
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
        const auto started=std::chrono::steady_clock::now();const DWORD now=GetTickCount();++persistentFrames;double diagMs=0; /* the diagnostics' own time this frame */
        try{
            if(lastRequest.map!=persistentMap){if(!persistentMap.empty())persistentCasters.clear();persistentMap=lastRequest.map;persistentMapMs=now;persistentWatch.clear();}
            persistentCasters.classes(quality.persistentCasters!=0,quality.persistentRigidProps!=0); /* settings are fixed after start-up: once */
            if(persistentWorker){persistentWorker->collect(persistentResults);
                for(auto& r:persistentResults){const auto id=r.id;const auto failure=r.failure;const unsigned bones=r.bones;const double ms=r.ms;
                    size_t vertices=r.mesh?r.mesh->vertices.size():0,triangles=r.mesh?r.mesh->indices.size()/3:0;
                    const bool accepted=persistentCasters.accept(std::move(r),now);
                    if(persistentLogged<200&&NorthlightDiagnostics::enabled()){++persistentLogged;const auto& all=persistentCasters.entries();auto e=std::lower_bound(all.begin(),all.end(),id,[](const auto& x,std::uint32_t k){return x.id<k;});
                        if(accepted&&e!=all.end()&&e->id==id)logf("PERSISTENT caster converted id=%u class=%s root=(%.1f %.1f %.1f) distance=%.1f draws=%u vertices=%zu triangles=%zu bones=%u cutouts=%zu bytes=%zu workerMs=%.2f",id,e->rigid?"rigid":"still",e->root[0],e->root[1],e->root[2],std::sqrt(e->distanceSquared),e->draws,vertices,triangles,bones,e->mesh?e->mesh->materials.size()-1:0,e->bytes,ms);
                        else logf("PERSISTENT caster rejected id=%u reason=%s bones=%u workerMs=%.2f",id,accepted?"gone":NorthlightPersistentCasters::failureName(failure),bones,ms);}}}
            persistentUploadStep();
            persistentScanLeft=PersistentScanVisits;if(quality.persistentRigidProps)persistentIndexStep();
            auto& obs=persistentObservations;obs.clear();persistentOneBone=0;persistentPoses.clear();persistentPoseAt.clear();persistentOthers.clear();persistentOthersOverflow=false;
            const float* pivot=actorShadowOriginValid?actorShadowOrigin:context.camera;
            persistentCasters.diagnostics(persistentDiag()?PersistentDiagRadius*1.6f:0.f,pivot); /* records near tracks only while on */
            for(size_t i=0;i<replays.size();){const unsigned group=replays[i]->constantGroup;size_t j=i;
                NorthlightPersistentCasters::Observation o;o.first=i;bool audited=true;const Replay* head=nullptr;
                for(;j<replays.size()&&replays[j]->constantGroup==group;++j){const auto& p=*replays[j];if(!persistentEligible(p))continue;
                    if(!persistentProgram(p.originalShader)){audited=false;continue;}
                    const auto key=persistentDrawKey(p);if(!head||key<o.key)o.key=key;if(!head)head=&p;
                    ++o.count;o.bytes+=p.mesh().byteSize();o.vertices+=p.mesh().vertexCount;o.triangles+=p.count;}
                o.end=j;i=j;
                // Unaudited groups, and audited ones without a finite root/basis: no observation, but located lazily they still hold (persistentBodyHeld).
                auto other=[&]{if(persistentOthers.size()<4096){PersistentOther g;g.first=o.first;g.end=o.end;persistentOthers.push_back(g);}else persistentOthersOverflow=true;};
                if(!audited)other();if(!head||!audited)continue;
                unsigned lanes=0;const auto* program=persistentProgram(head->originalShader,&lanes);
                if(!NorthlightActorDeformation::rootWorld(*program,head->constants,context.inverseView,o.root)){other();continue;}
                // Bone 0 basis: model axis a -> view (rows c31..c33, column a) -> world.
                const float* rows=head->constants+4*31;bool finite=true;
                for(unsigned a=0;a<3;++a)for(unsigned w=0;w<3;++w){const float v=rows[a]*context.inverseView[w]+rows[4+a]*context.inverseView[4+w]+rows[8+a]*context.inverseView[8+w];o.axes[a*3+w]=v;finite=finite&&std::isfinite(v);}
                if(!finite){other();continue;}
                float q=0;for(unsigned k=0;k<3;++k){const float t=o.root[k]-pivot[k];q+=t*t;}o.distanceSquared=q;
                persistentOneBone+=lanes==1;
                // Rigid props: a still root on a static-cache placement is screened once, before any pose work.
                if(quality.persistentRigidProps)persistentCasters.screen(o.key,o.root,[&]{return (!staticScene||staticScene->map!=lastRequest.map)?(now-persistentMapMs<15000?-1:0):persistentStaticCovered(o);});
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
                const auto& tuning=persistentCasters.tuning();persistentRigid.assign(obs.size(),-1);persistentBody.assign(obs.size(),PersistentBody{});
                if(persistentWorker->inFlight()<tuning.maxPending)for(size_t c:persistentCandidates){const auto& o=obs[c];
                    if(o.count>tuning.maxDraws||o.vertices>4*tuning.maxVertices){persistentCasters.reject(o,now,true,NorthlightPersistentCasters::Failure::Oversize);continue;}
                    // Rigid props convert only on complete captures (a held item's body may be a dropped draw),
                    // once this map's static scene is in (up to 15 s) and never when the static cache draws them.
                    if(o.rigid){if(captureShortfall||((!staticScene||staticScene->map!=lastRequest.map)&&now-persistentMapMs<15000)){persistentCasters.deferRigid(o,now,false);continue;}
                        const int covered=persistentStaticCovered(o);if(covered<0)continue; /* scene still being indexed */if(covered){persistentCasters.covered(o,now);continue;}
                        const auto hold=persistentBodyHeld(c);if(hold!=NorthlightPersistentCasters::Registry::Free){persistentCasters.deferRigid(o,now,true,hold==NorthlightPersistentCasters::Registry::Held);continue;}}
                    else if(persistentCasters.attachedToActor(obs,c,[&](size_t n){return persistentIsRigid(n);})){persistentCasters.defer(o,now,false);continue;}
                    const size_t geometry=size_t(std::min<size_t>(o.vertices,tuning.maxVertices))*sizeof(NorthlightGI::WorldVertex)+size_t(o.triangles)*3*2;
                    if(!persistentCasters.room(o,geometry,now)){persistentCasters.defer(o,now,true);break;} /* before any copy */
                    auto job=std::make_shared<NorthlightPersistentCasters::Job>();size_t textureBytes=0;
                    bool alpha=false;
                    if(!persistentJob(o,*job,textureBytes,alpha,o.rigid)){persistentCasters.reject(o,now,alpha,alpha?NorthlightPersistentCasters::Failure::Alpha:NorthlightPersistentCasters::Failure::Empty);continue;} /* alpha: permanent (still class: opaque only; rigid props: cutout not reproducible) */
                    float vertex[3];const bool hasVertex=persistentVertex(o,vertex);
                    const size_t estimate=geometry+textureBytes;
                    job->id=persistentCasters.begin(o,estimate,now,hasVertex?vertex:nullptr);
                    if(!job->id){persistentCasters.defer(o,now,true);break;}
                    job->maxVertices=tuning.maxVertices;job->bones=o.poseBones;persistentWorker->submit(std::move(job));break;}}
            persistentCasters.takeRemoved(persistentRemoved);for(auto id:persistentRemoved)persistentGpu.erase(id);persistentRemoved.clear();
            persistentCasters.takeEvents(persistentEvents);
            for(const auto& v:persistentEvents)if(persistentLogged<200&&NorthlightDiagnostics::enabled()){++persistentLogged;
                if(v.rejected){logf("PERSISTENT caster rejected reason=bones distinctBoneMatrices=%u characterBones=%u root=(%.1f %.1f %.1f)",v.bones,persistentCasters.tuning().characterBones,v.root[0],v.root[1],v.root[2]);continue;}
                logf("PERSISTENT caster removed id=%u class=%s reason=%s root=(%.1f %.1f %.1f) wasReady=%d",v.id,v.rigid?"rigid":"still",NorthlightPersistentCasters::removalName(v.reason),v.root[0],v.root[1],v.root[2],int(v.ready));}
            const auto diagStarted=persistentDiag()?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
            if(persistentDiag())try{persistentDiagnostics(now,pivot);}catch(...){persistentWatch.clear();} /* diagnostics never disable the casters */
            if(persistentDiag()){diagMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-diagStarted).count();persistentDiagPeakMs=std::max(persistentDiagPeakMs,diagMs);}
        }catch(...){
            for(auto& p:replays)p->persistentId=0;releasePersistentGPU();persistentWorker.reset();persistentDisabled=true;invalidateShadowCache();
            logf("PERSISTENT casters disabled: allocation failure; replay shadows retained");return;
        }
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();persistentFramePeakMs=std::max(persistentFramePeakMs,ms);
        if(captureSampled){const auto& s=persistentCasters.stats();using R=NorthlightPersistentCasters::Registry::State;
            logf("PERSISTENT casters ready=%zu converted=%zu pending=%zu bytes=%zu budget=%zu tracks=%u observed=%zu candidates=%zu markedDraws=%llu skippedReplayDraws=%llu submitted=%llu accepted=%llu uploads=%llu rejectedBones=%llu rejectedAlpha=%llu rejectedOther=%llu deferredAttach=%llu deferredBudget=%llu removed moved=%llu unseen=%llu range=%llu inView=%llu rekey=%llu shape=%llu evicted=%llu failed=%llu cleared=%llu workerMs=%.2f workerPeakMs=%.2f frameMs=%.3f framePeakMs=%.3f uploadPeakMs=%.3f diagMs=%.3f diagPeakMs=%.3f captureComplete=%d publications=%llu poseStillNow=%u attachBlockedNow=%u cacheRerendersSinceLog=%u classes=still:%d,rigid:%d rigidReady=%zu rigidPending=%zu rigidBytes=%zu rigidSubmitted=%llu rigidConverted=%llu rigidRemoved=%llu rigidHeld=%llu rigidShortfall=%llu rigidCovered=%llu rigidTeleport=%llu rigidScreened=%llu rigidWorn=%llu observedOneBone=%zu unaudited=%zu%s",
                persistentCasters.count(R::Ready),persistentCasters.count(R::Converted),persistentCasters.count(R::Pending),persistentCasters.usedBytes(),persistentCasters.tuning().budgetBytes,s.tracks,persistentObservations.size(),persistentCandidates.size(),persistentMarked,persistentSkipped,
                (unsigned long long)s.submitted,(unsigned long long)s.converted,(unsigned long long)s.uploads,(unsigned long long)s.rejectedBones,(unsigned long long)s.rejectedAlpha,(unsigned long long)s.rejectedOther,(unsigned long long)s.deferredAttach,(unsigned long long)s.deferredBudget,
                (unsigned long long)s.removed[0],(unsigned long long)s.removed[1],(unsigned long long)s.removed[2],(unsigned long long)s.removed[3],(unsigned long long)s.removed[4],(unsigned long long)s.removed[5],(unsigned long long)s.removed[6],(unsigned long long)s.removed[7],(unsigned long long)s.removed[8],
                s.workerMs,s.workerPeakMs,ms,persistentFramePeakMs,persistentUploadPeakMs,diagMs,persistentDiagPeakMs,int(!captureShortfall),(unsigned long long)s.publications,s.poseStill,s.attachBlocked,persistentRerenders,
                int(quality.persistentCasters!=0),int(quality.persistentRigidProps!=0),persistentCasters.countRigid(R::Ready),persistentCasters.countRigid(R::Pending)+persistentCasters.countRigid(R::Converted),persistentCasters.rigidBytes(),
                (unsigned long long)s.rigidSubmitted,(unsigned long long)s.rigidConverted,(unsigned long long)s.rigidRemoved,(unsigned long long)s.rigidHeld,(unsigned long long)s.rigidShortfall,(unsigned long long)s.rigidCovered,(unsigned long long)s.removed[NorthlightPersistentCasters::Teleport],(unsigned long long)s.rigidScreened,(unsigned long long)s.rigidWorn,persistentOneBone,persistentOthers.size(),persistentOthersOverflow?"+":"");
            if(persistentUploadPeakMs>0){const auto& k=persistentUploadPeak;
                logf("PERSISTENT upload peak ms=%.3f id=%u begin=%d admitMs=%.3f fullScans=%llu regionQueries=%llu createMs=%.3f textures=%u textureMs=%.3f copyBytes=%zu copyMs=%.3f vertexBytes=%zu indexBytes=%zu",
                    persistentUploadPeakMs,k.id,int(k.begin),k.admitMs,k.fullScans,k.regionQueries,k.createMs,k.textures,k.textureMs,k.copyBytes,k.copyMs,k.vertexBytes,k.indexBytes);}
            persistentMarked=persistentSkipped=0;persistentRerenders=0;persistentFramePeakMs=persistentUploadPeakMs=persistentDiagPeakMs=0;persistentUploadPeak=PersistentUploadPeak{};}
    }
    __attribute__((format(printf,2,3))) void persistentDiagLine(const char* format,...){if(persistentDiagLines.size()>=64){++persistentDiagSuppressed;return;}
        char line[1400];va_list a;va_start(a,format);std::vsnprintf(line,sizeof line,format,a);va_end(a);persistentDiagLines.emplace_back(line);}
    // Diagnostics: cache status of a ready caster per directional slot: committed in the slot's
    // content and its box inside the slot (then its replay draws are skipped there). 1: cached somewhere.
    int persistentDiagCache(std::uint32_t id,NorthlightGI::Vec3 low,NorthlightGI::Vec3 high,char* out=nullptr,size_t size=0){
        static const char* slots[4]={"sunNear","sunFar","moonNear","moonFar"};int cached=0;
        for(int slot=0;slot<4;++slot){const auto& k=shadowCacheKey[slot];if(!k.valid||!k.persistentContent.valid)continue;
            const bool committed=NorthlightPersistentCasters::committed(k.persistentContent,id),inside=StaticShadow::containsBounds(k.persistentContent.matrix,low,high);cached|=committed&&inside;
            if(out)NorthlightPersistentCasters::append(out,size," %s=%s",slots[slot],committed?(inside?"cached":"cachedEdge(replayKept)"):inside?"MISSING":"outside");}
        return cached;
    }
    // Diagnostics (no image change): rigid-class tracks (the one-influence program, a rigid caster,
    // or a still single-matrix pose) within PersistentDiagRadius of the pivot (the nearest
    // PersistentDiagWatch). Cache status is the last shadow pass's (it runs after this). Every 2nd frame:
    // state changes as "PERSISTENT near event" lines (with removals of near rigid casters, both
    // keys of a rekey; at most 20 lines a second, the rest counted); every 60th frame: a header,
    // the nearest rows and groups that give no observation (at most PersistentDiagShown lines).
    void persistentDiagnostics(DWORD now,const float* pivot){
        for(unsigned k=0;k<3&&!persistentDiagLines.empty();++k){logf("PERSISTENT near %s",persistentDiagLines.front().c_str());persistentDiagLines.pop_front();}
        ++persistentDiagFrames;persistentDiagShortfalls+=captureShortfall;
        using R=NorthlightPersistentCasters::Registry;using NorthlightPersistentCasters::append;const auto& reg=persistentCasters;const float r2=PersistentDiagRadius*PersistentDiagRadius;
        auto away2=[&](const float* a){float q=0;for(unsigned k=0;k<3;++k){const float t=a[k]-pivot[k];q+=t*t;}return q;};
        if(!persistentDiagStarted){persistentDiagStarted=true;const auto& t=reg.tuning();
            persistentDiagLine("thresholds radius=%.0f listEveryFrames=60 eventEveryFrames=2 cacheStatus=previousShadowPass(a new caster shows MISSING/notCached for one frame) stillMs=%u stillFrames=%u poseAfterFrames=%u stillTolerance=%.3f axisTolerance=%.3f poseTolerance=%.3f identityTolerance=%.3f moveTolerance=%.2f travelTolerance=%.2f turnTolerance=%.2f attachReach=%.1f attachMargin=%.2f attachSlack=%.2f attachRadius=%.1f deferMs=%u cooldownMs=%u publishMs=%u unseenMs=%u rigidUnseenMs=%u viewRange=%.0f absentInView=%u absentInViewMs=%u range=%.0f teleportDistance=%.0f rigidBudgetBytes=%zu rigidMaxCasters=%u maxDraws=%u maxVertices=%u maxPending=%u staticSceneWaitMs=15000",
                PersistentDiagRadius,t.stillMs,t.stillFrames,t.poseAfterFrames,t.stillTolerance,t.axisTolerance,t.poseTolerance,t.identityTolerance,t.moveTolerance,t.travelTolerance,t.turnTolerance,t.attachReach,t.attachMargin,t.attachSlack,t.attachRadius,
                t.deferMs,t.cooldownMs,t.publishMs,t.unseenMs,t.rigidUnseenMs,t.viewRange,t.absentInView,t.absentInViewMs,t.range,t.teleportDistance,t.rigidBudgetBytes,t.rigidMaxCasters,t.maxDraws,t.maxVertices,t.maxPending);}
        if(now-persistentDiagRefillMs>=1000){persistentDiagRefillMs=now;persistentDiagTokens=20;}
        auto token=[&]{if(persistentDiagTokens){--persistentDiagTokens;return true;}++persistentDiagSuppressed;return false;};
        for(const auto& v:persistentEvents){if(v.rejected)continue;auto old=persistentDiagParts.find(v.id);
            if(!(v.rigid&&away2(v.root)<=r2&&token())){if(old!=persistentDiagParts.end())persistentDiagParts.erase(old);continue;}char extra[400]="";
            if(v.reason==NorthlightPersistentCasters::Rekey){append(extra,sizeof extra," newKey=%08x(tri=%u draws=%u) part=%s dupDraw=%d",unsigned(v.other),v.otherTriangles,v.otherDraws,v.otherTriangles==v.triangles&&v.otherDraws==v.draws?"same":"other",int(v.otherKnown));
                PersistentKeyParts now_;bool fresh=false;for(const auto& o:persistentObservations)if(o.key==v.other&&away2(o.root)<=r2){float q=0;for(unsigned k=0;k<3;++k){const float t=o.root[k]-v.root[k];q+=t*t;}if(q<=.01f&&persistentKeyParts(o,now_)){fresh=true;break;}}
                R::Explained x;append(extra,sizeof extra," oldTrack=%d",int(reg.explain(v.key,v.root,now,0,x,false)));
                if(old==persistentDiagParts.end()||!fresh)append(extra,sizeof extra," changed=unknown(%s)",!fresh?"new draw not found":"old parts not recorded while near");
                else{const auto& a=old->second;append(extra,sizeof extra," changed=%s%s%s%s oldSnapshotAlive=%d oldVertices=%u newVertices=%u oldPrimitives=%u newPrimitives=%u",a.shader!=now_.shader?"shader,":"",a.decl!=now_.decl?"decl,":"",a.snapshot!=now_.snapshot?"snapshot,":"",
                    a.vertices!=now_.vertices||a.primitives!=now_.primitives||a.bytes!=now_.bytes?"counts,":"",int(!a.owner.expired()),a.vertices,now_.vertices,a.primitives,now_.primitives);}}
            if(old!=persistentDiagParts.end())persistentDiagParts.erase(old);
            persistentDiagLine("event t=%lu removed id=%u reason=%s key=%08x(tri=%u draws=%u) root=(%.1f %.1f %.1f) d=%.1f wasReady=%d%s",(unsigned long)now,v.id,NorthlightPersistentCasters::removalName(v.reason),unsigned(v.key),v.triangles,v.draws,v.root[0],v.root[1],v.root[2],std::sqrt(away2(v.root)),int(v.ready),extra);}
        R::Explained e;
        if(persistentDiagParts.size()>512)persistentDiagParts.clear();
        const bool list=persistentFrames%60==0;if(!list&&(persistentFrames&1))return;
        // Phases first (no text); reasons are formatted only for the lines printed.
        auto& rows=persistentRows;rows.clear();
        auto add=[&](std::uint64_t key,const float* root,const float* axes,unsigned triangles,unsigned draws,unsigned bones,unsigned lanes,unsigned marked,const R::Explained& e){
            if(rows.size()>=4*PersistentDiagWatch)return;for(const auto& r:rows)if(r.serial==e.serial)return;
            rows.emplace_back();auto& r=rows.back();r.serial=e.serial;r.key=key;std::memcpy(r.root,root,12);r.distance=std::sqrt(away2(root));r.triangles=triangles;r.draws=draws;r.bones=bones?bones:e.bones;r.lanes=lanes;r.marked=marked;r.seen=e.seen;r.why=e;if((r.hasAxes=axes!=nullptr))std::memcpy(r.axes,axes,36);
            if(e.caster&&(e.phase==R::Phase::Ready||e.phase==R::Phase::Published))r.cached=persistentDiagCache(e.caster,e.low,e.high);};
        auto relevant=[](unsigned lanes,const R::Explained& e){return lanes==1||e.rigid||(e.single&&!e.mobile&&!e.multi);};
        const auto& obs=persistentObservations;
        for(const auto& o:obs){if(!(o.distanceSquared<=r2))continue;unsigned lanes=0;
            for(size_t i=o.first;i<o.end&&!lanes;++i){const auto& p=*replays[i];if(persistentEligible(p))persistentProgram(p.originalShader,&lanes);}
            if(reg.explain(o.key,o.root,now,o.count,e,false)&&relevant(lanes,e)){add(o.key,o.root,o.axes,o.triangles,unsigned(o.count),o.poseBones,lanes,o.caster?unsigned(o.count):0,e);
}}
        for(const auto& o:obs)if(o.caster)persistentKeyParts(o,persistentDiagParts[o.caster]); /* every drawn caster: a later rekey names what changed */
        // Watched tracks not drawn now (camera turned away): the same track by key and root.
        for(const auto& w:persistentWatch){bool present=false;for(const auto& r:rows)present=present||r.serial==w.serial;if(present)continue;
            if(!reg.explain(w.key,w.root,now,0,e,false)||e.serial!=w.serial){if(token())persistentDiagLine("event t=%lu #%u key=%08x root=(%.1f %.1f %.1f) %s -> gone(track forgotten or replaced)",(unsigned long)now,w.serial,unsigned(w.key),w.root[0],w.root[1],w.root[2],R::phaseName(w.why.phase));continue;}
            if(away2(w.root)<=r2)add(w.key,w.root,w.hasAxes?w.axes:nullptr,w.triangles,w.draws,w.bones,w.lanes,0,e);}
        if(list)reg.tracksNear(pivot,PersistentDiagRadius,[&](std::uint64_t key,const float* root,std::uint32_t){if(reg.explain(key,root,now,0,e,false)&&!e.seen&&relevant(0,e))add(key,root,nullptr,0,0,0,0,0,e);});
        std::sort(rows.begin(),rows.end(),[](const PersistentWatch& a,const PersistentWatch& b){return a.distance<b.distance;});if(rows.size()>PersistentDiagWatch)rows.resize(PersistentDiagWatch);
        static const char* laneNames[5]={"?","1-bone","?","?","4-weight"};
        auto describe=[&](PersistentWatch& r,char* out,size_t size){
            if(!r.why.text[0]){const auto phase=r.why.phase;if(reg.explain(r.key,r.root,now,r.seen?r.draws:0,e)&&e.serial==r.serial){r.why=e;r.why.phase=phase;}
                if(r.cached>=0&&!r.cache[0])persistentDiagCache(r.why.caster,r.why.low,r.why.high,r.cache,sizeof r.cache);}
            append(out,size,"#%u key=%08x root=(%.1f %.1f %.1f) d=%.1f tri=%u draws=%u bones=%u/%u class=%s seen=%d state=%s: %s",r.serial,unsigned(r.key),r.root[0],r.root[1],r.root[2],r.distance,r.triangles,r.draws,r.bones,r.why.distinct,
                laneNames[r.lanes<5?r.lanes:0],int(r.seen),R::phaseName(r.why.phase),r.why.text);
            if(r.cached>=0)append(out,size," cache:%s marked=%u gpu=%d",r.cache,r.marked,int(persistentGpu.count(r.why.caster)!=0));
            if(!r.seen){unsigned other=0;for(const auto& o:obs){float q=0;for(unsigned k=0;k<3;++k){const float t=o.root[k]-r.root[k];q+=t*t;}
                    if(q<=.01f&&o.key!=r.key&&other++<2)append(out,size," seenUnderKey=%08x(tri=%u draws=%zu)",unsigned(o.key),o.triangles,o.count);}
                if(!other)append(out,size," notDrawnNow");}
            // Covered: the matched static placement, and whether the static scene has geometry for its model.
            if(r.why.phase==R::Phase::Covered&&r.hasAxes&&staticScene&&staticScene->map==lastRequest.map&&persistentIndex.scene==staticScene.get()&&persistentIndex.complete){size_t hit=SIZE_MAX;
                persistentIndex.find(r.root,.05f,[&](std::uint32_t i){const auto& place=staticScene->placements[i];const float t[3]={place.translation.x,place.translation.y,place.translation.z};
                    if(!NorthlightPersistentCasters::staticPlacement(r.root,r.axes,t,place.matrix))return false;hit=i;return true;});
                if(hit==SIZE_MAX)append(out,size," placement=none(now)");
                else{const auto& place=staticScene->placements[hit];auto model=staticScene->models.find(place.modelKey);const bool known=model!=staticScene->models.end()&&model->second;
                    append(out,size," placement uid=%llu category=%u modelKey=%s staticModel=%s(not a check that the cache draws it) indices=%zu batches=%zu",(unsigned long long)place.uid,place.category,place.modelKey.c_str(),
                        !known?"missing":model->second->indexCount()&&!model->second->batches.empty()?"loadedWithGeometry":"loadedEmpty",known?model->second->indexCount():size_t(0),known?model->second->batches.size():size_t(0));}}};
        // State changes (phase, or a ready caster's cache status) of rows already watched; new rows unless merely settling/static/moving.
        for(auto& r:rows){const PersistentWatch* old=nullptr;for(const auto& w:persistentWatch)if(w.serial==r.serial)old=&w;
            const bool quiet=r.why.phase==R::Phase::Moving||r.why.phase==R::Phase::Covered||r.why.phase==R::Phase::Mobile||r.why.phase==R::Phase::Multi;
            if(old?(old->why.phase==r.why.phase&&old->cached==r.cached):quiet)continue;if(!token())continue;
            char line[1024]="";describe(r,line,sizeof line);
            persistentDiagLine("event t=%lu %s%s -> %s",(unsigned long)now,old?R::phaseName(old->why.phase):"new",old&&old->cached>=0?(old->cached?"(cached)":"(notCached)"):"",line);}
        persistentWatch.swap(rows);
        if(!list)return;
        // Groups near the pivot that give no observation: no eligible draw, a draw outside the audited
        // programs, or no finite root. Located by an audited draw's root, else (at most 16 a list) a sampled vertex.
        auto& un=persistentUnobserved;un.clear();unsigned sampled=16;
        for(size_t i=0,next=0;i<replays.size();){const unsigned group=replays[i]->constantGroup;size_t j=i;PersistentUnobserved g;g.first=i;const Replay* head=nullptr;const Replay* any=nullptr;
            for(;j<replays.size()&&replays[j]->constantGroup==group;++j){const auto& p=*replays[j];++g.draws;g.triangles+=p.count;g.selected+=p.shadowSelected;g.skinned+=p.shadowSkinned;g.eligible+=persistentEligible(p);unsigned lanes=0;persistentProgram(p.originalShader,&lanes);
                if(lanes){if(!head)head=&p;g.oneBone+=lanes==1;}else if(persistentEligible(p)){++g.unaudited;if(!any)any=&p;}}
            while(next<obs.size()&&obs[next].first<i)++next;const bool observed=next<obs.size()&&obs[next].first==i;i=j;if(observed||(!g.oneBone&&!g.unaudited))continue;
            if(head){const auto* program=persistentProgram(head->originalShader);g.rooted=program&&NorthlightActorDeformation::rootWorld(*program,head->constants,context.inverseView,g.at);}
            if(!g.rooted&&any){bool found=false;for(const auto& o:persistentOthers)if(o.first==g.first&&o.located==1){std::memcpy(g.at,o.at,12);found=true;}
                if(!found&&sampled){--sampled;auto program=actorPrograms.find(any->originalShader);const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;float squared=0;
                    found=program!=actorPrograms.end()&&declarationCache.get(any->decl,elements,count)&&sampledVertices.distance(program->second,any->mesh(),any->shared,any->decl,elements,count,any->constants,context.inverseView,context.camera,squared,g.at);}
                if(!found)continue;}
            else if(!g.rooted)continue;
            if(!(away2(g.at)<=r2))continue;g.distance=std::sqrt(away2(g.at));un.push_back(g);}
        std::sort(un.begin(),un.end(),[](const PersistentUnobserved& a,const PersistentUnobserved& b){return a.distance<b.distance;});
        // Header: cache status of every rigid caster in the sun slots (MISSING: box inside the slot, not in its content).
        size_t ready=0,cached[2]={},missing[2]={};
        for(const auto& x:reg.entries()){if(!x.rigid||x.state!=R::State::Ready||!x.published)continue;++ready;
            for(int slot=0;slot<2;++slot){const auto& k=shadowCacheKey[slot];if(!k.valid||!k.persistentContent.valid||!StaticShadow::containsBounds(k.persistentContent.matrix,x.low,x.high))continue;
                if(NorthlightPersistentCasters::committed(k.persistentContent,x.id))++cached[slot];else ++missing[slot];}}
        if(persistentWatch.empty()&&un.empty()&&!ready)return;
        const size_t showGroups=std::min(un.size(),std::max<size_t>(4,PersistentDiagShown-std::min(persistentWatch.size(),PersistentDiagShown))),shown=std::min(persistentWatch.size(),PersistentDiagShown-showGroups);
        persistentDiagLine("list t=%lu frame=%u pivot=(%.1f %.1f %.1f) radius=%.0f complete=%d shortfallStreak=%u shortfallFrames=%u/%u staticScene=%s workerInFlight=%zu tracks=%u rows=%zu unobserved=%zu eventsSuppressed=%u rigidPublished=%zu sunNearCached=%zu sunNearMissing=%zu sunFarCached=%zu sunFarMissing=%zu",
            (unsigned long)now,persistentFrames,pivot[0],pivot[1],pivot[2],PersistentDiagRadius,int(!captureShortfall),reg.shortfallFrames(),persistentDiagShortfalls,persistentDiagFrames,
            (!staticScene||staticScene->map!=lastRequest.map)?(now-persistentMapMs<15000?"waiting":"none"):persistentIndex.complete?"indexed":"indexing",persistentWorker?persistentWorker->inFlight():size_t(0),
            reg.stats().tracks,persistentWatch.size(),un.size(),persistentDiagSuppressed,ready,cached[0],missing[0],cached[1],missing[1]);
        persistentDiagSuppressed=0;persistentDiagShortfalls=persistentDiagFrames=0;
        for(size_t k=0;k<shown;++k){char line[1024]="";describe(persistentWatch[k],line,sizeof line);persistentDiagLine("  %s",line);}
        for(size_t k=0;k<showGroups;++k){const auto& g=un[k];
            persistentDiagLine("  group first=%zu root=(%.1f %.1f %.1f) d=%.1f tri=%u draws=%u oneBoneDraws=%u state=notObserved: %s selected=%u/%u skinned=%u/%u unauditedDraws=%u located=%s",
                g.first,g.at[0],g.at[1],g.at[2],g.distance,g.triangles,g.draws,g.oneBone,
                g.unaudited?"unauditedProgram(never observed; holds rigid candidates within attachReach)":!g.eligible?"ineligible(no draw shadowSelected+skinned)":"noRoot(root or basis not finite)",
                g.selected,g.draws,g.skinned,g.draws,g.unaudited,g.rooted?"root":"sampledVertex");}
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
