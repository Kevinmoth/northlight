#pragma once
#include "world_context.h"
#include "world_camera.h"
#include "wmo_context.h"
#include "projection.h"
#include "celestial_context.h"
#include "celestial_sources.h"
#include "twilight_fill.h"
#include "regional_fog.h"
#include "regional_shadow_range.h"
#include "celestial_profiles.h"
#include "world_gi.h"
#include "world_local_lights.h"
#include "local_light_selection.h"
#include "point_light_shadow.h"
#include "local_light_compiled_shaders.h"
#include "replay_bounds.h"
#include "replay_bounds_schedule.h"
#include "replay_bounds_metadata.h"
#include <atomic>
#include "world_math.h"
#include "world_mesh_plan.h"
#include "world_streaming.h"
#include "world_mesh_pages.h"
#include "world_mesh_page_gpu.h"
#include "streaming_budget.h"
#include "streaming_phase_profile.h"
#include "cpu_retirement.h"
#include "deferred_gpu_release.h"
#include "geometry_memory.h"
#include <chrono>
#include "world_probe_cache.h"
#include "world_probe_progress.h"
#include "probe_activation.h"
#include "patch_terrain_shadow.h"
#include "celestial_time_warp.h"
#include "world_dynamic_probes.h"
#include "shadow_bounds.h"
#include "legacy_fog.h"
#include "replay_constant_ranges.h"
#include "replay_pose_groups.h"
#include "replay_capture_constants.h"
#include "shader_constant_usage.h"
#include "capture_phase_profile.h"
#include "world_compiled_shaders.h"
#include "patch_shadow_shader.h"
#include "terrain_capture_bounds.h"
#include "draw_snapshot.h"
#include "replay_gpu_cache.h"
#include "vertex_declaration_cache.h"
#include "replay_draw_state.h"
#include "actor_deformation.h"
#include "actor_texture.h"
#include "actor_scene_job.h"
#include "worker_actor_memo.h"
#include <set>
#include <unordered_set>
#include <map>
#include <tuple>
#include "world_shader_signatures.h"
#include <algorithm>
#include <array>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <stdexcept>
#include "world_diagnostics.h"
#include "shadow_terrain.h"
#include "live_terrain_gpu.h"
#include "static_shadow_request.h"
#include "static_shadow_gpu.h"
#include "static_shadow_dedup.h"
#include "static_shadow_coverage.h"
#include "celestial_terrain.h"

// Included after SavedState. Worker never accesses D3D or game memory.
class WorldRenderer {
    using V=NorthlightGI::Vec3;
    struct Snapshot {
        std::shared_ptr<NorthlightGI::BVH> bvh;
        std::shared_ptr<NorthlightWorldMesh::WorldMeshUploadPlan> meshPlan;
        std::vector<NorthlightGI::ProbeAtlasEntry> atlas;
        std::vector<NorthlightLocalLights::Light> localLights;
        V origin, center;
        std::string map, message;
        uint64_t serial=0;
        std::shared_ptr<const NorthlightRegionalFog::Field> fogField;
        unsigned validProbes=0;
        uint64_t requestId=0;
        DWORD requestedAt=0,queueMs=0,geometryMs=0,actorMs=0,solveMs=0;
        size_t retirementBytes=0;
        unsigned reusedProbes=0,solvedProbes=0,superseded=0,dynamicReused=0,dynamicSolved=0;
        unsigned processedProbes=0,retargeted=0;
        uint64_t lightingGeneration=0;
        bool staticOnly=false,partial=false;
    };
    struct Request { std::shared_ptr<const NorthlightActorGeometry::ActorJob> actorJob;uint64_t actorSerial=0; V camera; NorthlightGI::Lighting light; std::string map; uint64_t id=0,baseId=0;DWORD queuedAt=0;unsigned reason=0; };
    IDirect3DDevice9* d;
    std::string root;
    std::unique_ptr<StaticShadow::Streamer> staticStream;
    StaticShadow::GpuCache staticCasters;
    std::shared_ptr<const StaticShadow::Snapshot> staticScene;
    StaticShadowDedup::Matcher staticMatcher;
    V staticFramePivot;
    bool staticPivotReady=false,staticAllowLoads=true;
    DWORD staticMemoryTick=0,staticLogTick=0,staticRetryTick=0;
    uint64_t staticFrame=0,staticAdmissionFrame=UINT64_MAX,staticReservedBytes=0;
    NorthlightGeometryMemory::Sample staticMemorySample;
    unsigned staticDrawFailures=0,staticDedupSkipped=0,staticDedupCursor=0,staticDedupAttempts=0,staticDedupMatches=0;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable wake;
    bool stopping=false,pending=false;
    std::atomic<bool> workerBusy{false};DWORD actorRequestTick=0;
    std::atomic<unsigned> workerFaultCode{0};
public:
    // Read-only access to the COMMITTED terrain pages. Never start uploads,
    // loads, shadow updates or mesh publication from the native sky draw hook.
    uint64_t celestialTerrainGeneration()const{
        char map[64]={};float camera[3];
        if(failed||activeMesh<0||batches.empty()||!NorthlightWorldContext::readMapAndCamera(map,camera)||uploadedMap!=map)return 0;
        return meshGeneration+1;
    }
    bool drawCelestialTerrain(const float* matrix){
        if(!celestialTerrainGeneration())return false;
        UINT page=UINT_MAX;unsigned draws=0;uint64_t triangles=0;
        const NorthlightCelestialTerrain::Frustum frustum(matrix);
        for(const auto& b:batches){
            if(!b.terrain||frustum.reject(b.boundsLow,b.boundsHigh))continue;
            if(!bindMeshPage(b,page)||FAILED(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,b.minVertex,b.vertexCount,b.start,b.count)))return false;
            ++draws;triangles+=b.count;
        }
        static DWORD lastLog=0;const DWORD now=GetTickCount();
        if(!lastLog||now-lastLog>=10000){lastLog=now;logf("CELESTIAL terrain mask map=%s generation=%llu draws=%u triangles=%llu",uploadedMap.c_str(),(unsigned long long)meshGeneration,draws,(unsigned long long)triangles);}
        return true;
    }
    unsigned workerFault()const noexcept {return workerFaultCode.load();}
    static const char* workerFaultMessage(unsigned code)noexcept {
        return code==1?"GI worker allocation failed":code==2?"GI worker exception":code==3?"GI worker unknown exception":"";
    }
private:
    Request request,lastRequest;
    std::shared_ptr<Snapshot> published,active;
    std::weak_ptr<Snapshot> observedPublication;
    std::weak_ptr<NorthlightGI::BVH> uploaded; // GPU commit must not retain the CPU scene.
    std::vector<float> uploadedAlphaCutoffs;
    uint64_t uploadedSerial=0;
    std::vector<StaticShadow::Placement> uploadedStaticOwners;
    uint64_t staticOwnerGeneration=UINT64_MAX;
    uint64_t meshGeneration=0; // committed mesh identity; content signatures decide shadow reuse
    NorthlightProbeActivation probeActivation;
    bool valid=false,failed=false,reportedContext=false;
    unsigned contextRejects=0,frames=0,slowReports=0;
    DWORD diagnosticTick=0;
    bool gpuDiagnosticArmed=true;unsigned gpuDiagnosticCaptures=0;
    std::string gpuDiagnosticDirectory;
    IDirect3DTexture9* regionalFogTexture=nullptr;
    std::shared_ptr<const NorthlightRegionalFog::Field> uploadedFogField;
    NorthlightWorldContext::TerrainContext context;
    NorthlightCelestial::Context celestial,celestialLight;
    unsigned celestialOrbitReports=0;
    NorthlightCelestialOrbit::LightMotion celestialLightMotion;
    std::string celestialLightMap;V celestialLightCamera;
    bool continuousCelestialShadows=false;
    NorthlightCelestialProfiles::Table celestialProfiles;
    NorthlightRegionalShadow::Table shadowRanges;
    NorthlightCelestialProfiles::Transition paletteTransition;
    NorthlightCelestialProfiles::Profile framePalette;
    bool paletteFrameValid=false;std::string paletteFrameMap;DWORD paletteLogAt=0;
    struct PaletteRegion {std::string map;NorthlightRegionalFog::Region region;int tx=0,ty=0;};
    std::shared_ptr<const PaletteRegion> publishedPaletteRegion; // guarded by mutex
    bool celestialValid=false,shadowFrameReady=false;float authoredFill=1;
    V sourceDirections[2],sourceColors[2];
    float sourceWeights[2]={1,0};
    float sourceMatrices[2][2][16]={};
    DWORD animationEpoch=GetTickCount();
    NorthlightLegacyFog::Constants legacyFog;
    struct FogShader { unsigned major=0;int colorRegister=-1;bool verified=false; };
    std::unordered_map<IDirect3DPixelShader9*,FogShader> fogShaders;
    unsigned fogReports=0;
    float projection[3]={1,1,1};
    IDirect3DTexture9 *shadow[4]={},*probe[5]={},*light=nullptr,*fog=nullptr,*fogBlurred=nullptr,*color=nullptr,*baselineLight=nullptr;
    IDirect3DTexture9 *normalBuffer=nullptr;IDirect3DSurface9* normalSurface=nullptr;
    IDirect3DTexture9* sourceVis[2][2]={};IDirect3DSurface9* sourceVisSurface[2][2]={};unsigned sourceVisIndex=0;bool sourceVisValid=false;
    IDirect3DTexture9 *temporalLight[2]={},*temporalDepth[2]={};IDirect3DSurface9 *temporalLightSurface[2]={},*temporalDepthSurface[2]={};
    unsigned temporalIndex=0;bool temporalValid=false;float previousView[16]={};V previousCamera;std::string temporalMap;
    IDirect3DSurface9 *shadowSurface[4]={},*shadowDepth=nullptr,*lightSurface=nullptr,*fogSurface=nullptr,*fogBlurredSurface=nullptr,*colorSurface=nullptr,*baselineSurface=nullptr;
    // Static shadow cache: per (source,cascade) an R32F map with a 128-texel
    // margin holding static batches only, re-rendered when the snapped light
    // origin travels beyond the margin or the geometry/direction key changes.
    // Per frame only live terrain and replays are drawn (scratch) and united.
    static constexpr UINT ShadowCacheMargin=128,ShadowCacheSize=1024+2*ShadowCacheMargin;
    IDirect3DTexture9 *shadowCache[4]={},*shadowScratch=nullptr;
    IDirect3DSurface9 *shadowCacheSurface[4]={},*shadowCacheDepth=nullptr,*shadowScratchSurface=nullptr;
    IDirect3DPixelShader9* unionPS=nullptr;
    std::shared_ptr<const NorthlightLocalShadowSignature::Records> uploadedLocalShadowRecords;
    struct ShadowCacheKey {
        bool valid=false,localContentKnown=false;NorthlightWorldMath::ShadowFrame frame;V direction;
        uint64_t serial=0,chunkHash=0,staticSignature=0;
        NorthlightLocalShadowSignature::Digest localContent;
        NorthlightLocalShadowSignature::Memo localMemo;
    };
    ShadowCacheKey shadowCacheKey[4];
    DWORD shadowCacheLogAt=0,shadowPhaseLogAt=0;unsigned shadowCacheRenders=0,shadowCacheReuses=0,culledReplayDraws=0;
    void invalidateShadowCache(){for(auto& k:shadowCacheKey)k.valid=false;pointCacheValid=false;}
    uint64_t liveChunkHash()const{uint64_t h=1469598103934665603ull;for(const auto& c:liveTerrainChunks){h^=uint64_t(uint32_t(c.first))*0x9E3779B97F4A7C15ull+uint64_t(uint32_t(c.second));h*=1099511628211ull;}return h;}
    IDirect3DPixelShader9 *cachedFastPS=nullptr,*cachedOpaqueFastPS=nullptr,*cachedOpaquePS=nullptr;
    IDirect3DPixelShader9 *lightingPS=nullptr,*giPS=nullptr,*fogPS=nullptr,*fogBlurPS=nullptr,*localDirectPS=nullptr,*temporalPS=nullptr,*localFogPS=nullptr,*normalsPS=nullptr,*sourceVisPS=nullptr,*finalPS=nullptr,*shadowPS=nullptr,*cachedShadowPS=nullptr,*replayPS=nullptr;
    unsigned localDirectCount=0;float localDirectNearest=0;
    IDirect3DVertexShader9 *shadowVS=nullptr,*cachedShadowVS=nullptr;
    IDirect3DVertexDeclaration9* shadowDecl=nullptr;
    IDirect3DVertexBuffer9* vertices=nullptr; // non-owning alias of meshPool[activeMesh]
    IDirect3DIndexBuffer9* indices=nullptr;
    // Two persistent page pools: the inactive generation stages bounded static
    // resources, and all pages become drawable together after complete upload.
    struct MeshBuffers {IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;UINT vbCapacity=0,ibCapacity=0;};
    std::vector<MeshBuffers> meshPool[2];int activeMesh=-1;
    DWORD memoryPressureLogAt=0,memoryPressureUntil=0;
    int stagingSlot()const{return activeMesh<0?0:1-activeMesh;}
    static UINT roundBuffer(uint64_t bytes,uint64_t stepBytes){return UINT(NorthlightGeometryMemory::roundUp(bytes,stepBytes));}
    // Growth admission for any large GPU buffer. A refused growth is remembered
    // for one second so pressure does not add an address-space walk per frame.
    bool admitsGrowth(const char* stage,uint64_t capacityBytes,const NorthlightGeometryMemory::Sample* cachedSample=nullptr){
        DWORD now=GetTickCount();
        if(memoryPressureUntil&&now<memoryPressureUntil)return false;
        auto memory=cachedSample?*cachedSample:geometryMemory();
        if(NorthlightGeometryMemory::admits(memory,NorthlightGeometryMemory::dynamicBudget(capacityBytes)))return true;
        memoryPressureUntil=now+1000;
        if(!memoryPressureLogAt||now-memoryPressureLogAt>=60000){memoryPressureLogAt=now;logGeometryMemory(stage,memory);}
        return false;
    }
    bool acquireMeshPage(size_t page,bool vertex,UINT bytes,NorthlightGeometryMemory::Sample& memory){
        MeshBuffers& slot=meshPool[stagingSlot()][page];
        if(vertex?slot.vb&&slot.vbCapacity>=bytes:slot.ib&&slot.ibCapacity>=bytes)return true;
        // At most 512 KiB per resource, including rounded unused capacity.
        const UINT capacity=roundBuffer(bytes,64u<<10);
        if(!NorthlightGeometryMemory::admitsSmallPageGrowth(memory.valid,memory.available,capacity))return false;
        if(vertex){
            drop(slot.vb);slot.vbCapacity=0;
            if(FAILED(d->CreateVertexBuffer(capacity,D3DUSAGE_WRITEONLY,0,D3DPOOL_MANAGED,&slot.vb,nullptr))||!slot.vb){drop(slot.vb);return false;}
            slot.vbCapacity=capacity;
        }else{
            drop(slot.ib);slot.ibCapacity=0;
            if(FAILED(d->CreateIndexBuffer(capacity,D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,D3DPOOL_MANAGED,&slot.ib,nullptr))||!slot.ib){drop(slot.ib);return false;}
            slot.ibCapacity=capacity;
        }
        NorthlightGeometryMemory::debitGrowth(memory,capacity);return true;
    }
    void releaseMeshPool(){for(auto& pool:meshPool){for(auto& slot:pool){drop(slot.vb);drop(slot.ib);}pool.clear();}activeMesh=-1;vertices=nullptr;indices=nullptr;}
    std::vector<IDirect3DTexture9*> materials;
    NorthlightStreaming::DeferredRelease<IDirect3DTexture9> retiredMaterials;
    size_t uploadedTextureBytes=0;
    using Batch=NorthlightWorldMeshPages::Batch;
    bool bindMeshPage(const Batch& batch,UINT& boundPage){
        if(boundPage==batch.page)return true;
        if(activeMesh<0||batch.page>=meshPool[activeMesh].size())return false;
        auto& page=meshPool[activeMesh][batch.page];
        if(!page.vb||!page.ib||FAILED(d->SetStreamSource(0,page.vb,0,sizeof(NorthlightGI::WorldVertex)))||FAILED(d->SetIndices(page.ib)))return false;
        boundPage=batch.page;return true;
    }
    std::vector<Batch> batches;
    std::set<std::pair<int,int>> fixedTerrainChunks;
    std::string uploadedMap;
    IDirect3DIndexBuffer9* liveIndicesGPU=nullptr;
    UINT liveIndexBytes=0;
    struct PendingMesh {
        std::shared_ptr<NorthlightGI::BVH> bvh;
        std::shared_ptr<NorthlightWorldMesh::WorldMeshUploadPlan> plan;
        std::string map;
        NorthlightWorldMeshPages::Upload upload;
        IDirect3DTexture9* white=nullptr;
        std::vector<IDirect3DTexture9*> textures;
        size_t material=0,materialRow=0;
        IDirect3DTexture9* partialTexture=nullptr;
        DWORD started=GetTickCount();unsigned frames=0,textureUploads=0;
        // The upload cursor owns no GPU data; only textures are owned here.
        ~PendingMesh(){drop(partialTexture);drop(white);for(auto& t:textures)drop(t);}
    };
    std::unique_ptr<PendingMesh> pendingMesh;
    NorthlightWorldStreaming::Retry meshRetry;
    unsigned streamingReports=0;
    double streamingCpuPeakMs=0;unsigned streamingBudgetOverruns=0;
    NorthlightStreaming::PhaseProfile streamingPhases;
    NorthlightVertexDeclarations::Cache declarationCache;
    NorthlightReplayBounds::Cache replayBoundsCache;
    NorthlightReplayMetadata::Cache<NorthlightReplayBounds::Prepared,IDirect3DVertexShader9,IDirect3DVertexDeclaration9> replayBoundsMetadata;
    std::unordered_set<IDirect3DVertexShader9*> terrainShaders;
    NorthlightLiveTerrainGPU::Cache<NorthlightTerrainCapture::MeshSnapshot,NorthlightGI::WorldVertex> liveTerrainGPU;
    std::vector<uint32_t> liveTerrainIndices;
    std::vector<uint32_t> liveDirectionalIndices;
    uint64_t liveTerrainGeneration=0;
    // Ordered immutable captures preserve exact point-light batch offsets.
    // Vertex residency is per capture; the compact active index stream keeps
    // the directional pass at one draw even when residency is fragmented.
    using TerrainSnapshot = std::shared_ptr<const NorthlightTerrainCapture::MeshSnapshot>;
    std::vector<TerrainSnapshot> frameTerrain, uploadedTerrain;
    size_t frameTerrainVertices=0,frameTerrainIndices=0,terrainUploadBytes=0;
    bool terrainUploadReused=false;
    NorthlightStateBlockPool stateBlocks;
    unsigned terrainAttempts=0,terrainSnapshots=0,terrainFailures=0;
    NorthlightTerrainCapture::FrameCache terrainBoundsCache;
    std::set<std::pair<int,int>> liveTerrainChunks;
    UINT width=0,height=0,vertexCount=0;
    struct Replay {
        IDirect3DVertexShader9* shader=nullptr,*originalShader=nullptr;
        NorthlightReplayBounds::Bounds pointBounds;
        NorthlightReplayBounds::WorkInfo boundsWork; // scheduling hint; cleared every frame, never an identity proof
        std::shared_ptr<const NorthlightReplayBounds::Prepared> boundsPrepared;
        IDirect3DVertexDeclaration9* decl=nullptr;
        IDirect3DVertexBuffer9* stream[4]={};
        UINT offset[4]={},stride[4]={};
        IDirect3DIndexBuffer9* index=nullptr;
        IDirect3DBaseTexture9* texture=nullptr;
        float constantStorage[1024]={};BOOL boolStorage[16]={};int intStorage[64]={};
        const float* constants=constantStorage;const BOOL* bools=boolStorage;const int* ints=intStorage;
        NorthlightConstantEpoch::Stamp constantStamp;
        Replay()=default;Replay(const Replay&)=delete;Replay& operator=(const Replay&)=delete;
        float cutoff=-1;
        NorthlightShaderConstants::Usage constantUsage;
        unsigned constantGroup=0; // exact consecutive banks, rebuilt from this frame's captures
        unsigned projectionKind=2;DWORD addressU=D3DTADDRESS_WRAP,addressV=D3DTADDRESS_WRAP;
        NorthlightDrawSnapshot::Mesh snapshot; // owned copy: DYNAMIC/UP draws and misses the cache could not store
        std::shared_ptr<const NorthlightDrawSnapshot::Mesh> shared; // immutable snapshot: tracked generations or legacy static cache
        const NorthlightDrawSnapshot::Mesh& mesh()const{return shared?*shared:snapshot;}
        bool gpuCached=false;
        uint32_t staticProofMask=0;
        V staticProofLow,staticProofHigh;
        D3DPRIMITIVETYPE type;INT base;UINT min,vertices,start,count;bool indexed;
        void releaseResources(bool retainSnapshot=false){NorthlightReplayCaptureConstants::reset(*this);drop(shader);drop(originalShader);pointBounds={};boundsWork={};boundsPrepared.reset();drop(decl);drop(index);drop(texture);for(auto& s:stream)drop(s);shared.reset();if(!retainSnapshot)snapshot=NorthlightDrawSnapshot::Mesh{};}
        ~Replay(){releaseResources();}
    };
    std::vector<std::unique_ptr<Replay>> replays,freeReplays;
    size_t pooledSnapshotBytes=0;
    NorthlightDrawSnapshot::Frame replaySnapshots;
    NorthlightReplayGPU::Cache replayGpuCache;
    IDirect3DVertexBuffer9* replayVerticesGPU[4]={};
    IDirect3DIndexBuffer9* replayIndicesGPU=nullptr;
    UINT replayVertexBytes[4]={},replayIndexBytes=0;
    std::unordered_map<IDirect3DVertexShader9*,NorthlightActorDeformation::Program> actorPrograms,actorUVPrograms;
    std::shared_ptr<NorthlightActorGeometry::ActorJob> actorJob=std::make_shared<NorthlightActorGeometry::ActorJob>();
    std::shared_ptr<const NorthlightActorGeometry::ActorJob> actorJobComplete;
    uint64_t actorJobSerial_=0;DWORD lastActorCapture=0;bool actorCaptureDecided=false,actorCaptureDue=false;std::string actorSceneMap_;
    unsigned actorVerticesEvaluated=0,actorDraws=0,actorSkippedAlpha=0,snapshotRejects=0;

    LONGLONG terrainCaptureTicks=0,replayCaptureTicks=0;
    LARGE_INTEGER captureFrequency={};
    unsigned terrainCaptureCalls=0,terrainUPCalls=0,replayCaptureCalls=0,unknownCaptureCalls=0;
    std::uint64_t previousCacheHits=0;
    size_t capturedConstantBytes=0,capturedConstantCalls=0;
    void* constantEpochContext=nullptr;
    NorthlightConstantEpoch::Reader constantEpochReader=nullptr;
    NorthlightReplayCaptureConstants::Stats constantEpochStats;
    unsigned capturedSM1Draws=0,capturedRelativeDraws=0;
    unsigned skinnedCandidates=0,skinnedBlendRejected=0,skinnedProjectionRejected=0,skinnedBudgetRejected=0,skinnedSnapshotRejected=0,skinnedAccepted=0;
    size_t captureRejectedBytes=0,acceptedSkinnedBytes=0,acceptedOtherBytes=0;
    bool captureSampled=false;
    enum CapturePhase {CaptureState,CaptureProjection,CaptureSnapshot,CaptureConstants,CaptureMaterial,CaptureFinalize,CaptureActor,CapturePhaseCount};
    NorthlightCapturePhases::Stats<CapturePhaseCount> capturePhases;
    NorthlightCapturePhases::Stats<3> actorPhases;
    NorthlightCapturePhases::Stats<2> replayUploadPhases;
    unsigned capturePhaseSerial=0,capturePhaseDraws=0,capturePhaseAccepted=0,alphaStateQueriesSkipped=0;
    unsigned actorPhaseCandidates=0,actorTextureReads=0,replayUploadCalls=0,replayUploadFallbacks=0;
    void clearCaptureDiagnostics(){
        capturePhases.clear();actorPhases.clear();replayUploadPhases.clear();constantEpochStats={};
        capturePhaseDraws=capturePhaseAccepted=alphaStateQueriesSkipped=0;
        actorPhaseCandidates=actorTextureReads=replayUploadCalls=replayUploadFallbacks=0;
    }
    std::unique_ptr<Replay> acquireReplay(){
        if(freeReplays.empty())return std::make_unique<Replay>();
        auto p=std::move(freeReplays.back());freeReplays.pop_back();pooledSnapshotBytes-=p->snapshot.capacityBytes();return p;
    }
    void recycleReplay(Replay* raw){
        std::unique_ptr<Replay> p(raw);size_t capacity=p->snapshot.capacityBytes();
        bool keep=capacity<=48u*1024u*1024u-pooledSnapshotBytes;p->releaseResources(keep);
        if(keep)pooledSnapshotBytes+=capacity;
        try{freeReplays.push_back(std::move(p));}catch(...){if(keep)pooledSnapshotBytes-=capacity;}
    }
    struct ReplayRecycle {WorldRenderer* owner;void operator()(Replay* replay)const{if(replay)owner->recycleReplay(replay);}};
    // Immutable for a registered shader. Re-registration replaces the complete
    // record before the pointer can be used for a different shader object.
    struct CaptureShader {
        IDirect3DVertexShader9* replacement=nullptr;
        unsigned projectionKind=0;
        NorthlightShaderConstants::Usage usage;
        bool skinned=false,sm1=false;
    };
    std::unordered_map<IDirect3DVertexShader9*,CaptureShader> captureShaders;
    // Game terrain pixel shaders with their own shadow term neutralised
    // (patch_terrain_shadow.h); nullptr records a shader the patch rejected.
    // Bound only for terrain draws while the extension's shadows are active.
    std::unordered_map<IDirect3DPixelShader9*,IDirect3DPixelShader9*> terrainShadowShaders;
    unsigned terrainShadowPatched=0,terrainShadowRejected=0,terrainShadowReports=0;
    // True only after a render() that drew at least one shadow source and
    // ran the lighting composite; terrain draws of the next frame consult it.
    bool shadowsComposited=false;
    // Shadow cascades are centred on the camera's orbit pivot (the player),
    // not the eye: orbiting the camera then leaves every cascade texel, the
    // near/far hand-over and the cached static maps exactly where they were,
    // so foliage shadows and light shafts do not change with the view. The
    // pivot distance is recovered from the intersection of successive centre
    // rays while the camera rotates; walking keeps the last distance.
    V pivotEye,pivotForward;bool pivotValid=false;float pivotDistance=12.f;unsigned pivotUpdates=0;
    V shadowPivot(){
        const V eye=vec(context.camera);
        V forward=V(context.inverseView[8],context.inverseView[9],context.inverseView[10])*projection[2];
        const float length=std::sqrt(NorthlightGI::dot(forward,forward));if(!(length>1e-6f))return eye;forward=forward*(1.f/length);
        if(!pivotValid){pivotEye=eye;pivotForward=forward;pivotValid=true;}
        else{
            const float b=NorthlightGI::dot(pivotForward,forward);
            // Two rays only 2 degrees apart locate their crossing to within
            // gap/sin(2 deg), i.e. tens of units, and the estimate swung
            // between 12 and 26 u while orbiting. Wait for a 12 degree turn
            // (error under 3 u for a .6 u miss), then filter slowly.
            if(b<.978f){
                const V w0=pivotEye-eye;const float d=NorthlightGI::dot(pivotForward,w0),e=NorthlightGI::dot(forward,w0),denom=1-b*b;
                const float sRef=(b*e-d)/denom,t=(e-b*d)/denom;
                const V onRef=pivotEye+pivotForward*sRef,onNow=eye+forward*t,gap=onRef-onNow;
                if(t>1&&t<80&&NorthlightGI::dot(gap,gap)<.36f){
                    const float step=std::max(-2.f,std::min(2.f,(t-pivotDistance)*.25f));
                    pivotDistance+=step;++pivotUpdates;
                }
                pivotEye=eye;pivotForward=forward;
            }else{
                // Walking moves the pivot point itself; an orbit with the right
                // distance does not. Resetting on eye motion alone discarded the
                // reference every 2 u, before a 12 degree turn could accumulate.
                const V refPivot=pivotEye+pivotForward*pivotDistance,nowPivot=eye+forward*pivotDistance,moved=nowPivot-refPivot;
                if(NorthlightGI::dot(moved,moved)>16){pivotEye=eye;pivotForward=forward;}
            }
        }
        return eye+forward*pivotDistance;
    }
    std::unordered_map<IDirect3DVertexShader9*,const WmoShaderSignature*> wmoShaders;
    unsigned cameraChecks=0,wmoContexts=0,wmoRejects=0;
    static V vec(const float* p){return V(p[0],p[1],p[2]);}
    static bool different(V a,V b,float e){return NorthlightGI::dot(a-b,a-b)>e*e;}
    static V quantize(V p,float step){return V(std::floor(p.x/step)*step,std::floor(p.y/step)*step,std::floor(p.z/step)*step);}
#include "world_point_rendering.inl"
    static NorthlightGeometryMemory::Sample geometryMemory() {
        MEMORYSTATUSEX info={};info.dwLength=sizeof info;
        NorthlightGeometryMemory::Sample sample;
        if(!GlobalMemoryStatusEx(&info))return sample;
        sample.available=info.ullAvailVirtual;
        uintptr_t address=0;MEMORY_BASIC_INFORMATION region={};
        while(VirtualQuery(reinterpret_cast<const void*>(address),&region,sizeof region)==sizeof region){
            if(region.State==MEM_FREE)sample.largest=std::max(sample.largest,uint64_t(region.RegionSize));
            const uint64_t next=uint64_t(reinterpret_cast<uintptr_t>(region.BaseAddress))+uint64_t(region.RegionSize);
            if(next<=address||next>std::numeric_limits<uintptr_t>::max())break;
            address=uintptr_t(next);
        }
        sample.valid=sample.largest>0;return sample;
    }
    static void logGeometryMemory(const char* stage,NorthlightGeometryMemory::Sample sample,size_t generations=0){
        logf("GEOMETRY MEMORY stage=%s availableVirtualMiB=%llu largestFreeMiB=%llu generations=%zu valid=%u",
            stage,(unsigned long long)(sample.available>>20),(unsigned long long)(sample.largest>>20),generations,unsigned(sample.valid));
    }
    bool admitStaticAllocation(size_t managedBytes){
        // One address-space walk per frame, not per texture/model allocation.
        // GpuCache already includes the managed host copy in managedBytes.
        if(staticAdmissionFrame!=staticFrame){staticAdmissionFrame=staticFrame;staticMemorySample=geometryMemory();staticReservedBytes=0;}
        NorthlightGeometryMemory::Budget budget;
        budget.available=NorthlightGeometryMemory::ProcessReserve+64*NorthlightGeometryMemory::MiB+managedBytes+staticReservedBytes;
        budget.largest=managedBytes+NorthlightGeometryMemory::ChunkMargin;budget.valid=true;
        if(!NorthlightGeometryMemory::admits(staticMemorySample,budget))return false;
        staticReservedBytes+=managedBytes;return true;
    }
    void requestStaticCasters(const char* map){
        if(!staticStream)return;
        staticFramePivot=shadowPivot();staticPivotReady=true;
        const DWORD now=GetTickCount();
        if(!staticMemoryTick||DWORD(now-staticMemoryTick)>=1000){
            staticMemoryTick=now;
            staticAllowLoads=NorthlightGeometryMemory::admits(geometryMemory(),NorthlightGeometryMemory::buildBudget(64*NorthlightGeometryMemory::MiB));
        }
        const bool lit[2]={NorthlightGI::dot(sourceColors[0],sourceColors[0])>1e-10f,
                           NorthlightGI::dot(sourceColors[1],sourceColors[1])>1e-10f};
        try {staticStream->request(StaticShadow::makeRequest(map,staticFramePivot,sourceDirections,lit,
                celestialValid,celestialValid?celestial.dayFraction:0.,staticAllowLoads));}
        catch(...){if(staticDrawFailures++<4)logf("STATIC SHADOW request deferred: allocation failure; existing shadows retained");}
    }
    void updateStaticCasters(bool allowUploads=true,NorthlightStreaming::Budget* budget=nullptr){
        ++staticFrame;
        if(!staticStream)return;
        const DWORD now=GetTickCount();
        auto next=staticStream->snapshot();
        // Never carry another world's casters while its replacement loads.
        if(staticScene&&staticScene->map!=lastRequest.map){staticCasters.reset();staticMatcher.clear();staticScene.reset();staticOwnerGeneration=UINT64_MAX;}
        if(next&&next->map==lastRequest.map)staticScene=std::move(next);
        if(staticOwnerGeneration!=meshGeneration){
            try {staticCasters.setCoveredPlacements(uploadedStaticOwners);staticOwnerGeneration=meshGeneration;}
            catch(...){staticCasters.setCoveredPlacements({});staticOwnerGeneration=UINT64_MAX;}
        }
        if(staticScene&&(!staticRetryTick||DWORD(now-staticRetryTick)>=1000)){
            try {if(!staticCasters.update(d,staticScene,staticFrame,allowUploads,budget))staticRetryTick=now;else staticRetryTick=0;}
            catch(...){staticRetryTick=now;if(staticDrawFailures++<4)logf("STATIC SHADOW upload deferred: allocation failure; base shadows retained");}
        }
        if(staticScene&&(!staticLogTick||DWORD(now-staticLogTick)>=2000)){
            staticLogTick=now;const auto& c=staticScene->stats;const auto& g=staticCasters.stats();
            logf("STATIC INSTANCE STREAM totalLocks=%llu totalDiscards=%llu totalBytes=%llu",
                 (unsigned long long)g.instanceLocks,(unsigned long long)g.instanceDiscards,(unsigned long long)g.instanceBytes);
            logf("STATIC SHADOW PLANS builds=%llu hits=%llu invalidations=%llu placementTests=%llu batchTests=%llu modelBuilds=%llu modelReuses=%llu modelFallbacks=%llu",
                 (unsigned long long)g.planBuilds,(unsigned long long)g.planHits,(unsigned long long)g.planInvalidations,
                 (unsigned long long)g.placementTests,(unsigned long long)g.batchTests,
                 (unsigned long long)g.modelPlanBuilds,(unsigned long long)g.modelPlanReuses,(unsigned long long)g.modelPlanFallbacks);
            logf("STATIC SHADOW map=%s generation=%llu placements=%u cpuModels=%u readyGPU=%u pendingCPU=%u pendingGPU=%u metadataReads=%llu modelReads=%llu reused=%llu cpuMiB=%.2f cpuPeakMiB=%.2f transientPeakMiB=%.2f gpuMiB=%.2f gpuPeakMiB=%.2f uploadBytes=%llu totalUploadBytes=%llu uploadMs=%.3f readMs=%.3f packMs=%.3f maxJobMs=%.3f latencyMs=%.3f instancing=%u draws=%llu instances=%llu cpuFailures=%llu gpuFailures=%llu dedupSkipped=%u complete=%u dedupAttempts=%u dedupMatches=%u localOwners=%zu metadataMiB=%.2f selectionMs=%.3f",
                 staticScene->map.c_str(),(unsigned long long)staticScene->mapGeneration,c.placements,c.models,g.readyModels,c.pendingModels,g.pendingModels,
                 (unsigned long long)c.metadataReads,(unsigned long long)c.modelReads,(unsigned long long)c.modelReuses,
                 double(c.cpuBytes)/1048576,double(c.cpuPeak)/1048576,double(c.transientPeak)/1048576,double(g.residentBytes)/1048576,double(g.peakBytes)/1048576,
                 (unsigned long long)g.frameBytes,(unsigned long long)g.uploadedBytes,g.uploadMs,c.readMs,c.packMs,c.maxJobMs,c.latencyMs,unsigned(g.instancing),
                 (unsigned long long)g.drawCalls,(unsigned long long)g.instances,(unsigned long long)c.failures,(unsigned long long)g.failures,staticDedupSkipped,unsigned(c.complete),staticDedupAttempts,staticDedupMatches,uploadedStaticOwners.size(),double(c.metadataBytes)/1048576,c.selectionMs);
            staticDedupSkipped=staticDedupAttempts=staticDedupMatches=0;
        }
    }
    void work() {
      // Below-normal priority: the solve competes with the single game thread
      // for the same performance cores under Rosetta; it is latency-tolerant.
      SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
      try {
        std::shared_ptr<NorthlightGI::BVH> bvh;std::shared_ptr<NorthlightWorldMesh::WorldMeshUploadPlan> scenePlan;V sceneCenter;std::shared_ptr<const NorthlightRegionalFog::Field> sceneFog;std::string sceneMap;uint64_t serial=0;
        NorthlightGeometryMemory::Generations<NorthlightGI::BVH> generations;
        NorthlightGI::LocalSceneCache localGeometry;
        std::shared_ptr<Snapshot> previousLighting;
        DWORD memoryReportAt=0;
        NorthlightGI::ProbeCache probeCache;
        NorthlightGI::DynamicProbeLayer dynamicProbes;
        NorthlightLocalLights::Cache lightCache(root+"world-cache/lights");
        std::vector<NorthlightGI::PointLight> sceneLights;
        std::vector<NorthlightLocalLights::Light> rawSceneLights;
        std::shared_ptr<NorthlightGI::BVH> actors;
        NorthlightWorkerActorMemo<NorthlightActorGeometry::ActorJob> actorMemo;
        uint64_t solvedActorHash=0,completedActorHash=0,completedBaseId=0;
        std::shared_ptr<NorthlightGI::BVH> probeCacheGeometry;
        NorthlightGI::Lighting probeCacheLight;NorthlightGI::PreparedLighting preparedLight;
        NorthlightGI::ProbePublicationCadence publicationCadence;
        std::vector<NorthlightGI::ProbeAtlasEntry> displayFallback;
        uint64_t lightingGeneration=0;unsigned superseded=0,retargeted=0;
        for(;;){
            Request r;{std::unique_lock<std::mutex> lock(mutex);wake.wait(lock,[&]{return stopping||pending;});if(stopping)return;r=request;pending=false;workerBusy=true;}
            struct WorkerIdle {std::atomic<bool>& busy;~WorkerIdle(){busy=false;}} idle{workerBusy};
            // Publish small authored zone data before expensive geometry/GI work.
            // No cache reads are performed by the sky or world draw callbacks.
            int paletteTX=int(std::floor((NorthlightRegionalFog::WorldZero-r.camera.y)/NorthlightRegionalFog::TileSize));
            int paletteTY=int(std::floor((NorthlightRegionalFog::WorldZero-r.camera.x)/NorthlightRegionalFog::TileSize));
            std::shared_ptr<const PaletteRegion> paletteRegion;
            {std::lock_guard<std::mutex> lock(mutex);paletteRegion=publishedPaletteRegion;}
            if(!paletteRegion||paletteRegion->map!=r.map||paletteRegion->tx!=paletteTX||paletteRegion->ty!=paletteTY){
                auto next=std::make_shared<PaletteRegion>();next->map=r.map;next->tx=paletteTX;next->ty=paletteTY;
                next->region=NorthlightRegionalFog::loadRegion(root+"world-cache/fog",r.map,r.camera.x,r.camera.y);
                paletteRegion=next;
                {std::lock_guard<std::mutex> lock(mutex);publishedPaletteRegion=next;}
            }
            auto result=std::make_shared<Snapshot>();result->map=r.map;result->center=r.camera;
            DWORD started=GetTickCount();result->requestId=r.id;result->requestedAt=r.queuedAt;result->queueMs=started-r.queuedAt;
            auto publishError=[&](){std::lock_guard<std::mutex> lock(mutex);if(stopping||request.id!=r.id){++superseded;return false;}published=result;return true;};
            auto currentGeometry=[&](){std::lock_guard<std::mutex> lock(mutex);return !stopping&&request.map==r.map&&!different(request.camera,r.camera,64);};
            auto deferBuild=[&](const char* stage,NorthlightGeometryMemory::Sample sample,unsigned delay){
                DWORD now=GetTickCount();if(!memoryReportAt||now-memoryReportAt>=1000){memoryReportAt=now;logGeometryMemory(stage,sample,generations.live());}
                // The consumed request must survive a stationary camera. Never
                // replace a newer request with r; next loop reads latest state.
                std::unique_lock<std::mutex> lock(mutex);pending=true;
                wake.wait_for(lock,std::chrono::milliseconds(delay),[&]{return stopping||request.id!=r.id;});
            };
            if(!bvh||sceneMap!=r.map||different(sceneCenter,r.camera,64)) {
                // Discard worker-only old geometry before building its replacement.
                // Preserve just the small lighting snapshot for geometry-first publication.
                {std::lock_guard<std::mutex> lock(mutex);
                    if(published&&published->bvh==bvh&&bvh){
                        previousLighting=std::make_shared<Snapshot>(*published);
                        previousLighting->bvh.reset();previousLighting->meshPlan.reset();published.reset();
                    }}
                probeCache.clear();probeCacheGeometry.reset();
                dynamicProbes.reset(actors?&actors->scene():nullptr);
                scenePlan.reset();sceneFog.reset();bvh.reset();
                auto memory=geometryMemory();
                if(!generations.canAdmit()){deferBuild("generation-deferred",memory,100);continue;}
                if(!NorthlightGeometryMemory::admits(memory,NorthlightGeometryMemory::buildBudget(64*NorthlightGeometryMemory::MiB))){localGeometry.reset();memory=geometryMemory();}
                if(!NorthlightGeometryMemory::admits(memory,NorthlightGeometryMemory::buildBudget(64*NorthlightGeometryMemory::MiB))){deferBuild("build-deferred",memory,1000);continue;}
                logGeometryMemory("before-load",memory,generations.live());
                std::string error;
                int tx=int(std::floor((17066.6666667-r.camera.y)/533.3333333));
                int ty=int(std::floor((17066.6666667-r.camera.x)/533.3333333));
                std::vector<std::string> tiles;
                for(int y=ty-1;y<=ty+1;++y)for(int x=tx-1;x<=tx+1;++x){
                    char name[512];std::snprintf(name,sizeof name,"%sworld-cache/%s/%d_%d.fg3",root.c_str(),r.map.c_str(),x,y);
                    FILE* f=std::fopen(name,"rb");if(f){std::fclose(f);tiles.emplace_back(name);}
                }
                bool localDeferred=false;
                NorthlightGI::AllocationAdmission localAdmission=[&](uint64_t bytes){
                    if(bytes<NorthlightGeometryMemory::MiB)return true;
                    auto sample=geometryMemory();
                    if(NorthlightGeometryMemory::admits(sample,NorthlightGeometryMemory::buildBudget(bytes)))return true;
                    localDeferred=true;logGeometryMemory("local-geometry-allocation-deferred",sample,generations.live());return false;
                };
                auto replacement=std::make_shared<NorthlightGI::BVH>();
                if(!generations.track(replacement))throw std::runtime_error("Geometry generation admission invariant");
                if(tiles.empty()||!localGeometry.build(tiles,root+"world-cache/models",r.map,r.camera-V(288,288,320),r.camera+V(288,288,320),*replacement,error,localAdmission)||!replacement->triangleCount()){
                    replacement.reset();
                    if(localDeferred){localGeometry.reset();deferBuild("local-geometry-retry",geometryMemory(),1000);continue;}
                    result->message=error.empty()?"No cached geometry for "+r.map:error;if(publishError())bvh.reset();continue;
                }
                const auto& localStats=localGeometry.stats();
                logf("WORLD local incremental: reads=%llu built=%llu reused=%llu reusedTriangles=%llu builtTriangles=%llu retainedMiB=%.2f flatMiB=%.2f bvhMiB=%.2f loadMs=%.3f pieceMs=%.3f assembleMs=%.3f bvhMs=%.3f totalMs=%.3f",
                    (unsigned long long)localStats.modelReads,(unsigned long long)localStats.pieceBuilds,(unsigned long long)localStats.pieceReuses,
                    (unsigned long long)localStats.reusedTriangles,(unsigned long long)localStats.builtTriangles,double(localStats.retainedBytes)/1048576,
                    double(localStats.flatBytes)/1048576,double(localStats.bvhBytes)/1048576,localStats.loadMs,localStats.pieceMs,localStats.assembleMs,localStats.bvhMs,localStats.totalMs);
                if(!currentGeometry()){++superseded;continue;}
                memory=geometryMemory();logGeometryMemory("after-bvh",memory,generations.live());
                if(!NorthlightGeometryMemory::admits(memory,NorthlightGeometryMemory::buildBudget())){
                    replacement.reset();deferBuild("plan-deferred",memory,1000);continue;
                }
                auto plan=std::make_shared<NorthlightWorldMesh::WorldMeshUploadPlan>();
                // GI stays local. Load only authored terrain over the complete
                // directional caster volume into a separate GPU shadow source.
                bool terrainDeferred=false;uint64_t terrainLargest=0;unsigned terrainChecks=0;
                NorthlightGI::AllocationAdmission terrainAdmission=[&](uint64_t bytes){
                    terrainLargest=std::max(terrainLargest,bytes);
                    // Small metadata cannot fragment a large block. Keep the
                    // process reserve at stage entry and check every large growth.
                    if(bytes<NorthlightGeometryMemory::MiB)return true;
                    ++terrainChecks;
                    auto sample=geometryMemory();
                    if(NorthlightGeometryMemory::admits(sample,NorthlightGeometryMemory::buildBudget(bytes)))return true;
                    terrainDeferred=true;logGeometryMemory("shadow-terrain-allocation-deferred",sample,generations.live());
                    logf("WORLD terrain allocation requestMiB=%.2f requiredContiguousMiB=%.2f",double(bytes)/1048576,double(bytes+NorthlightGeometryMemory::ContiguousMargin)/1048576);
                    return false;
                };
                const float reach=shadowRanges.at(r.map,NorthlightRegionalFog::zoneAt(paletteRegion->region,r.camera.x,r.camera.y));
                const bool extended=reach>NorthlightShadowTerrain::Radius;
                const int tileReach=extended?int(std::ceil(reach/NorthlightRegionalFog::TileSize)):2;
                std::function<bool(V,V)> terrainFilter,terrainChunkFilter;
                if(extended)terrainFilter=[center=r.camera](V lo,V hi){return NorthlightRegionalShadow::selected(center,lo,hi);};
                if(extended)terrainChunkFilter=[center=r.camera](V lo,V hi){return NorthlightRegionalShadow::selectedChunk(center,lo,hi);};
                std::vector<std::string> shadowTiles;
                for(int y=ty-tileReach;y<=ty+tileReach;++y)for(int x=tx-tileReach;x<=tx+tileReach;++x){
                    const double zero=NorthlightRegionalFog::WorldZero,size=NorthlightRegionalFog::TileSize;
                    if(extended&&!terrainFilter(V(float(zero-(y+1)*size),float(zero-(x+1)*size),-100000),V(float(zero-y*size),float(zero-x*size),100000)))continue;
                    char name[512];std::snprintf(name,sizeof name,"%sworld-cache/%s/%d_%d.fg3",root.c_str(),r.map.c_str(),x,y);
                    FILE* f=std::fopen(name,"rb");if(f){std::fclose(f);shadowTiles.emplace_back(name);}
                }
                NorthlightGI::WorldScene shadowTerrain;
                if(!NorthlightGI::loadInstancedScenes(shadowTiles,root+"world-cache/models",r.camera-V(reach,reach,reach),r.camera+V(reach,reach,reach),shadowTerrain,error,1,terrainAdmission,terrainFilter,terrainChunkFilter)||
                   !NorthlightShadowTerrain::build(replacement->scene(),shadowTerrain,r.camera,*plan,error,terrainAdmission,reach)){
                    if(terrainDeferred){shadowTerrain=NorthlightGI::WorldScene{};plan.reset();replacement.reset();deferBuild("shadow-terrain-retry",geometryMemory(),1000);continue;}
                    result->message=error;publishError();continue;
                }
                logf("WORLD terrain allocation largestMiB=%.2f checks=%u",double(terrainLargest)/1048576,terrainChecks);
                logf("WORLD shadow terrain: localTriangles=%zu shadowTriangles=%u fixedChunks=%zu reach=%.0f tiles=%zu",replacement->triangleCount(),plan->triangleCount,plan->fixedTerrainChunks.size(),reach,shadowTiles.size());
                if(!currentGeometry()){++superseded;continue;}
                shadowTerrain=NorthlightGI::WorldScene{}; // merged shadow plan already owns its data
                auto pages=std::make_shared<NorthlightWorldMeshPages::Plan>();
                if(!NorthlightWorldMeshPages::build(*plan,*pages,error,terrainAdmission)){
                    if(terrainDeferred){pages.reset();plan.reset();replacement.reset();deferBuild("mesh-page-retry",geometryMemory(),1000);continue;}
                    result->message=error;publishError();continue;
                }
                if(!NorthlightWorldMeshPages::seal(*plan,pages,error,terrainAdmission)){
                    if(terrainDeferred){pages.reset();plan.reset();replacement.reset();deferBuild("mesh-page-seal-retry",geometryMemory(),1000);continue;}
                    result->message=error;publishError();continue;
                }
                logf("WORLD mesh pages=%zu batches=%zu originalBatches=%zu vertexMiB=%.2f indexMiB=%.2f maxResourceKiB=512",pages->pages.size(),pages->batches.size(),plan->batches.size(),double(pages->vertexBytes)/1048576,double(pages->indexBytes)/1048576);
                logGeometryMemory("after-plan",geometryMemory(),generations.live());
                scenePlan=plan;bvh=replacement;sceneMap=r.map;sceneCenter=r.camera;
                float eye[]={r.camera.x,r.camera.y,r.camera.z};
                sceneLights.clear();
                rawSceneLights.clear();
                if(lightCache.loadLights(r.map,eye,520,rawSceneLights))for(auto& light:rawSceneLights)
                    sceneLights.push_back({vec(light.position),vec(light.diffuse)*3.14159265f,light.attenuationStart,light.attenuationEnd});
                logf("GI local lights map=%s count=%zu (indirect only; authored direct light retained)",r.map.c_str(),sceneLights.size());
                const auto& region=paletteRegion->region;
                sceneFog=std::make_shared<NorthlightRegionalFog::Field>(NorthlightRegionalFog::buildField(bvh->scene(),region,r.camera.x,r.camera.y));
                logf("REGIONAL FOG map=%s tiles=%zu missing=%u groundCells=%u fogCells=%u airCells=%u indoorCells=%u citySurfaceCells=%u",r.map.c_str(),region.tiles.size(),region.missing,sceneFog->groundCells,sceneFog->fogCells,sceneFog->airCells,sceneFog->indoorCells,sceneFog->citySurfaceCells);
            }
            result->geometryMs=GetTickCount()-started;
            {std::lock_guard<std::mutex> lock(mutex);if(stopping)return;if(request.id!=r.id){++superseded;continue;}}
            DWORD actorStarted=GetTickCount();
            bool actorsChanged=false;
            if(!actorMemo.matches(r.actorJob)){
              NorthlightActorGeometry::Result resolvedActors;
              if(r.actorJob)resolvedActors=r.actorJob->resolve();
              actorsChanged=resolvedActors.hash!=solvedActorHash;
              if(actorsChanged){
                actors.reset();std::string actorError;
                if(resolvedActors.scene&&!resolvedActors.scene->triangles.empty()){
                    auto next=std::make_shared<NorthlightGI::BVH>();auto scene=*resolvedActors.scene;
                    if(next->build(std::move(scene),actorError))actors=next;
                    else logf("GI actor BVH rejected: %s",actorError.c_str());
                }
                solvedActorHash=resolvedActors.hash;
                // Pose/packet change only: keep corrections whose nearby actor
                // bounds are unchanged (0.3.35); the static generation is intact.
                dynamicProbes.observe(actors?&actors->scene():nullptr);
              }
              actorMemo.remember(r.actorJob);
            }
            result->actorMs=GetTickCount()-actorStarted;
            // An unchanged actor packet must not republish/upload an identical
            // 4096-entry probe atlas every capture interval. Only skip after a
            // complete solve for this actor generation has actually published.
            if(r.reason==64&&!actorsChanged&&completedActorHash==solvedActorHash&&completedBaseId==r.baseId&&r.baseId!=0&&probeCacheGeometry==bvh)continue;
            r.light.points=sceneLights;r.light.movingGeometry=nullptr;
            // A probe belongs to a fixed world point and a geometry/lighting
            // generation. Camera motion alone must not recompute that point.
            if(probeCacheGeometry!=bvh||!NorthlightGI::sameStaticLighting(probeCacheLight,r.light)){
                probeCache.clear();probeCacheGeometry=bvh;probeCacheLight=r.light;
                preparedLight=NorthlightGI::prepareLighting(probeCacheLight);
                publicationCadence.reset();++lightingGeneration;
                // Keep already displayed same-map GI until replacement probes
                // are ready. This is display fallback only, never solver input.
                displayFallback.clear();
                {std::lock_guard<std::mutex> lock(mutex);
                 if(published&&published->map==r.map)displayFallback=published->atlas;
                 else if(previousLighting&&previousLighting->map==r.map)displayFallback=previousLighting->atlas;}
                dynamicProbes.reset(actors?&actors->scene():nullptr);
            }
            // Publish new geometry immediately. Shadow rendering must not wait
            // for the probe solve; retain prior GI until its replacement arrives.
            {std::lock_guard<std::mutex> lock(mutex);
             if(stopping)return;if(request.id!=r.id){++superseded;continue;}
             if(!published||published->bvh!=bvh){
                auto geometry=std::make_shared<Snapshot>();
                if(published&&published->map==r.map)*geometry=*published;
                else if(previousLighting&&previousLighting->map==r.map)*geometry=*previousLighting;
                geometry->bvh=bvh;geometry->meshPlan=scenePlan;geometry->retirementBytes=bvh->retainedBytes()+scenePlan->cpuBytes()+sizeof(Snapshot);geometry->map=r.map;geometry->center=r.camera;geometry->fogField=sceneFog;geometry->localLights=rawSceneLights;
                published=geometry;previousLighting.reset();
             }}
            result->fogField=sceneFog;result->bvh=bvh;result->meshPlan=scenePlan;result->retirementBytes=bvh->retainedBytes()+scenePlan->cpuBytes()+sizeof(Snapshot);result->localLights=rawSceneLights;result->origin=NorthlightGI::probeWindowOrigin(r.camera);
            result->lightingGeneration=lightingGeneration;
            DWORD solveStart=GetTickCount();bool obsolete=false;
            // Called with mutex held. A camera-only request may consume this
            // generation's completed world probes, but a new map, distant BVH
            // region or changed lighting must never accept this publication.
            uint64_t checkedRequestId=r.id;bool checkedCompatibility=true;
            auto compatiblePublication=[&](){
                if(probeCacheGeometry!=bvh)return false;
                if(checkedRequestId==request.id)return checkedCompatibility;
                checkedRequestId=request.id;
                auto latestLight=request.light;latestLight.points=sceneLights;latestLight.movingGeometry=nullptr;
                checkedCompatibility=NorthlightGI::staticFallbackCompatible(r.map,sceneMap,request.map,
                    r.camera,sceneCenter,request.camera,probeCacheLight,latestLight);
                return checkedCompatibility;
            };
            auto publishProgress=[&](){
                if(!publicationCadence.due(GetTickCount(),result->processedProbes))return;
                {std::lock_guard<std::mutex> lock(mutex);if(stopping||!compatiblePublication())return;}
                // Every export owns its storage. Neither subsequent solves nor
                // cancellation can modify an atlas already seen by rendering.
                auto progress=std::make_shared<Snapshot>(*result);
                progress->atlas=probeCache.atlas();progress->dynamicReused=progress->dynamicSolved=0;
                if(!dynamicProbes.applyCached(progress->atlas,
                    [&](){std::lock_guard<std::mutex> lock(mutex);return stopping||!compatiblePublication();},
                    progress->dynamicReused))return;
                NorthlightGI::retainProbeDisplayFallback(progress->atlas,displayFallback);
                progress->solveMs=GetTickCount()-solveStart;progress->superseded=superseded;
                progress->retargeted=retargeted;progress->partial=true;progress->staticOnly=progress->dynamicReused==0;
                std::lock_guard<std::mutex> lock(mutex);if(stopping||!compatiblePublication())return;
                progress->serial=++serial;published=std::move(progress);publicationCadence.published(GetTickCount());
            };
            auto noteSuperseded=[&](){
                std::lock_guard<std::mutex> lock(mutex);
                if(!stopping&&request.reason==2&&compatiblePublication())++retargeted;else ++superseded;
            };
            const auto order=NorthlightGI::probeSolveOrder(r.camera);
            for(unsigned cursor=0;cursor<NorthlightGI::ProbeGridCount;++cursor){
                if(cursor%8==0){
                    {std::lock_guard<std::mutex> lock(mutex);if(stopping)return;obsolete=request.id!=r.id;}
                    if(obsolete)break;
                    publishProgress();
                }
                V p=NorthlightGI::probeWindowPosition(result->origin,order[cursor]);
                NorthlightGI::ProbeGridKey key;
                if(!NorthlightGI::probeGridKey(p,key))throw std::runtime_error("Invalid world probe coordinate");
                NorthlightGI::Probe probe;
                if(probeCache.get(key,probe))++result->reusedProbes;
                else{probe=NorthlightGI::solveProbePrepared(*bvh,p,preparedLight,64,NorthlightGI::probeSeed(key));probeCache.put(key,probe);++result->solvedProbes;publicationCadence.solved();}
                ++result->processedProbes;if(probe.valid)++result->validProbes;
            }
            if(obsolete){
                noteSuperseded();publishProgress();
                // The pending request selects a new nearest-first window and
                // current actor packet. Its static cache and publication clock
                // survive camera-only retargeting; no completed probe is lost.
                continue;
            }
            result->atlas=probeCache.atlas();
            // Preserve all static world slots across pose/culling changes. The
            // observed actor scene only supplies a local radiance correction.
            if(!dynamicProbes.apply(result->atlas,
                [&](NorthlightGI::ProbeGridKey key,V p){return NorthlightGI::solveProbePrepared(*bvh,p,preparedLight,64,NorthlightGI::probeSeed(key),actors.get());},
                [&](){std::lock_guard<std::mutex> lock(mutex);return stopping||request.id!=r.id;},
                result->dynamicReused,result->dynamicSolved)){
                noteSuperseded();result->atlas.clear();publishProgress();continue;
            }
            result->solveMs=GetTickCount()-solveStart;result->superseded=superseded;result->retargeted=retargeted;
            bool publishObsolete=false;
            {std::lock_guard<std::mutex> lock(mutex);if(stopping)return;
             if(request.id!=r.id)publishObsolete=true;
             else{result->serial=++serial;published=result;publicationCadence.published(GetTickCount());displayFallback.clear();completedActorHash=solvedActorHash;completedBaseId=r.baseId;}}
            if(publishObsolete){noteSuperseded();result->atlas.clear();publishProgress();}
        }
      }catch(const std::bad_alloc&){workerFaultCode.store(1);}
       catch(const std::exception&){workerFaultCode.store(2);}
       catch(...){workerFaultCode.store(3);}
      // The thread has stopped. Do not allocate a diagnostic snapshot in an
      // allocation-failure handler. The render thread reports workerFault().
      workerBusy=false;
    }
    bool check(HRESULT h,const char* s){if(SUCCEEDED(h))return true;if(!failed)logf("WORLD DISABLED: %s HRESULT=%08lx",s,(unsigned long)h);failed=true;return false;}
    void clearMesh(){vertices=nullptr;indices=nullptr;for(auto& m:materials)drop(m);materials.clear();uploadedTextureBytes=0;batches.clear();uploadedAlphaCutoffs.clear();uploadedLocalShadowRecords.reset();uploaded.reset();}
    bool target(UINT w,UINT h,D3DFORMAT fmt,IDirect3DTexture9** t,IDirect3DSurface9** s){return check(d->CreateTexture(w,h,1,D3DUSAGE_RENDERTARGET,fmt,D3DPOOL_DEFAULT,t,nullptr),"world render texture")&&check((*t)->GetSurfaceLevel(0,s),"world render surface");}
    bool resources(UINT w,UINT h,D3DFORMAT fmt){
        if(width==w&&height==h&&color)return true;
        releaseGPU();width=w;height=h;
        if(!check(d->CreatePixelShader(kWorldGIShader,&giPS),"world GI shader")||!check(d->CreatePixelShader(kWorldLightingShader,&lightingPS),"world lighting shader")||!check(d->CreatePixelShader(kWorldFogShader,&fogPS),"volume shader")||!check(d->CreatePixelShader(kFogBlurShader,&fogBlurPS),"volume blur shader")||!check(d->CreatePixelShader(kLocalDirectShader,&localDirectPS),"local direct light shader")||!check(d->CreatePixelShader(kTemporalLightShader,&temporalPS),"temporal light shader")||!check(d->CreatePixelShader(kLocalFogShader,&localFogPS),"local fog glow shader")||!check(d->CreatePixelShader(kWorldNormalsShader,&normalsPS),"world normals shader")||!check(d->CreatePixelShader(kSourceVisibilityPSShader,&sourceVisPS),"source visibility shader")||!check(d->CreatePixelShader(kWorldCompositeShader,&finalPS),"world composite shader")||!check(d->CreatePixelShader(kShadowPSShader,&shadowPS),"shadow shader")||!check(d->CreatePixelShader(kShadowReplayPSShader,&replayPS),"replay shader")||!check(d->CreateVertexShader(kShadowVSShader,&shadowVS),"shadow vertex shader")||!check(d->CreateVertexShader(kShadowCacheVSShader,&cachedShadowVS),"cached shadow vertex shader")||!check(d->CreatePixelShader(kStaticCasterPSShader,&cachedShadowPS),"cached shadow depth shader"))return false;
        // Optional equivalent variants; unsupported creation retains .95 depth semantics.
        if(FAILED(d->CreatePixelShader(kStaticCasterFastPSShader,&cachedFastPS)))drop(cachedFastPS);
        if(FAILED(d->CreatePixelShader(kStaticCasterOpaqueFastPSShader,&cachedOpaqueFastPS)))drop(cachedOpaqueFastPS);
        if(FAILED(d->CreatePixelShader(kStaticCasterOpaquePSShader,&cachedOpaquePS)))drop(cachedOpaquePS);
        for(int i=0;i<4;++i)if(!target(1024,1024,D3DFMT_R32F,&shadow[i],&shadowSurface[i]))return false;
        if(!check(d->CreateDepthStencilSurface(1024,1024,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,TRUE,&shadowDepth,nullptr),"shadow depth"))return false;
        for(int i=0;i<4;++i)if(!target(ShadowCacheSize,ShadowCacheSize,D3DFMT_R32F,&shadowCache[i],&shadowCacheSurface[i]))return false;
        if(!check(d->CreateDepthStencilSurface(ShadowCacheSize,ShadowCacheSize,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,TRUE,&shadowCacheDepth,nullptr),"shadow cache depth"))return false;
        if(!target(1024,1024,D3DFMT_R32F,&shadowScratch,&shadowScratchSurface))return false;
        if(!check(d->CreatePixelShader(kShadowUnionShader,&unionPS),"shadow union shader"))return false;
        invalidateShadowCache();
        for(int i=0;i<5;++i)if(!check(d->CreateTexture(NorthlightGI::ProbeAtlasN*NorthlightGI::ProbeAtlasN,i==3?NorthlightGI::ProbeAtlasN*6:NorthlightGI::ProbeAtlasN,1,0,D3DFMT_A32B32G32R32F,D3DPOOL_MANAGED,&probe[i],nullptr),"GI probe texture"))return false;
        if(!target(w/2,h/2,D3DFMT_A16B16G16R16F,&baselineLight,&baselineSurface)||!target(w/2,h/2,D3DFMT_A16B16G16R16F,&light,&lightSurface)||!target(w/2,h/2,D3DFMT_A16B16G16R16F,&fog,&fogSurface)||!target(w/2,h/2,D3DFMT_A16B16G16R16F,&fogBlurred,&fogBlurredSurface)||!target(w/2,h/2,D3DFMT_A16B16G16R16F,&normalBuffer,&normalSurface)||!target(1,1,D3DFMT_A16B16G16R16F,&sourceVis[0][0],&sourceVisSurface[0][0])||!target(1,1,D3DFMT_A16B16G16R16F,&sourceVis[0][1],&sourceVisSurface[0][1])||!target(1,1,D3DFMT_A16B16G16R16F,&sourceVis[1][0],&sourceVisSurface[1][0])||!target(1,1,D3DFMT_A16B16G16R16F,&sourceVis[1][1],&sourceVisSurface[1][1])||!target(w/2,h/2,D3DFMT_A16B16G16R16F,&temporalLight[0],&temporalLightSurface[0])||!target(w/2,h/2,D3DFMT_A16B16G16R16F,&temporalLight[1],&temporalLightSurface[1])||!target(w/2,h/2,D3DFMT_R32F,&temporalDepth[0],&temporalDepthSurface[0])||!target(w/2,h/2,D3DFMT_R32F,&temporalDepth[1],&temporalDepthSurface[1])||!target(w,h,fmt,&color,&colorSurface))return false;
        const D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},{0,12,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_NORMAL,0},{0,24,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
        return check(d->CreateVertexDeclaration(elements,&shadowDecl),"shadow declaration");
    }
    void retirePendingCpu(){
        if(!pendingMesh)return;
        // No D3D owner leaves this thread. The packet may be the last CPU
        // geometry owner after a commit/cancel, so release it on the reaper.
        auto& pending=*pendingMesh;
        if(pending.plan&&!pending.textures.empty())retiredMaterials.enqueue(pending.textures,size_t(pending.plan->textureBytes));
        if(pending.plan&&NorthlightStreaming::cpuRetirement().retire(pending.plan,pending.plan->cpuBytes()))pending.plan.reset();
        if(pending.bvh&&NorthlightStreaming::cpuRetirement().retire(pending.bvh,pending.bvh->retainedBytes()))pending.bvh.reset();
        // Queue pressure uses synchronous reclamation, preserving the memory
        // reserve; it never retains an unbounded chain of old generations.
    }
    bool streamingFailure(HRESULT hr,const char* stage,const char* detail=""){
        const auto* plan=pendingMesh?pendingMesh->plan.get():(active?active->meshPlan.get():nullptr);
        if(streamingReports++<12)logf("WORLD streaming retry: stage=%s hr=%08lx detail=%s vertexBytes=%llu indexBytes=%llu vertices=%u triangles=%u sourceMatch=%d oldMesh=%d retryMs=1000",stage,(unsigned long)hr,detail,
            (unsigned long long)(plan?plan->vertexBytes:0),(unsigned long long)(plan?plan->indexBytes:0),plan?plan->vertexCount:0,plan?plan->triangleCount:0,
            plan&&plan->pagesSealed,vertices&&indices&&active&&uploadedMap==active->map);
        retirePendingCpu();pendingMesh.reset();meshRetry.fail(GetTickCount());
        return vertices&&indices&&active&&uploadedMap==active->map;
    }
    bool uploadRegionalFog(){
        if(uploadedFogField==active->fogField)return true;
        if(!active->fogField){uploadedFogField.reset();return true;}
        constexpr unsigned n=NorthlightRegionalFog::N;
        if(!regionalFogTexture&&!check(d->CreateTexture(n,n,1,0,D3DFMT_A32B32G32R32F,D3DPOOL_MANAGED,&regionalFogTexture,nullptr),"regional fog field"))return false;
        D3DLOCKED_RECT lock={};if(!check(regionalFogTexture->LockRect(0,&lock,nullptr,0),"regional fog lock"))return false;
        if(!lock.pBits||lock.Pitch<INT(n*sizeof(NorthlightRegionalFog::Texel))){regionalFogTexture->UnlockRect(0);return check(E_FAIL,"regional fog layout");}
        for(unsigned y=0;y<n;++y)memcpy(static_cast<char*>(lock.pBits)+y*lock.Pitch,active->fogField->texels.data()+y*n,n*sizeof(NorthlightRegionalFog::Texel));
        if(!check(regionalFogTexture->UnlockRect(0),"regional fog unlock"))return false;
        uploadedFogField=active->fogField;return true;
    }
    bool upload(NorthlightStreaming::Budget& streamBudget){
        retiredMaterials.drain(streamBudget);
        if(!uploadRegionalFog())return false;
        static_assert(sizeof(NorthlightGI::WorldVertex)==32,"FGS vertex layout");
        if(pendingMesh&&(pendingMesh->map!=active->map||pendingMesh->bvh!=active->bvh)){retirePendingCpu();pendingMesh.reset();meshRetry.clear();}
        if(uploaded.lock()!=active->bvh&&!pendingMesh&&streamBudget.available()){
            if(!meshRetry.ready(GetTickCount()))return uploadedMap==active->map&&vertices&&indices;
            if(!active->meshPlan)return streamingFailure(D3DERR_INVALIDCALL,"validate plan","missing upload plan");
            uint64_t largestTexture=0;for(const auto& material:active->meshPlan->materials)largestTexture=std::max(largestTexture,uint64_t(material.bgra.size()));
            const auto& plan=*active->meshPlan;NorthlightGeometryMemory::Sample memory;
            {auto phase=streamingPhases.measure(NorthlightStreaming::PhaseProfile::Admission);memory=geometryMemory();}
            const char* invalid=NorthlightWorldMeshPages::validateSealed(plan);
            if(invalid||!plan.pages||plan.pages->pages.empty())return streamingFailure(D3DERR_INVALIDCALL,"validate pages",invalid?invalid:"missing pages");
            auto budget=NorthlightGeometryMemory::uploadBudget(plan.pages->vertexBytes,plan.pages->indexBytes,plan.textureBytes,largestTexture);
            budget.largest=2*std::max<uint64_t>(NorthlightWorldMeshPages::PageVertexByteLimit,largestTexture)+NorthlightGeometryMemory::ChunkMargin;
            logGeometryMemory("before-upload",memory);
            if(!NorthlightGeometryMemory::admits(memory,budget))return streamingFailure(E_OUTOFMEMORY,"upload-admission","address-space reserve or contiguous block");
            pendingMesh=std::make_unique<PendingMesh>();pendingMesh->bvh=active->bvh;
            pendingMesh->plan=active->meshPlan;pendingMesh->map=active->map;
            auto& next=*pendingMesh;next.textures.reserve(next.plan->materials.size());
            auto& pool=meshPool[stagingSlot()];if(pool.size()<plan.pages->pages.size())pool.resize(plan.pages->pages.size());
            std::vector<NorthlightWorldMeshPages::PageBytes> sizes;sizes.reserve(plan.pages->pages.size());
            for(const auto& page:plan.pages->pages)sizes.push_back({page.vertices.size()*sizeof(NorthlightGI::WorldVertex),page.indices.size()*sizeof(uint32_t)});
            if(!next.upload.begin(std::move(sizes)))return streamingFailure(D3DERR_INVALIDCALL,"page sizes");
        }
        if(pendingMesh){
            auto& next=*pendingMesh;auto& plan=*next.plan;
            ++next.frames;
            // Each bounded driver operation yields back to the shared deadline.
            struct PageBudget {
                NorthlightStreaming::Budget& parent;
                bool available()const{return parent.available()&&parent.elapsedMs()<.5;}
                size_t chunk(size_t n)const{return available()?parent.chunk(n):0;}
                void consume(size_t n){parent.consume(n);}
            } pageBudget{streamBudget};
            NorthlightGeometryMemory::Sample pageMemory;bool sampled=false;
            auto perform=[&](NorthlightWorldMeshPages::Step step,size_t pageIndex,size_t offset,size_t bytes){
                using S=NorthlightWorldMeshPages::Step;
                auto& page=meshPool[stagingSlot()][pageIndex];const auto& cpu=plan.pages->pages[pageIndex];
                if(step==S::CreateVertices||step==S::CreateIndices){
                    const bool vb=step==S::CreateVertices;
                    const bool reuse=vb?page.vb&&page.vbCapacity>=bytes:page.ib&&page.ibCapacity>=bytes;
                    if(reuse)return true;
                    if(!sampled){
                        auto phase=streamingPhases.measure(NorthlightStreaming::PhaseProfile::Admission);
                        // Full address-space admission already ran at generation
                        // start. Bounded <=512 KiB growth needs a fresh aggregate
                        // reserve check, not another VirtualQuery walk per page.
                        MEMORYSTATUSEX status={};status.dwLength=sizeof status;
                        pageMemory.valid=GlobalMemoryStatusEx(&status)!=FALSE;
                        pageMemory.available=status.ullAvailVirtual;sampled=true;
                    }
                    auto phase=streamingPhases.measure(NorthlightStreaming::PhaseProfile::Buffers);
                    return acquireMeshPage(pageIndex,vb,UINT(bytes),pageMemory);
                }
                if(step==S::PreloadVertices||step==S::PreloadIndices){
                    auto phase=streamingPhases.measure(NorthlightStreaming::PhaseProfile::Preload);
                    if(step==S::PreloadVertices)page.vb->PreLoad();else page.ib->PreLoad();return true;
                }
                auto phase=streamingPhases.measure(NorthlightStreaming::PhaseProfile::Copies);
                const bool vb=step==S::CopyVertices;void* data=nullptr;
                HRESULT hr=vb?page.vb->Lock(UINT(offset),UINT(bytes),&data,0):page.ib->Lock(UINT(offset),UINT(bytes),&data,0);
                if(FAILED(hr))return false;
                if(data)std::memcpy(data,(vb?reinterpret_cast<const char*>(cpu.vertices.data()):reinterpret_cast<const char*>(cpu.indices.data()))+offset,bytes);
                hr=vb?page.vb->Unlock():page.ib->Unlock();return data&&SUCCEEDED(hr);
            };
            while(pageBudget.available()&&next.upload.status()==NorthlightWorldMeshPages::Result::Pending){
                if(next.upload.advance(perform,pageBudget)==NorthlightWorldMeshPages::Result::Failed)return streamingFailure(E_FAIL,"mesh page upload");
            }
            size_t budget=streamBudget.remainingBytes();
            auto canStage=[&]{return pageBudget.available()&&next.upload.status()==NorthlightWorldMeshPages::Result::Ready;};
            {auto phase=streamingPhases.measure(NorthlightStreaming::PhaseProfile::Materials);
            unsigned count=0;
            while(canStage()&&budget&&next.material<plan.materials.size()&&count<16){
                const auto& m=plan.materials[next.material];
                const bool white=m.width==1&&m.height==1&&m.bgra.size()==4&&
                    m.bgra[0]==255&&m.bgra[1]==255&&m.bgra[2]==255&&m.bgra[3]==255;
                if(white&&next.white){next.white->AddRef();next.textures.push_back(next.white);++next.material;++count;continue;}
                if(!next.partialTexture){
                    HRESULT hr=d->CreateTexture(m.width,m.height,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&next.partialTexture,nullptr);
                    if(FAILED(hr)||!next.partialTexture)return streamingFailure(FAILED(hr)?hr:E_FAIL,"staged material");
                    next.materialRow=0;
                }
                if(!canStage())break;
                const size_t rowBytes=size_t(m.width)*4;
                if(rowBytes>budget)break;
                const UINT rows=UINT(std::min({size_t(m.height)-next.materialRow,budget/rowBytes,size_t(64)}));
                RECT rect={0,LONG(next.materialRow),LONG(m.width),LONG(next.materialRow+rows)};
                D3DLOCKED_RECT lock={};HRESULT hr=next.partialTexture->LockRect(0,&lock,&rect,0);
                if(FAILED(hr))return streamingFailure(hr,"staged material lock");
                if(!lock.pBits||lock.Pitch<INT(rowBytes)){next.partialTexture->UnlockRect(0);return streamingFailure(E_FAIL,"staged material layout");}
                for(UINT y=0;y<rows;++y)memcpy((char*)lock.pBits+y*lock.Pitch,m.bgra.data()+(next.materialRow+y)*rowBytes,rowBytes);
                hr=next.partialTexture->UnlockRect(0);if(FAILED(hr))return streamingFailure(hr,"staged material unlock");
                next.materialRow+=rows;budget-=rows*rowBytes;streamBudget.consume(rows*rowBytes);
                if(next.materialRow==m.height){
                    auto* t=next.partialTexture;t->PreLoad();next.partialTexture=nullptr;next.textures.push_back(t);++next.material;++count;++next.textureUploads;
                    if(white){next.white=t;t->AddRef();}
                }
            }
            }
            if(streamBudget.available()&&next.upload.status()==NorthlightWorldMeshPages::Result::Ready&&next.material==plan.materials.size()){
                auto phase=streamingPhases.measure(NorthlightStreaming::PhaseProfile::Commit);
                // Allocate small commit metadata before releasing the previous GPU set.
                std::vector<float> cutoffs;cutoffs.reserve(plan.materials.size());
                for(const auto& material:plan.materials)cutoffs.push_back(material.alphaCutoff);
                std::vector<Batch> committedBatches=plan.pages->batches;
                std::string committedMap=next.map;
                auto committedFixed=plan.fixedTerrainChunks;
                std::vector<StaticShadow::Placement> committedOwners;
                committedOwners.reserve(plan.completePlacements.size());
                for(const auto& owner:plan.completePlacements){StaticShadow::Placement p;
                    p.uid=owner.uid;p.category=owner.category;p.modelKey=owner.modelKey;
                    std::copy(owner.matrix,owner.matrix+9,p.matrix);p.translation=owner.translation;p.low=owner.low;p.high=owner.high;
                    committedOwners.push_back(std::move(p));
                }
                // Keep the previous complete geometry drawable until publication fits.
                if(uploadedTextureBytes<=NorthlightStreaming::DeferredRelease<IDirect3DTexture9>::MaxBytes&&
                   !retiredMaterials.enqueue(materials,uploadedTextureBytes))return vertices&&indices&&uploadedMap==active->map;
                clearMesh();activeMesh=stagingSlot();vertices=meshPool[activeMesh][0].vb;indices=meshPool[activeMesh][0].ib;
                materials=std::move(next.textures);uploadedTextureBytes=size_t(plan.textureBytes);vertexCount=plan.vertexCount;
                uploadedLocalShadowRecords=plan.localShadowRecords;
                batches=std::move(committedBatches);uploadedAlphaCutoffs=std::move(cutoffs);
                fixedTerrainChunks=std::move(committedFixed);uploadedStaticOwners=std::move(committedOwners);
                uploaded=next.bvh;uploadedMap=std::move(committedMap);
                ++meshGeneration;
                logf("WORLD staged mesh committed: vertices=%u triangles=%zu materials=%zu textureUploads=%u stagingFrames=%u stagingMs=%lu",vertexCount,size_t(plan.triangleCount),materials.size(),next.textureUploads,next.frames,(unsigned long)(GetTickCount()-next.started));
                retirePendingCpu();pendingMesh.reset();meshRetry.clear();
            }
        }
        if(!vertices||!indices||uploadedMap!=active->map)return false;
        // Geometry can be ready before a new map's first GI solve. Shadows
        // may render immediately; GridInfo.w prevents reading the old atlas.
        if(active->serial==0)uploadedSerial=0;
        if(uploadedSerial!=active->serial){
            auto phase=streamingPhases.measure(NorthlightStreaming::PhaseProfile::Probes);
            if(active->atlas.size()!=NorthlightGI::ProbeAtlasSize)return false;
            probeActivation.begin(active->map);
            float activationNow=float(DWORD(GetTickCount()-animationEpoch))*.001f;
            constexpr unsigned n=NorthlightGI::ProbeAtlasN;
            for(int channel=0;channel<5;++channel){D3DLOCKED_RECT lock;if(!check(probe[channel]->LockRect(0,&lock,nullptr,0),"probe upload"))return false;
                for(unsigned z=0;z<n;++z)for(unsigned y=0;y<n;++y)for(unsigned x=0;x<n;++x){
                    const auto& entry=active->atlas[x+y*n+z*n*n];const auto& p=entry.probe;
                    float* dst=(float*)((char*)lock.pBits+y*lock.Pitch)+(x+z*n)*4;
                    if(channel<3){for(int k=0;k<4;++k)dst[k]=channel==0?p.sh[k].x:channel==1?p.sh[k].y:p.sh[k].z;}
                    else if(channel==3)for(unsigned axis=0;axis<6;++axis){float* moment=(float*)((char*)lock.pBits+(y+axis*n)*lock.Pitch)+(x+z*n)*4;moment[0]=p.moments[axis].mean;moment[1]=p.moments[axis].meanSquare;moment[2]=entry.occupied&&p.valid?1.f:0.f;moment[3]=0;}
                    else{dst[0]=float(entry.key.x);dst[1]=float(entry.key.y);dst[2]=float(entry.key.z);dst[3]=probeActivation.update(x+y*n+z*n*n,entry,activationNow);}
                }
                probe[channel]->UnlockRect(0);
            }
            uploadedSerial=active->serial;
        }return true;
    }
    bool uploadLiveTerrain(){
        terrainUploadBytes=0;terrainUploadReused=false;
        liveTerrainGPU.beginFrame();
        if(liveTerrainGeneration==meshGeneration&&frameTerrain==uploadedTerrain && (frameTerrain.empty()||(liveTerrainGPU.vertices()&&liveIndicesGPU))){terrainUploadReused=true;return true;}
        NorthlightGeometryMemory::Sample arenaMemory;bool arenaSampled=false;
        const HRESULT terrainResult=liveTerrainGPU.update(d,frameTerrain,
            [&](size_t bytes){
                if(bytes>8*NorthlightGeometryMemory::MiB){
                    if(memoryPressureUntil&&GetTickCount()<memoryPressureUntil)return false;
                    arenaMemory=geometryMemory();arenaSampled=true;
                    // Refusing optional retention capacity must not suppress
                    // the original 8 MiB active-set allocation for one second.
                    return NorthlightGeometryMemory::admits(arenaMemory,NorthlightGeometryMemory::dynamicBudget(bytes));
                }
                return admitsGrowth("live-terrain-arena",bytes,arenaSampled?&arenaMemory:nullptr);
            },
            [](const NorthlightTerrainCapture::Position& p){NorthlightGI::WorldVertex v;v.position=V(p.x,p.y,p.z);return v;});
        if(terrainResult==S_FALSE){
            // Preserve the existing memory-pressure fallback: the complete
            // cached terrain covers this frame; do not advertise absent live chunks.
            liveTerrainIndices.clear();liveDirectionalIndices.clear();liveTerrainChunks.clear();uploadedTerrain.clear();return true;
        }
        if(!check(terrainResult,"live terrain arena upload"))return false;
        liveTerrainIndices.reserve(frameTerrainIndices);liveDirectionalIndices.reserve(frameTerrainIndices);
        if(!liveTerrainGPU.indices(frameTerrain,meshGeneration,[&](const auto& snapshot,uint32_t offset,std::vector<uint32_t>& output){
            NorthlightShadowTerrain::appendLiveDirectional(snapshot,fixedTerrainChunks,offset,output);
        },liveTerrainIndices,liveDirectionalIndices))return check(E_FAIL,"live terrain arena membership");
        if(liveTerrainIndices.empty()){uploadedTerrain=frameTerrain;liveTerrainGeneration=meshGeneration;return true;}
        UINT ib=UINT((liveTerrainIndices.size()+liveDirectionalIndices.size())*sizeof(uint32_t));
        if(ib>liveIndexBytes){
            const UINT ibCapacity=roundBuffer(ib,8*NorthlightGeometryMemory::MiB);
            if(!admitsGrowth("live-terrain-index-growth",ibCapacity)){
                liveTerrainIndices.clear();liveDirectionalIndices.clear();liveTerrainChunks.clear();uploadedTerrain.clear();return true;
            }
            drop(liveIndicesGPU);liveIndexBytes=ibCapacity;
            if(!check(d->CreateIndexBuffer(liveIndexBytes,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,D3DPOOL_DEFAULT,&liveIndicesGPU,nullptr),"live index buffer"))return false;
        }
        void* data=nullptr;
        if(!check(liveIndicesGPU->Lock(0,ib,&data,D3DLOCK_DISCARD),"live index upload"))return false;
        const size_t originalBytes=liveTerrainIndices.size()*sizeof(uint32_t);
        memcpy(data,liveTerrainIndices.data(),originalBytes);
        if(!liveDirectionalIndices.empty())memcpy(static_cast<char*>(data)+originalBytes,liveDirectionalIndices.data(),liveDirectionalIndices.size()*sizeof(uint32_t));
        if(!check(liveIndicesGPU->Unlock(),"live index unlock"))return false;
        uploadedTerrain=frameTerrain;liveTerrainGeneration=meshGeneration;terrainUploadBytes=liveTerrainGPU.uploadedBytes+ib;
        return true;
    }
    HRESULT quad(UINT w,UINT h){struct Q{float x,y,z,rhw,u,v;};Q q[]={{-.5f,-.5f,0,1,0,0},{float(w)-.5f,-.5f,0,1,1,0},{-.5f,float(h)-.5f,0,1,0,1},{float(w)-.5f,float(h)-.5f,0,1,1,1}};D3DVIEWPORT9 vp={0,0,w,h,0,1};d->SetViewport(&vp);return d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(Q));}
public:
    // Buffer identity for the snapshot cache: a private-data token created on
    // first sight and owned by the COM object, so pointer reuse cannot alias.
    static std::uint64_t bufferIdentity(void* buffer,bool indexBuffer){
        return NorthlightTrackedBuffers::identity(buffer,indexBuffer);
    }
    void setConstantEpochSource(void* context,NorthlightConstantEpoch::Reader reader){
        // A different provider can have the same numeric serials.
        for(auto& replay:replays)replay->constantStamp={};
        constantEpochContext=context;constantEpochReader=reader;
    }
    explicit WorldRenderer(IDirect3DDevice9* device):d(device),stateBlocks(device){terrainBoundsCache.setIdentityProvider(&bufferIdentity);terrainBoundsCache.setVersionProvider(&NorthlightTrackedBuffers::version);terrainBoundsCache.setDeclarationCache(&declarationCache);replaySnapshots.setDeclarationProvider(&declarationCache,[](void* c,IDirect3DVertexDeclaration9* decl,const D3DVERTEXELEMENT9*& e,UINT& n){return static_cast<NorthlightVertexDeclarations::Cache*>(c)->get(decl,e,n);});replaySnapshots.setLayoutProvider(&declarationCache,[](void* c,IDirect3DVertexDeclaration9* decl,UINT* extent){return static_cast<NorthlightVertexDeclarations::Cache*>(c)->captureLayout(decl,extent);});replaySnapshots.setIdentityProvider(&bufferIdentity);replaySnapshots.setVersionProvider(&NorthlightTrackedBuffers::version);replaySnapshots.setMetadataProvider(&NorthlightTrackedBuffers::captureMetadata);QueryPerformanceFrequency(&captureFrequency);char path[MAX_PATH*3];WideCharToMultiByte(CP_UTF8,0,rootPath,-1,path,sizeof path,nullptr,nullptr);root=path;std::string paletteError;
        (void)NorthlightStreaming::cpuRetirement();
        bool paletteLoaded=NorthlightCelestialProfiles::load(root+"celestial-profiles.ini",celestialProfiles,paletteError);
        logf("CELESTIAL profiles loaded=%d zones=%zu %s",paletteLoaded,celestialProfiles.zones.size(),paletteError.c_str());
        std::string rangeError;bool rangesLoaded=NorthlightRegionalShadow::load(root+"shadow-range-profiles.ini",shadowRanges,rangeError);
        logf("SHADOW regional terrain loaded=%d zones=%zu %s",rangesLoaded,shadowRanges.zones.size(),rangeError.c_str());
        staticStream=std::make_unique<StaticShadow::Streamer>(root+"world-cache");
        staticCasters.setAdmission([this](size_t bytes){return admitStaticAllocation(bytes);});
        worker=std::thread([this]{work();});}
    ~WorldRenderer(){{std::lock_guard<std::mutex> lock(mutex);stopping=true;}wake.notify_one();if(worker.joinable())worker.join();releaseGPU();for(auto& p:captureShaders)drop(p.second.replacement);for(auto& p:terrainShadowShaders)drop(p.second);}
    void releaseGPU(){replayBoundsMetadata.clear();replayBoundsCache.clear();declarationCache.clear();uploadedStaticOwners.clear();staticOwnerGeneration=UINT64_MAX;staticCasters.reset();staticMatcher.clear();staticScene.reset();staticRetryTick=0;stateBlocks.clear();uploadedTerrain.clear();liveTerrainIndices.clear();liveDirectionalIndices.clear();fixedTerrainChunks.clear();liveTerrainGeneration=0;drop(regionalFogTexture);uploadedFogField.reset();releasePointGPU();probeActivation.reset();drop(baselineSurface);drop(baselineLight);releaseReplayGPU();liveTerrainGPU.clear();drop(liveIndicesGPU);liveIndexBytes=0;pendingMesh.reset();clearMesh();retiredMaterials.clear();releaseMeshPool();uploadedMap.clear();for(auto& t:shadow)drop(t);for(auto& s:shadowSurface)drop(s);for(auto& t:shadowCache)drop(t);for(auto& s:shadowCacheSurface)drop(s);drop(shadowCacheDepth);drop(shadowScratch);drop(shadowScratchSurface);drop(unionPS);invalidateShadowCache();for(auto& t:probe)drop(t);drop(shadowDepth);drop(lightSurface);drop(fogSurface);drop(fogBlurredSurface);drop(colorSurface);drop(light);drop(fog);drop(fogBlurred);drop(color);drop(lightingPS);drop(giPS);drop(fogPS);drop(fogBlurPS);drop(localDirectPS);drop(temporalPS);drop(localFogPS);drop(normalsPS);drop(normalBuffer);drop(normalSurface);drop(sourceVisPS);for(int a=0;a<2;++a)for(int b=0;b<2;++b){drop(sourceVis[a][b]);drop(sourceVisSurface[a][b]);}sourceVisValid=false;for(int i=0;i<2;++i){drop(temporalLight[i]);drop(temporalLightSurface[i]);drop(temporalDepth[i]);drop(temporalDepthSurface[i]);}temporalValid=false;drop(finalPS);drop(shadowPS);drop(replayPS);drop(shadowVS);drop(cachedShadowVS);drop(cachedShadowPS);drop(cachedFastPS);drop(cachedOpaqueFastPS);drop(cachedOpaquePS);drop(shadowDecl);width=height=0;uploadedSerial=0;}
    // Explicit enable/retry only, called after the wrapper's clearFrame(). This
    // never calls endFrame(), so packet capture and cleanup run exactly once.
    void recover(){meshRetry.clear();if(!failed)return;releaseGPU();failed=false;valid=false;streamingReports=0;logf("WORLD explicit recovery requested");}
    void releaseStateCache(){stateBlocks.clear();invalidateShadowCache();}
    void reset(){shadowsComposited=false;pivotValid=false;pivotDistance=12.f;endFrame();replaySnapshots.clearIndexCache();actorJobComplete.reset();actorJobSerial_=0;actorSceneMap_.clear();lastActorCapture=0;terrainBoundsCache.clearPersistent();previousCacheHits=0;freeReplays.clear();pooledSnapshotBytes=0;valid=false;failed=false;releaseGPU();}
    void endFrame(bool retainPool=true){
        paletteFrameValid=false;staticPivotReady=false;
        if(!valid||failed||workerFault())stateBlocks.clear();
        if(captureSampled&&captureFrequency.QuadPart>0){double ms=1000.0/double(captureFrequency.QuadPart);
            logf("WORLD CPU capture terrain=%.3fms replay=%.3fms terrainCalls=%u terrainUP=%u replayCandidates=%u unknownCalls=%u acceptedReplay=%zu",terrainCaptureTicks*ms,replayCaptureTicks*ms,terrainCaptureCalls,terrainUPCalls,replayCaptureCalls,unknownCaptureCalls,replays.size());
        }
        if(captureSampled)logf("WORLD capture cache hits=%llu frameHits=%llu entries=%zu bytes=%llu constantsReadBytes=%zu constantReadCalls=%zu sm1Draws=%u relativeDraws=%u",(unsigned long long)terrainBoundsCache.persistentHits(),(unsigned long long)(terrainBoundsCache.persistentHits()-previousCacheHits),terrainBoundsCache.persistentEntries(),(unsigned long long)terrainBoundsCache.persistentBytes(),capturedConstantBytes,capturedConstantCalls,capturedSM1Draws,capturedRelativeDraws);
        if(captureSampled)logf("WORLD optimized caches terrainTrackedHits=%llu terrainAvoidedBytes=%llu boundsHits=%llu boundsMisses=%llu boundsAvoidedVertices=%llu boundsLookupMs=%.3f staticMaintenanceVisits=%llu staticPublications=%llu",
            (unsigned long long)terrainBoundsCache.trackedHits(),(unsigned long long)terrainBoundsCache.avoidedReadBytes(),
            (unsigned long long)replayBoundsCache.hits(),(unsigned long long)replayBoundsCache.misses(),(unsigned long long)replayBoundsCache.avoidedVertices(),replayBoundsCache.lookupMilliseconds(),
            (unsigned long long)staticCasters.stats().maintenanceVisits,(unsigned long long)staticCasters.stats().publications);
        if(captureSampled)logf("MODEL index cache hits=%u misses=%u entries=%u bytes=%zu snapshotCacheHits=%u snapshotCacheMisses=%u snapshotCacheEntries=%zu snapshotCacheBytes=%zu revalidated=%u revalidationMismatches=%u trackedHits=%u avoidedReadBytes=%zu",replaySnapshots.indexCacheHits(),replaySnapshots.indexCacheMisses(),replaySnapshots.indexCacheEntries(),replaySnapshots.indexCacheBytes(),replaySnapshots.snapshotCacheHits(),replaySnapshots.snapshotCacheMisses(),replaySnapshots.snapshotCacheEntries(),replaySnapshots.snapshotCacheBytes(),replaySnapshots.snapshotRevalidated(),replaySnapshots.snapshotRevalidationMismatches(),replaySnapshots.trackedHits(),replaySnapshots.avoidedReadBytes());
        if(captureSampled)logf("MODEL capture metadata batches=%u knownBuffers=%u fallbackBuffers=%u fastSnapshotHits=%u",replaySnapshots.metadataBatches(),replaySnapshots.metadataHits(),replaySnapshots.metadataFallbacks(),replaySnapshots.fastCacheHits());
        if(captureSampled)logf("MODEL frame capture skinnedCandidates=%u accepted=%u blendRejected=%u projectionRejected=%u budgetRejected=%u snapshotRejected=%u acceptedSkinnedBytes=%zu acceptedOtherBytes=%zu rejectedReadBytes=%zu totalReadBytes=%zu compactedDraws=%u spanVertices=%zu uniqueVertices=%zu savedVertexBytes=%zu",skinnedCandidates,skinnedAccepted,skinnedBlendRejected,skinnedProjectionRejected,skinnedBudgetRejected,skinnedSnapshotRejected,acceptedSkinnedBytes,acceptedOtherBytes,captureRejectedBytes,replaySnapshots.bytesRead(),replaySnapshots.compactedDraws(),replaySnapshots.sourceSpanVertices(),replaySnapshots.uniqueVertices(),replaySnapshots.savedVertexBytes());
        if(captureSampled&&captureFrequency.QuadPart>0){
            const double ms=1000.0/double(captureFrequency.QuadPart);
            // Raw totals of the rotating subset; do not multiply by 16. Rejected
            // candidates contribute only the phases they actually reached.
            const auto& c=capturePhases.ticks;
            logf("MODEL capture phases stride=16 measured=%u accepted=%u clockReads=%u stateMs=%.3f projectionMs=%.3f snapshotMs=%.3f constantsMs=%.3f materialMs=%.3f finalizeMs=%.3f actorMs=%.3f alphaQueriesSkipped=%u",capturePhaseDraws,capturePhaseAccepted,capturePhases.clockReads,c[CaptureState]*ms,c[CaptureProjection]*ms,c[CaptureSnapshot]*ms,c[CaptureConstants]*ms,c[CaptureMaterial]*ms,c[CaptureFinalize]*ms,c[CaptureActor]*ms,alphaStateQueriesSkipped);
            const auto& a=actorPhases.ticks;
            logf("MODEL actor capture phases candidates=%u textureReads=%u packets=%u vertices=%u clockReads=%u probeMs=%.3f textureMs=%.3f packetMs=%.3f",actorPhaseCandidates,actorTextureReads,actorDraws,actorVerticesEvaluated,actorPhases.clockReads,a[0]*ms,a[1]*ms,a[2]*ms);
            const auto& u=replayUploadPhases.ticks;
            logf("MODEL GPU upload phases calls=%u fallbackRetries=%u clockReads=%u cacheMs=%.3f bulkMs=%.3f",replayUploadCalls,replayUploadFallbacks,replayUploadPhases.clockReads,u[0]*ms,u[1]*ms);
        }
        if(captureSampled)logf("MODEL capture constants epochTests=%llu snapshotHits=%llu bytesAvoided=%llu poseFastHits=%llu",
            (unsigned long long)constantEpochStats.tests,(unsigned long long)constantEpochStats.hits,
            (unsigned long long)constantEpochStats.bytesAvoided,(unsigned long long)constantEpochStats.poseFastHits);
        clearCaptureDiagnostics();if(captureSampled)++capturePhaseSerial;
        skinnedCandidates=skinnedBlendRejected=skinnedProjectionRejected=skinnedBudgetRejected=skinnedSnapshotRejected=skinnedAccepted=0;
        captureRejectedBytes=acceptedSkinnedBytes=acceptedOtherBytes=0;
        previousCacheHits=terrainBoundsCache.persistentHits();capturedConstantBytes=capturedConstantCalls=0;capturedSM1Draws=capturedRelativeDraws=0;
        terrainCaptureTicks=replayCaptureTicks=0;terrainCaptureCalls=terrainUPCalls=replayCaptureCalls=unknownCaptureCalls=0;captureSampled=false;
        valid=false;shadowFrameReady=false;legacyFog=NorthlightLegacyFog::Constants{};
        // Bound retained vector capacities across changing scenes. Reuse storage,
        // never old geometry: each subsequent draw still re-reads every byte.
        // Give the current scene first claim on the pool, instead of letting
        // unused records from a previous crowded frame monopolize its budget.
        if(retainPool){
            for(auto& unused:freeReplays){pooledSnapshotBytes-=unused->snapshot.capacityBytes();unused->snapshot=NorthlightDrawSnapshot::Mesh{};}
            for(auto& p:replays)recycleReplay(p.release());replays.clear();
        }else{replays.clear();freeReplays.clear();pooledSnapshotBytes=0;}
        terrainBoundsCache.clearFrame();liveTerrainChunks.clear();frameTerrain.clear();frameTerrainVertices=frameTerrainIndices=0;pointLiveBatches.clear();pointReady=false;
        replaySnapshots.clearFrame();actorCaptureDecided=false;actorJob.reset();actorVerticesEvaluated=actorDraws=actorSkippedAlpha=0;
    }
    bool ready()const{return valid&&active&&active->bvh&&!failed&&!workerFault();}
    bool hasContext()const{return valid&&!failed&&!workerFault();}
    const float* legacyFogParameters()const{return legacyFog.parameters;}
    const NorthlightCelestialProfiles::Profile& celestialPalette(const char* map,const float* camera){
        // Freeze once per frame: early disc, late halo, direct/GI and fog cannot
        // observe different worker publications or temporal transition states.
        if(paletteFrameValid&&paletteFrameMap==map)return framePalette;
        std::shared_ptr<const PaletteRegion> region;
        {std::lock_guard<std::mutex> lock(mutex);region=publishedPaletteRegion;}
        auto target=celestialProfiles.fallback;
        if(region&&region->map==map)target=NorthlightCelestialProfiles::sample(celestialProfiles,region->region,map,camera[0],camera[1]);
        framePalette=paletteTransition.update(target,map,camera[0],camera[1],double(GetTickCount())*.001);
        paletteFrameMap=map;paletteFrameValid=true;
        DWORD now=GetTickCount();if(!paletteLogAt||now-paletteLogAt>=10000){paletteLogAt=now;
            uint32_t zone=region&&region->map==map?NorthlightRegionalFog::zoneAt(region->region,camera[0],camera[1]):0;
            logf("CELESTIAL palette map=%s zone=%u moonDisc=(%.3f %.3f %.3f) lightMix=%.3f/%.3f strength=%.3f/%.3f",map,zone,framePalette.disc[1][0],framePalette.disc[1][1],framePalette.disc[1][2],framePalette.mix[0],framePalette.mix[1],framePalette.strength[0],framePalette.strength[1]);
        }return framePalette;
    }
    bool celestialContext(NorthlightCelestial::Context& out,float* inverseView,float* projectionOut)const{
        if(!hasContext()||!celestialValid)return false;
        out=celestial;memcpy(inverseView,context.inverseView,64);memcpy(projectionOut,projection,12);return true;
    }
    bool waterContext(NorthlightWaterContext& out,float nearZ,float farZ,float minZ,float maxZ)const{
        if(!hasContext())return false;
        memcpy(out.inverseView,context.inverseView,sizeof out.inverseView);memcpy(out.projection,projection,sizeof projection);
        out.nearZ=nearZ;out.farZ=farZ;out.minZ=minZ;out.maxZ=maxZ;
        const auto copy=[](float* to,V from){to[0]=from.x;to[1]=from.y;to[2]=from.z;};
        copy(out.sunDirection,sourceDirections[0]);copy(out.sunColor,sourceColors[0]);
        copy(out.moonDirection,sourceDirections[1]);copy(out.moonColor,sourceColors[1]);memcpy(out.skyColor,context.ambient,12);
        out.seconds=float(DWORD(GetTickCount()-animationEpoch))*.001f;
        if(shadowFrameReady)for(int source=0;source<2;++source){
            if(NorthlightGI::dot(sourceColors[source],sourceColors[source])<1e-10f)continue;
            copy(out.sourceShadowDirection[source],sourceDirections[source]);
            for(int cascade=0;cascade<2;++cascade){out.sourceShadows[source][cascade]=shadow[source*2+cascade];memcpy(out.sourceShadowMatrices[source][cascade],sourceMatrices[source][cascade],64);}
        }
        return true;
    }
    bool recognizesWmo(IDirect3DVertexShader9* shader)const{return wmoShaders.count(shader)!=0;}
    bool isWorldShader(IDirect3DVertexShader9* shader)const{return terrainShaders.count(shader)||captureShaders.count(shader)||wmoShaders.count(shader);}
    bool isSkinnedShader(IDirect3DVertexShader9* shader)const{auto it=actorPrograms.find(shader);return it!=actorPrograms.end()&&it->second.skinned;}
    bool wmoContext(IDirect3DVertexShader9* shader){
        if(failed)return false;if(valid)return true;
        auto it=wmoShaders.find(shader);if(it==wmoShaders.end())return false;
        float columns[16],rows[16];Projection q;
        if(FAILED(d->GetVertexShaderConstantF(2,columns,4))||!decodeColumnProjection(columns,q,rows))return false;
        char map[64]={};NorthlightWorldCamera::Camera camera;NorthlightWorldCamera::Diagnostics why;
        NorthlightWmoContext::Lighting light;
        bool cameraRead=NorthlightWorldCamera::read(map,rows[11],camera,&why);
        bool globalRead=cameraRead&&NorthlightWmoContext::readGlobalLighting(camera.camera,light);
        bool lightingRead=globalRead&&NorthlightWmoContext::context(camera.view,light,context);
        if(cameraRead&&!lightingRead&&it->second->lighting){float values[12];
            lightingRead=SUCCEEDED(d->GetVertexShaderConstantF(10,values,3))&&NorthlightWmoContext::decodeLitShader(it->second->hash,camera.view,values,context);
        }
        if(!cameraRead||!lightingRead){
            if(++wmoRejects==1||wmoRejects%3600==0)logf("CITY context rejected camera=%s count=%u",NorthlightWorldCamera::rejectName(why.reason),wmoRejects);
            return false;
        }
        valid=true;projection[0]=rows[0];projection[1]=rows[5];projection[2]=rows[11];
        readOriginalFog(30,it->second->fog);
        updateWorldContext(map,camera.camera);
        if(++wmoContexts==1||wmoContexts%600==0)logf("CITY WMO context accepted map=%s count=%u fogProof=%d globalLight=%d",map,wmoContexts,it->second->fog,globalRead);
        return true;
    }
    void terrainContext(){
        if(valid||failed)return;float view[16]={},lighting[12]={},p[16]={},camera[3]={};char map[64]={};
        bool registers=SUCCEEDED(d->GetVertexShaderConstantF(0,view,4))&&SUCCEEDED(d->GetVertexShaderConstantF(4,p,4));
        bool nativeRead=SUCCEEDED(d->GetVertexShaderConstantF(24,lighting,3));
        bool gameContext=NorthlightWorldContext::readMapAndCamera(map,camera);
        NorthlightWorldCamera::Camera independent;NorthlightWorldCamera::Diagnostics why;char cameraMap[64]={};
        bool cameraRead=registers&&gameContext&&NorthlightWorldCamera::read(cameraMap,p[11],independent,&why);
        bool cameraMatches=cameraRead&&!std::strcmp(map,cameraMap)&&NorthlightWorldCamera::agreesWithTerrain(independent,view);
        NorthlightWmoContext::Lighting global;
        bool globalRead=cameraMatches&&NorthlightWmoContext::readGlobalLighting(camera,global);
        bool decoded=registers&&gameContext&&NorthlightWmoContext::terrainContext(view,nativeRead?lighting:nullptr,camera,globalRead?&global:nullptr,context);
        bool agreement=decoded;
        if(!agreement){
            if(++contextRejects==1||contextRejects%3600==0)logf("WORLD context rejected: registers=%d affineLight=%d clientRead=%d cameraAgreement=%d map=%s shaderCamera=(%.2f %.2f %.2f) gameCamera=(%.2f %.2f %.2f) light=(%.3f %.3f %.3f) count=%u",registers,decoded,gameContext,agreement,map,context.camera[0],context.camera[1],context.camera[2],camera[0],camera[1],camera[2],lighting[0],lighting[1],lighting[2],contextRejects);return;}
        if(++cameraChecks==1||cameraChecks%3600==0)logf("CITY camera audit read=%d terrainAgreement=%d reject=%s permissiveSignatures=%d failingSignature=%d",cameraRead,cameraMatches,NorthlightWorldCamera::rejectName(why.reason),int(NorthlightWorldCamera::kPermissiveCameraSignatures),NorthlightWorldCamera::failingSignature());
        valid=true;projection[0]=p[0];projection[1]=p[5];projection[2]=p[11];
        readOriginalFog(12,true);
        updateWorldContext(map,camera);
    }
    void readOriginalFog(UINT fogRegister,bool known){
        // Read the original terrain fog before any extension render pass changes constants/state.
        IDirect3DPixelShader9* originalPS=nullptr;FogShader fogShader;
        if(SUCCEEDED(d->GetPixelShader(&originalPS))&&originalPS){
            auto it=fogShaders.find(originalPS);if(it!=fogShaders.end())fogShader=it->second;
        }drop(originalPS);
        float fogParameters[4]={},fogColor[4]={};DWORD fogEnabled=0,fogTable=0,fogARGB=0;
        bool fogRegisters=known&&SUCCEEDED(d->GetVertexShaderConstantF(fogRegister,fogParameters,1));
        if(fogShader.major==3)fogRegisters=fogRegisters&&SUCCEEDED((fogShader.colorRegister>=0?d->GetPixelShaderConstantF(UINT(fogShader.colorRegister),fogColor,1):D3DERR_INVALIDCALL));
        else fogRegisters=fogRegisters&&SUCCEEDED(d->GetRenderState(D3DRS_FOGENABLE,&fogEnabled))&&SUCCEEDED(d->GetRenderState(D3DRS_FOGTABLEMODE,&fogTable))&&SUCCEEDED(d->GetRenderState(D3DRS_FOGCOLOR,&fogARGB));
        bool fogKnown=fogRegisters&&NorthlightLegacyFog::decode(fogShader.major,fogShader.verified,fogEnabled!=0,fogTable,fogParameters,fogColor,fogARGB,projection[2],legacyFog);
        if(++fogReports==1||fogReports%600==0)logf("WORLD legacy fog known=%d ps=%u verified=%d colorRegister=%d enabled=%.0f params=(%.7g %.7g %.7g) color=(%.5f %.5f %.5f)",fogKnown,fogShader.major,fogShader.verified,fogShader.colorRegister,legacyFog.parameters[3],legacyFog.parameters[0],legacyFog.parameters[1],legacyFog.parameters[2],legacyFog.color[0],legacyFog.color[1],legacyFog.color[2]);
    }
    void updateWorldContext(const char* map,const float* camera){
        if(unsigned fault=workerFault()){if(!failed)logf("WORLD worker stopped: %s; restart required",workerFaultMessage(fault));failed=true;valid=false;return;}
        if(!reportedContext){logf("WORLD context validated: map=%s camera=(%.2f %.2f %.2f) sun=(%.3f %.3f %.3f)",map,camera[0],camera[1],camera[2],context.lightDirection[0],context.lightDirection[1],context.lightDirection[2]);reportedContext=true;}
        celestialValid=NorthlightCelestial::read(camera,context.direct,celestial);
        if(celestialValid){
            // A pure render-clock orbit shared by discs, shadows and fog.
            const float nativeSunAlpha=celestial.sun.alpha,nativeMoonAlpha=celestial.moon.alpha;
            const double nativeSun=NorthlightCelestialOrbit::elevationDegrees(celestial.sun.direction[2]);
            const double nativeMoon=NorthlightCelestialOrbit::elevationDegrees(celestial.moon.direction[2]);
            const auto orbit=NorthlightCelestial::resolveRendererSky(celestial,context.direct);
            const auto lightOrbit=celestialLightMotion.update(orbit,celestial.dayFraction,GetTickCount(),
                celestialLightMap!=map||different(celestialLightCamera,vec(camera),40));
            celestialLightMap=map;celestialLightCamera=vec(camera);
            celestialLight=celestial;
            std::memcpy(celestialLight.sun.direction,lightOrbit.sun.direction,sizeof celestialLight.sun.direction);
            std::memcpy(celestialLight.moon.direction,lightOrbit.moon.direction,sizeof celestialLight.moon.direction);
            NorthlightCelestial::applyRendererPolicy(celestialLight,context.direct);
            const auto palette=celestialPalette(map,camera);
            NorthlightCelestialProfiles::apply(palette,context.direct,celestial);
            NorthlightCelestialProfiles::apply(palette,context.direct,celestialLight);
            continuousCelestialShadows=false; // Native-speed orbit: ordinary cache policy, no accelerated phases.
            if(++celestialOrbitReports%600==1)logf("CELESTIAL orbit gameDay=%.6f sun=%.2f->%.2f moon=%.2f->%.2f schedule=native sunCrest=85 moonCrest=43 weights=%.3f/%.3f nativeAlpha=%.3f/%.3f rendererAlpha=%.1f/%.1f",celestial.dayFraction,nativeSun,orbit.sun.elevation,nativeMoon,orbit.moon.elevation,celestial.sunWeight,celestial.moonWeight,nativeSunAlpha,nativeMoonAlpha,celestial.sun.alpha,celestial.moon.alpha);
        }
        if(!celestialValid){celestialLightMotion.reset();continuousCelestialShadows=false;}
        sourceDirections[0]=vec(context.lightDirection);sourceColors[0]=vec(context.direct);
        sourceDirections[1]=V(0,0,1);sourceColors[1]=V();sourceWeights[0]=1;sourceWeights[1]=0;
        auto resolvedSources=NorthlightCelestialSources::resolve(vec(context.lightDirection),vec(context.direct),
            celestialValid?vec(celestialLight.sun.direction):vec(context.lightDirection),
            celestialValid?vec(celestialLight.moon.direction):V(0,0,1),
            celestialValid?celestialLight.sunWeight:0.f,celestialValid?celestialLight.moonWeight:0.f);
        if(celestialValid){resolvedSources.sources[0].color=vec(celestialLight.sunColor);resolvedSources.sources[1].color=vec(celestialLight.moonColor);}
        authoredFill=resolvedSources.authoredFill;
        for(unsigned source=0;source<2;++source){sourceDirections[source]=continuousCelestialShadows?resolvedSources.sources[source].direction:NorthlightWorldMath::quantizeDirection(resolvedSources.sources[source].direction);sourceColors[source]=resolvedSources.sources[source].color;sourceWeights[source]=resolvedSources.sources[source].weight;}
        requestStaticCasters(map);
        Request r;r.map=map;r.camera=vec(camera);r.light.sunDirection=sourceDirections[0];r.light.sunIrradiance=sourceColors[0]*3.14159265f;r.light.skyRadiance=vec(context.ambient);
        r.light.additionalDirections.push_back({sourceDirections[1],sourceColors[1]*3.14159265f});
        if(lastRequest.map==r.map&&completedActorSceneMap()==r.map){r.actorJob=completedActorJob();r.actorSerial=actorJobSerial();}
        std::shared_ptr<Snapshot> retiredSnapshot;
        {std::lock_guard<std::mutex> lock(mutex);
            if(published&&published!=observedPublication.lock()){
                observedPublication=published;if(!published->message.empty())logf("WORLD cache: %s",published->message.c_str());
            }
            if(published&&published!=active&&NorthlightWorldStreaming::applicable(published->map,published->center,r.map,r.camera)){
                bool newGI=published->serial&&(!active||active->serial!=published->serial);retiredSnapshot=std::move(active);active=published;
                if(newGI)logf("GI activated tick=%lu id=%llu ageMs=%lu queueMs=%lu geometryMs=%lu actorMs=%lu solveMs=%lu reused=%u solved=%u cancelled=%u origin=(%.1f %.1f %.1f) dynamicReused=%u dynamicSolved=%u staticOnly=%u partial=%u processed=%u generation=%llu retargeted=%u",(unsigned long)GetTickCount(),(unsigned long long)active->requestId,(unsigned long)(GetTickCount()-active->requestedAt),(unsigned long)active->queueMs,(unsigned long)active->geometryMs,(unsigned long)active->actorMs,(unsigned long)active->solveMs,active->reusedProbes,active->solvedProbes,active->superseded,active->origin.x,active->origin.y,active->origin.z,active->dynamicReused,active->dynamicSolved,unsigned(active->staticOnly),unsigned(active->partial),active->processedProbes,(unsigned long long)active->lightingGeneration,active->retargeted);
            }
            r.reason=(lastRequest.map!=r.map?1u:0u)|(different(quantize(lastRequest.camera,8),quantize(r.camera,8),.1f)?2u:0u)|
                (different(lastRequest.light.sunDirection,r.light.sunDirection,.02f)?4u:0u)|(different(lastRequest.light.sunIrradiance,r.light.sunIrradiance,.03f)?8u:0u)|(different(lastRequest.light.skyRadiance,r.light.skyRadiance,.01f)?16u:0u)|
                (lastRequest.light.additionalDirections.empty()||different(lastRequest.light.additionalDirections[0].direction,r.light.additionalDirections[0].direction,.02f)||different(lastRequest.light.additionalDirections[0].irradiance,r.light.additionalDirections[0].irradiance,.03f)?32u:0u);
            if(r.reason){r.id=request.id+1;r.baseId=r.id;r.queuedAt=GetTickCount();request=r;lastRequest=r;pending=true;wake.notify_one();
                logf("GI request tick=%lu id=%llu reason=%u camera=(%.2f %.2f %.2f) sun=(%.5f %.5f %.5f)",(unsigned long)r.queuedAt,(unsigned long long)r.id,r.reason,r.camera.x,r.camera.y,r.camera.z,r.light.sunDirection.x,r.light.sunDirection.y,r.light.sunDirection.z);
            }}
        if(retiredSnapshot)NorthlightStreaming::cpuRetirement().retire(retiredSnapshot,retiredSnapshot->retirementBytes);
        retiredSnapshot.reset();
        if(active&&!NorthlightWorldStreaming::applicable(active->map,active->center,r.map,r.camera)){NorthlightStreaming::cpuRetirement().retire(active,active->retirementBytes);active.reset();}
        DWORD now=GetTickCount();if(now-diagnosticTick>=250){diagnosticTick=now;
            logf("WORLD camera tick=%lu rendered=%u eye=(%.2f %.2f %.2f) forward=(%.4f %.4f %.4f) sun=(%.5f %.5f %.5f) GI=%llu pivotDistance=%.1f pivotUpdates=%u",(unsigned long)now,frames,context.camera[0],context.camera[1],context.camera[2],context.inverseView[8]*projection[2],context.inverseView[9]*projection[2],context.inverseView[10]*projection[2],context.lightDirection[0],context.lightDirection[1],context.lightDirection[2],(unsigned long long)(active?active->serial:0),pivotDistance,pivotUpdates);
        }
    }
    IDirect3DPixelShader9* terrainShadowReplacement(IDirect3DPixelShader9* shader){
        auto found=terrainShadowShaders.find(shader);if(found!=terrainShadowShaders.end())return found->second;
        IDirect3DPixelShader9* replacement=nullptr;UINT size=0;
        if(shader&&SUCCEEDED(shader->GetFunction(nullptr,&size))&&size>=8&&size<=65536&&size%4==0){
            std::vector<uint32_t> code(size/4),output;NorthlightTerrainShadow::PatchInfo info;
            if(SUCCEEDED(shader->GetFunction(code.data(),&size))&&NorthlightTerrainShadow::patch(code.data(),code.size(),output,&info)){
                if(FAILED(d->CreatePixelShader(reinterpret_cast<const DWORD*>(output.data()),&replacement)))replacement=nullptr;
                if(replacement&&terrainShadowReports++<4)logf("TERRAIN SHADOW patched ps_%u_%u: replaced=%u shadow=r%u.%u one=c%d.%u instructions=%u",info.major,info.minor,info.replaced,info.shadowRegister,info.shadowComponent,info.oneConstant,info.oneComponent,info.instructions);
            }
        }
        if(replacement)++terrainShadowPatched;else ++terrainShadowRejected;
        terrainShadowShaders[shader]=replacement;return replacement;
    }
    bool terrainShadowActive()const{return ready()&&shadowsComposited;}
    void registerPixelShader(IDirect3DPixelShader9* shader){
        fogShaders.erase(shader);UINT size=0;
        {auto old=terrainShadowShaders.find(shader);if(old!=terrainShadowShaders.end()){drop(old->second);terrainShadowShaders.erase(old);}}
        if(!shader||FAILED(shader->GetFunction(nullptr,&size))||size<8||size>65536||size%4)return;
        std::vector<uint32_t> code(size/4);if(FAILED(shader->GetFunction(code.data(),&size)))return;
        FogShader info;info.major=(code[0]>>8)&255;
        info.colorRegister=NorthlightLegacyFog::ps3FogColorRegister(code.data(),code.size());info.verified=info.colorRegister>=0;fogShaders[shader]=info;
    }
    void registerShader(IDirect3DVertexShader9* shader,uint64_t hash){
        replayBoundsMetadata.invalidateShader(shader);
        wmoShaders.erase(shader);if(auto* info=NorthlightWmoContext::signature(hash))wmoShaders[shader]=info;
        terrainShaders.erase(shader);if(contains(kTerrainVS,hash))terrainShaders.insert(shader);
        auto begin=std::begin(kWorldShaderSignatures),end=std::end(kWorldShaderSignatures);
        auto it=std::lower_bound(begin,end,hash,[](const WorldShaderSignature& a,uint64_t h){return a.hash<h;});
        auto old=captureShaders.find(shader);if(old!=captureShaders.end()){drop(old->second.replacement);captureShaders.erase(old);}
        actorPrograms.erase(shader);actorUVPrograms.erase(shader);
        if(it==end||it->hash!=hash)return;
        unsigned kind=contains(kTerrainVS,hash)?1:it->projectionKind;
        if(kind==1)return; // Terrain uses an immediate position snapshot, including SM1.
        UINT size=0;if(FAILED(shader->GetFunction(nullptr,&size))||size>65536)return;
        std::vector<uint32_t> words(size/4),output;
        if(FAILED(shader->GetFunction(words.data(),&size)))return;
        NorthlightShadowShader::PatchInfo info;
        if(!NorthlightShadowShader::patch(words.data(),words.size(),output,&info)||!info.texcoord0XY)return;
        NorthlightDrawSnapshot::Ref<IDirect3DVertexShader9> replacement;
        if(SUCCEEDED(d->CreateVertexShader(reinterpret_cast<const DWORD*>(output.data()),replacement.out()))&&replacement.p){
            CaptureShader metadata;metadata.replacement=replacement.p;metadata.projectionKind=kind;
            metadata.usage=NorthlightShaderConstants::analyze(words.data(),words.size(),kind);
            NorthlightActorDeformation::Program program;
            if(NorthlightActorDeformation::compile(words.data(),words.size(),program)){
                metadata.skinned=program.skinned;metadata.sm1=program.major==1;
                actorPrograms.emplace(shader,std::move(program));
            }
            if(NorthlightActorDeformation::compile(words.data(),words.size(),program,true))actorUVPrograms.emplace(shader,std::move(program));
            // Publish only complete metadata. RAII retains ownership on any
            // compiler/map allocation exception before this transfer.
            captureShaders.emplace(shader,metadata);replacement.p=nullptr;
        }
    }
    void appendTerrain(TerrainSnapshot snapshot){
        bool fresh=false;for(auto& chunk:snapshot->bounds.chunks)if(!liveTerrainChunks.count({chunk.x,chunk.y}))fresh=true;
        if(!fresh)return;
        if(frameTerrainVertices+snapshot->positions.size()>262144||frameTerrainIndices+snapshot->indices.size()>1572864)return;
        pointRecordTerrain(UINT(frameTerrainIndices),UINT(snapshot->indices.size()/3),snapshot->bounds);
        for(auto& chunk:snapshot->bounds.chunks)liveTerrainChunks.emplace(chunk.x,chunk.y);
        frameTerrainVertices+=snapshot->positions.size();frameTerrainIndices+=snapshot->indices.size();
        frameTerrain.push_back(std::move(snapshot));++terrainSnapshots;
    }
    void captureUP(D3DPRIMITIVETYPE type,UINT minimum,UINT vertexTotal,UINT count,const void* indexData,D3DFORMAT format,const void* vertexData,UINT stride,bool indexed,IDirect3DVertexShader9* shader,bool sample){
        if(!ready())return;
        if(!terrainShaders.count(shader)){captureSampled|=sample;captureModel(type,0,minimum,vertexTotal,0,count,indexed,shader,sample,indexData,format,vertexData,stride);return;}
        captureSampled|=sample;if(sample){++terrainCaptureCalls;++terrainUPCalls;}CpuScope cpu(sample?&terrainCaptureTicks:nullptr);
        ++terrainAttempts;NorthlightTerrainCapture::MeshSnapshot snapshot;NorthlightTerrainCapture::Diagnostics why;
        bool ok=indexed?terrainBoundsCache.readMeshUP(d,type,minimum,vertexTotal,count,indexData,format,vertexData,stride,context.view,snapshot,&why):
                        terrainBoundsCache.readPrimitiveUP(d,type,count,vertexData,stride,context.view,snapshot,&why);
        if(!ok){if(terrainFailures++<12)logf("TERRAIN UP snapshot rejected: %s hr=%08lx type=%u stride=%u",NorthlightTerrainCapture::rejectName(why.reason),(unsigned long)why.hr,why.positionType,why.stride);return;}
        appendTerrain(std::make_shared<const NorthlightTerrainCapture::MeshSnapshot>(std::move(snapshot)));
    }
    void capture(D3DPRIMITIVETYPE type,INT base,UINT min,UINT vertexTotal,UINT start,UINT count,bool indexed,IDirect3DVertexShader9* current,bool sample){
        if(!ready()||replays.size()>=4096||(type!=D3DPT_TRIANGLELIST&&type!=D3DPT_TRIANGLESTRIP))return;
        captureSampled|=sample;bool isTerrain=terrainShaders.count(current)!=0;
        if(isTerrain){
            if(sample)++terrainCaptureCalls;CpuScope cpu(sample?&terrainCaptureTicks:nullptr);
            ++terrainAttempts;
            if(!indexed){if(terrainFailures++<12)logf("TERRAIN snapshot rejected: non-indexed buffer draw");return;}
            float projectionRows[16];if(FAILED(d->GetVertexShaderConstantF(4,projectionRows,4))){if(terrainFailures++<12)logf("TERRAIN projection constants unavailable");return;}
            if(std::fabs(projectionRows[0]-projection[0])>.005f||std::fabs(projectionRows[5]-projection[1])>.005f||std::fabs(projectionRows[11]-projection[2])>.005f){if(terrainFailures++<12)logf("TERRAIN projection mismatch");return;}
            TerrainSnapshot snapshot;NorthlightTerrainCapture::Diagnostics why;
            if(!terrainBoundsCache.readMeshShared(d,type,base,min,vertexTotal,start,count,context.view,snapshot,&why)){
                if(terrainFailures++<12)logf("TERRAIN snapshot rejected: %s hr=%08lx vertexUsage=%lx indexUsage=%lx type=%u stride=%u",NorthlightTerrainCapture::rejectName(why.reason),(unsigned long)why.hr,(unsigned long)why.vertexUsage,(unsigned long)why.indexUsage,why.positionType,why.stride);return;}
            appendTerrain(snapshot);return;
        }
        captureModel(type,base,min,vertexTotal,start,count,indexed,current,sample,nullptr,D3DFMT_INDEX16,nullptr,0);
    }
    void releaseReplayGPU(){replayGpuCache.clear();for(auto& buffer:replayVerticesGPU)drop(buffer);drop(replayIndicesGPU);for(auto& bytes:replayVertexBytes)bytes=0;replayIndexBytes=0;}
    bool uploadReplay(bool allowCache=true){
        NorthlightCapturePhases::Scope<2> phase(captureSampled?&replayUploadPhases:nullptr);
        if(captureSampled)++replayUploadCalls;
        if(!allowCache){replayGpuCache.clear();for(auto& p:replays)if(p->gpuCached){for(auto& stream:p->stream)drop(stream);drop(p->index);}}
        replayGpuCache.beginFrame();bool memoryChecked=false,memoryAllowed=false;
        auto admission=[&](size_t){if(!memoryChecked){memoryChecked=true;memoryAllowed=admitsGrowth("replay-cache",64*NorthlightGeometryMemory::MiB);}return memoryAllowed;};
        for(auto& p:replays){p->gpuCached=allowCache&&replayGpuCache.bind(d,p->shared,p->stream,p->index,admission);
            if(p->gpuCached){for(unsigned s=0;s<4;++s){p->offset[s]=0;p->stride[s]=p->mesh().streams[s].stride;}p->start=0;}}
        phase.next(1); // Layout, bulk-buffer growth/copy and output bindings.
        UINT totals[4]={};std::uint64_t totalIndices=0;
        auto layout=[&](size_t count){
            for(auto& t:totals)t=0;totalIndices=0;
            for(size_t i=0;i<count;++i){auto& p=replays[i];if(p->gpuCached)continue;for(unsigned s=0;s<4;++s){auto& source=p->mesh().streams[s];p->stride[s]=source.stride;p->offset[s]=totals[s];
                    std::uint64_t next=std::uint64_t(totals[s])+((source.bytes.size()+15)&~std::size_t(15));if(next>64u*1024u*1024u)return false;totals[s]=UINT(next);}
                p->start=p->indexed?UINT(totalIndices):0;totalIndices+=p->mesh().indices.size();}
            return totalIndices<=16u*1024u*1024u;
        };
        if(!layout(replays.size()))return false;
        {bool grow=totalIndices*4>replayIndexBytes;uint64_t capacity=0;
         for(unsigned s=0;s<4;++s){if(totals[s]>replayVertexBytes[s])grow=true;capacity+=totals[s]>replayVertexBytes[s]?roundBuffer(totals[s],8*NorthlightGeometryMemory::MiB):replayVertexBytes[s];}
         capacity+=totalIndices*4>replayIndexBytes?roundBuffer(totalIndices*4,8*NorthlightGeometryMemory::MiB):replayIndexBytes;
         if(grow&&!admitsGrowth("replay-growth",capacity)){
             // Optional residency must not displace casters that the original
             // bulk path could fit. Reclaim it before applying the old fallback.
             if(allowCache&&replayGpuCache.bytes()){
                 if(captureSampled)++replayUploadFallbacks;
                 phase.stop(); // The recursive retry owns its own interval.
                 return uploadReplay(false);
             }
             // Under address-space pressure draw the prefix of casters that fits
             // the existing buffers this frame; nothing is allocated.
             size_t fit=0;UINT running[4]={};std::uint64_t runningIndices=0;
             for(;fit<replays.size();++fit){auto& p=replays[fit];if(p->gpuCached)continue;bool ok=true;
                 for(unsigned s=0;s<4&&ok;++s){std::uint64_t next=std::uint64_t(running[s])+((p->mesh().streams[s].bytes.size()+15)&~std::size_t(15));ok=next<=replayVertexBytes[s];}
                 if(!ok||(runningIndices+p->mesh().indices.size())*4>replayIndexBytes)break;
                 for(unsigned s=0;s<4;++s)running[s]=UINT(running[s]+((p->mesh().streams[s].bytes.size()+15)&~std::size_t(15)));runningIndices+=p->mesh().indices.size();}
             replays.resize(fit);if(!layout(fit))return false;
         }}
        for(unsigned s=0;s<4;++s){if(!totals[s])continue;
            if(replayVertexBytes[s]<totals[s]){drop(replayVerticesGPU[s]);replayVertexBytes[s]=roundBuffer(totals[s],8*NorthlightGeometryMemory::MiB);
                if(!check(d->CreateVertexBuffer(replayVertexBytes[s],D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&replayVerticesGPU[s],nullptr),"model snapshot vertex buffer"))return false;}
            void* memory=nullptr;if(!check(replayVerticesGPU[s]->Lock(0,totals[s],&memory,D3DLOCK_DISCARD),"model snapshot vertex lock"))return false;
            for(auto& p:replays){if(p->gpuCached)continue;auto& source=p->mesh().streams[s];if(!source.bytes.empty())std::memcpy(static_cast<std::uint8_t*>(memory)+p->offset[s],source.bytes.data(),source.bytes.size());}
            if(!check(replayVerticesGPU[s]->Unlock(),"model snapshot vertex unlock"))return false;
        }
        UINT indexBytes=UINT(totalIndices*4);
        if(indexBytes){if(replayIndexBytes<indexBytes){drop(replayIndicesGPU);replayIndexBytes=roundBuffer(indexBytes,8*NorthlightGeometryMemory::MiB);
                if(!check(d->CreateIndexBuffer(replayIndexBytes,D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,D3DFMT_INDEX32,D3DPOOL_DEFAULT,&replayIndicesGPU,nullptr),"model snapshot index buffer"))return false;}
            void* memory=nullptr;if(!check(replayIndicesGPU->Lock(0,indexBytes,&memory,D3DLOCK_DISCARD),"model snapshot index lock"))return false;
            for(auto& p:replays)if(!p->gpuCached&&!p->mesh().indices.empty())std::memcpy(static_cast<std::uint32_t*>(memory)+p->start,p->mesh().indices.data(),p->mesh().indices.size()*4);
            if(!check(replayIndicesGPU->Unlock(),"model snapshot index unlock"))return false;
        }
        for(auto& p:replays){if(p->gpuCached)continue;for(unsigned s=0;s<4;++s){drop(p->stream[s]);if(!p->mesh().streams[s].bytes.empty()){p->stream[s]=replayVerticesGPU[s];p->stream[s]->AddRef();}}
            drop(p->index);if(p->indexed){p->index=replayIndicesGPU;p->index->AddRef();}}
        phase.stop(); // Report I/O is not part of cache/bulk timing.
        if(captureSampled){size_t bulk=indexBytes;for(auto total:totals)bulk+=total;
            logf("MODEL GPU cache hits=%u reusedBytes=%zu newBytes=%zu residentBytes=%zu bulkUploadBytes=%zu",replayGpuCache.hits(),replayGpuCache.reused(),replayGpuCache.uploaded(),replayGpuCache.bytes(),bulk);
            const auto population=replayGpuCache.population();const auto& policy=replayGpuCache.stats();
            logf("MODEL GPU policy entries=%zu resident=%zu probation=%zu countPressure=%u bytePressure=%u roomRejected=%u promotionCountOnly=%u promotionCountRejected=%u evictions=%u warmup=%u uploadDeferred=%u admissionRejected=%u entryLimit=%zu scanPasses=%u scanVisits=%u scanMemoHits=%u attemptDeferred=%u uploadByteDeferred=%u expiredEntries=%u expiredBytes=%zu evictionChecks=%u lruMoves=%u",population.entries,population.resident,population.probation,policy.countPressure,policy.bytePressure,policy.roomRejected,policy.promotionCountOnly,policy.promotionCountRejected,policy.evictions,policy.warmup,policy.uploadDeferred,policy.admissionRejected,replayGpuCache.entryLimit(),policy.scanPasses,policy.scanVisits,policy.scanMemoHits,policy.attemptDeferred,policy.uploadByteDeferred,policy.expiredEntries,policy.expiredBytes,policy.evictionChecks,policy.lruMoves);
            const auto& cleared=replayGpuCache.clearStats();
            logf("MODEL GPU clears lifetimeCalls=%llu lifetimeEntries=%llu lifetimeBytes=%llu",(unsigned long long)cleared.calls,(unsigned long long)cleared.entries,(unsigned long long)cleared.bytes);
        }
        return true;
    }
    bool actorMaterial(IDirect3DBaseTexture9* base,NorthlightGI::WorldMaterial& material){
        if(!base||base->GetType()!=D3DRTYPE_TEXTURE)return false;
        auto* texture=static_cast<IDirect3DTexture9*>(base);UINT levels=texture->GetLevelCount();if(!levels)return false;
        D3DSURFACE_DESC desc={};UINT level=0;
        for(;level<levels;++level){if(FAILED(texture->GetLevelDesc(level,&desc)))return false;if(desc.Width<=128&&desc.Height<=128)break;}
        if(level==levels||!desc.Width||!desc.Height)return false;
        using Format=NorthlightActorTexture::Format;Format format;
        switch(desc.Format){case D3DFMT_A8R8G8B8:format=Format::BGRA8;break;case D3DFMT_X8R8G8B8:format=Format::BGRX8;break;case D3DFMT_R5G6B5:format=Format::RGB565;break;
        case D3DFMT_DXT1:format=Format::BC1;break;case D3DFMT_DXT3:format=Format::BC2;break;case D3DFMT_DXT5:format=Format::BC3;break;default:return false;}
        bool compressed=desc.Format==D3DFMT_DXT1||desc.Format==D3DFMT_DXT3||desc.Format==D3DFMT_DXT5;
        if(desc.Usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL))return false;
        UINT rows=compressed?(desc.Height+3)/4:desc.Height;
        UINT rowBytes=compressed?((desc.Width+3)/4)*(desc.Format==D3DFMT_DXT1?8:16):desc.Width*(desc.Format==D3DFMT_R5G6B5?2:4);
        std::vector<std::uint8_t> raw(std::size_t(rows)*rowBytes); // Allocate before taking a game texture lock.
        D3DLOCKED_RECT locked={};if(FAILED(texture->LockRect(level,&locked,nullptr,D3DLOCK_READONLY)))return false;
        bool readable=locked.pBits&&locked.Pitch>=INT(rowBytes);
        if(readable)for(UINT row=0;row<rows;++row)std::memcpy(raw.data()+std::size_t(row)*rowBytes,static_cast<const std::uint8_t*>(locked.pBits)+std::size_t(row)*unsigned(locked.Pitch),rowBytes);
        HRESULT hr=texture->UnlockRect(level);if(FAILED(hr)||!readable)return false;
        if(!NorthlightActorTexture::decode(raw.data(),raw.size(),desc.Width,desc.Height,rowBytes,format,material.rgba))return false;
        material.width=desc.Width;material.height=desc.Height;material.albedo=V(1,1,1);return true;
    }
    bool actorCaptureEnabled(){if(!actorCaptureDecided){actorCaptureDecided=true;actorCaptureDue=!lastActorCapture||GetTickCount()-lastActorCapture>=200;}if(actorCaptureDue&&!actorJob)actorJob=std::make_shared<NorthlightActorGeometry::ActorJob>();return actorCaptureDue;}
    void appendActor(IDirect3DVertexShader9* original,const Replay& replay,bool sample=false){
        if(!actorCaptureEnabled())return;
        auto it=actorPrograms.find(original);if(it==actorPrograms.end())return;const auto& program=it->second;
        if(!program.skinned&&!replay.mesh().dynamic)return;
        if(actorJob->packets.size()>=128)return;
        if(actorVerticesEvaluated+replay.mesh().vertexCount>16384)return;
        NorthlightCapturePhases::Scope<3> phase(sample?&actorPhases:nullptr);
        if(sample)++actorPhaseCandidates;
        const D3DVERTEXELEMENT9* elements=nullptr;UINT n=0;if(!declarationCache.get(replay.decl,elements,n))return;
        // Evaluate one vertex first to avoid skinning distant crowds. 96 units
        // allows large models intersecting the exact 48-unit triangle region.
        NorthlightDrawSnapshot::Mesh first;first.vertexCount=1;
        for(unsigned s=0;s<4;++s){first.streams[s].stride=replay.mesh().streams[s].stride;if(!replay.mesh().streams[s].bytes.empty())first.streams[s].bytes.assign(replay.mesh().streams[s].bytes.begin(),replay.mesh().streams[s].bytes.begin()+first.streams[s].stride);}
        std::vector<NorthlightActorDeformation::Position> positions;
        if(!NorthlightActorDeformation::worldPositions(program,first,elements,n,replay.constants,context.inverseView,positions))return;
        V firstPosition(positions[0].x,positions[0].y,positions[0].z);V distance=firstPosition-vec(context.camera);if(NorthlightGI::dot(distance,distance)>96*96)return;
        actorVerticesEvaluated+=replay.mesh().vertexCount;
        phase.next(1); // Material read/decode, still using current game texture.
        NorthlightGI::WorldMaterial material;material.albedo=V(.35f,.35f,.35f);material.alphaCutoff=std::max(0.f,replay.cutoff);
        auto uvProgram=actorUVPrograms.find(original);bool hasUV=uvProgram!=actorUVPrograms.end();
        if(sample&&hasUV)++actorTextureReads;
        bool textureRead=hasUV&&actorMaterial(replay.texture,material);
        if(replay.cutoff>=0&&!textureRead){++actorSkippedAlpha;return;}
        material.addressU=replay.addressU;material.addressV=replay.addressV;
        phase.next(2); // Immutable packet construction and queueing.
        NorthlightActorGeometry::Packet packet;packet.sharedMesh=replay.shared;if(!packet.sharedMesh)packet.mesh=replay.mesh();packet.position=program;
        if(hasUV)packet.uv=uvProgram->second;packet.hasUV=hasUV;packet.alphaTest=replay.cutoff>=0;
        packet.elements.assign(elements,elements+n);std::memcpy(packet.constants.data(),replay.constants,sizeof replay.constantStorage);std::memcpy(packet.inverseView.data(),context.inverseView,64);
        packet.material=std::move(material);actorJob->packets.push_back(std::move(packet));++actorDraws;
    }
    void finishActorScene(){
        if(!actorCaptureEnabled())return;lastActorCapture=GetTickCount();actorJob->center=vec(context.camera);
        actorJobComplete=std::move(actorJob);actorCaptureDue=false;++actorJobSerial_;actorSceneMap_=active?active->map:std::string{};
        if(captureSampled)logf("WORLD actor packets draws=%u queuedVertices=%u skippedAlpha=%u snapshotReadBytes=%zu material=actual-rgba128-or-neutral035",actorDraws,actorVerticesEvaluated,actorSkippedAlpha,replaySnapshots.bytesRead());
    }
    std::shared_ptr<const NorthlightActorGeometry::ActorJob> completedActorJob()const{return actorJobComplete;}
    uint64_t actorJobSerial()const{return actorJobSerial_;}
    const std::string& completedActorSceneMap()const{return actorSceneMap_;}
    void prepareStaticProofs(){
        for(auto& p:replays)p->staticProofMask=0;
        if(!staticScene||staticScene->map!=lastRequest.map||!staticCasters.stats().readyModels||replays.empty())return;
        StaticShadowDedup::Budget budget;
        try {
            // Expensive shader evaluation is bounded too, not only matching.
            // Rotate the starting point so a difficult first draw cannot starve
            // other eligible rigid WMO packets indefinitely.
            const size_t start=staticDedupCursor++%replays.size();size_t evaluated=0;
            for(size_t j=0;j<replays.size()&&!budget.expired()&&evaluated<1024;++j){
                auto& p=*replays[(start+j)%replays.size()];const auto& mesh=p.mesh();
                auto program=actorPrograms.find(p.originalShader);
                if(!recognizesWmo(p.originalShader)||program==actorPrograms.end()||program->second.skinned||mesh.dynamic||p.cutoff>=0||!mesh.vertexCount||mesh.vertexCount>256||mesh.primitiveCount>512)continue;
                if(program->second.operations.size()*mesh.vertexCount>8192)continue;
                const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;
                if(!declarationCache.get(p.decl,elements,count))continue;
                evaluated+=mesh.vertexCount;++staticDedupAttempts;
                std::vector<NorthlightActorDeformation::Position> positions;
                if(!NorthlightActorDeformation::worldPositions(program->second,mesh,elements,count,p.constants,context.inverseView,positions)||budget.expired())continue;
                std::vector<StaticShadowDedup::Triangle> triangles;
                if(!StaticShadowDedup::makeTriangles(positions,mesh.indices,mesh.indexed,mesh.primitiveCount,mesh.topology==D3DPT_TRIANGLESTRIP,triangles,512))continue;
                auto proof=staticMatcher.match(*staticScene,triangles,{true,false,false,false},budget,
                    [this](const StaticShadow::Placement& place,uint64_t revision){return staticCasters.coverage(place,revision)?15u:0u;});
                if(!proof.valid)continue;
                ++staticDedupMatches;p.staticProofMask=proof.slotMask;
                p.staticProofLow=V(INFINITY,INFINITY,INFINITY);p.staticProofHigh=V(-INFINITY,-INFINITY,-INFINITY);
                for(const auto& t:triangles)for(auto v:t){p.staticProofLow.x=std::min(p.staticProofLow.x,v.x);p.staticProofLow.y=std::min(p.staticProofLow.y,v.y);p.staticProofLow.z=std::min(p.staticProofLow.z,v.z);p.staticProofHigh.x=std::max(p.staticProofHigh.x,v.x);p.staticProofHigh.y=std::max(p.staticProofHigh.y,v.y);p.staticProofHigh.z=std::max(p.staticProofHigh.z,v.z);}
            }
        }catch(...){for(auto& p:replays)p->staticProofMask=0;}
    }
    uint64_t staticSignature(const float* matrix){
        try {return staticCasters.signature(matrix);}
        catch(...){staticCasters.reset();staticOwnerGeneration=UINT64_MAX;staticRetryTick=GetTickCount();return 0;}
    }
    NorthlightLocalShadowSignature::Digest localShadowSignature(ShadowCacheKey& key,const float* matrix){
        return key.localMemo.get(uploadedLocalShadowRecords.get(),meshGeneration,fixedTerrainChunks,matrix);
    }
    bool drawStaticCasters(const float* matrix){
        try {if(staticCasters.draw(d,matrix))return true;}catch(...){}
        if(staticDrawFailures++<8)logf("STATIC SHADOW draw retry: base terrain/local shadows retained");
        staticCasters.reset();staticOwnerGeneration=UINT64_MAX;staticRetryTick=GetTickCount();return false;
    }
    void captureModel(D3DPRIMITIVETYPE type,INT base,UINT minimum,UINT vertexTotal,UINT start,UINT count,bool indexed,IDirect3DVertexShader9* current,bool sample,const void* userIndices,D3DFORMAT userFormat,const void* userVertices,UINT userStride){
        auto it=captureShaders.find(current);if(it==captureShaders.end()){if(sample)++unknownCaptureCalls;return;}
        const auto& metadata=it->second;
        if(replays.size()>=4096)return;
        if(sample)++replayCaptureCalls;CpuScope cpu(sample?&replayCaptureTicks:nullptr);
        // Rotate the 1/16 draw subset between sampled frames. No timing calls
        // or new diagnostic counters run in this path on ordinary frames.
        const bool detailed=sample&&((replayCaptureCalls-1)&15u)==(capturePhaseSerial&15u);
        NorthlightCapturePhases::Scope<CapturePhaseCount> phase(detailed?&capturePhases:nullptr,CaptureState);
        if(detailed)++capturePhaseDraws;
        const bool priority=metadata.skinned;
        if(sample&&priority)++skinnedCandidates;
        // Only reject when no legal snapshot can fit, including cache hits
        // and UP draws. A previous oversized failure is not an exhaustion proof.
        if(replaySnapshots.captureExhausted(priority)){
            if(sample&&priority){++skinnedSnapshotRejected;++skinnedBudgetRejected;}
            return;
        }
        DWORD blend=0,alpha=0,ref=0,alphaFunc=D3DCMP_ALWAYS;if(FAILED(d->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend))||blend){if(sample&&priority)++skinnedBlendRejected;return;}
        if(FAILED(d->GetRenderState(D3DRS_ALPHATESTENABLE,&alpha)))return;
        if(alpha){if(FAILED(d->GetRenderState(D3DRS_ALPHAREF,&ref))||FAILED(d->GetRenderState(D3DRS_ALPHAFUNC,&alphaFunc)))return;}
        else if(sample)alphaStateQueriesSkipped+=2;
        if(alpha&&alphaFunc!=D3DCMP_GREATER&&alphaFunc!=D3DCMP_GREATEREQUAL)return;
        // Validate the main camera before touching model buffers. Full constant
        // banks are copied only for snapshots that survive range/budget checks.
        phase.next(CaptureProjection);
        float q[16];const unsigned kind=metadata.projectionKind;
        if(FAILED(d->GetVertexShaderConstantF(kind==1?4:2,q,4)))return;
        if(sample){capturedConstantBytes+=64;++capturedConstantCalls;}
        if(std::fabs(q[0]-projection[0])>.005f||std::fabs(q[5]-projection[1])>.005f||std::fabs(q[kind==1?11:14]-projection[2])>.005f||std::fabs(q[15])>.001f){if(sample&&priority)++skinnedProjectionRejected;return;}
        // A rejected draw returns its record to the pool as well. Exhausted
        // geometry budgets must not allocate/zero a fresh constant bank per draw.
        phase.next(CaptureSnapshot);
        std::unique_ptr<Replay,ReplayRecycle> p(acquireReplay().release(),ReplayRecycle{this});p->projectionKind=kind;p->shader=metadata.replacement;p->shader->AddRef();p->originalShader=current;p->originalShader->AddRef();p->pointBounds={};
        if(FAILED(d->GetVertexDeclaration(&p->decl))||!p->decl)return;
        NorthlightDrawSnapshot::Draw draw{type,base,minimum,vertexTotal,start,count,indexed};NorthlightDrawSnapshot::Diagnostics why;
        const size_t readBefore=replaySnapshots.bytesRead();
        bool captured=userVertices?replaySnapshots.readUP(p->decl,draw,userIndices,userFormat,userVertices,userStride,p->snapshot,&why,priority):replaySnapshots.read(d,p->decl,draw,p->snapshot,&why,priority,&p->shared);
        if(!captured){if(sample){captureRejectedBytes+=replaySnapshots.bytesRead()-readBefore;if(priority){++skinnedSnapshotRejected;skinnedBudgetRejected+=why.error==NorthlightDrawSnapshot::Error::Budget;}}if(snapshotRejects++<12)logf("MODEL snapshot rejected: %s hr=%08lx stream=%u",NorthlightDrawSnapshot::errorName(why.error),(unsigned long)why.hr,why.stream);return;}
        phase.next(CaptureConstants);
        const auto& usage=metadata.usage;
        p->constantUsage=usage;
        const auto f=usage.floats,b=usage.booleans,i=usage.integers;
        const Replay* previous=replays.empty()?nullptr:replays.back().get();
        if(!NorthlightReplayCaptureConstants::capture(*p,previous,constantEpochContext,constantEpochReader,[&]{
            if((f.count&&FAILED(d->GetVertexShaderConstantF(f.first,p->constantStorage+4*f.first,f.count)))||
               (b.count&&FAILED(d->GetVertexShaderConstantB(b.first,p->boolStorage+b.first,b.count)))||
               (i.count&&FAILED(d->GetVertexShaderConstantI(i.first,p->intStorage+4*i.first,i.count))))return false;
            if(sample){capturedConstantBytes+=16*f.count+4*b.count+16*i.count;
                capturedConstantCalls+=(f.count!=0)+(b.count!=0)+(i.count!=0);}
            return true;
        },sample?&constantEpochStats:nullptr))return;
        if(sample){capturedSM1Draws+=metadata.sm1;capturedRelativeDraws+=usage.relativeFloat;}
        phase.next(CaptureMaterial);
        if(FAILED(d->GetTexture(0,&p->texture))||(alpha&&!p->texture))return;
        if(FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSU,&p->addressU))||FAILED(d->GetSamplerState(0,D3DSAMP_ADDRESSV,&p->addressV)))return;
        if(p->addressU<D3DTADDRESS_WRAP||p->addressU>D3DTADDRESS_CLAMP||p->addressV<D3DTADDRESS_WRAP||p->addressV>D3DTADDRESS_CLAMP)return;
        phase.next(CaptureFinalize);
        p->cutoff=alpha?(float(ref)+(alphaFunc==D3DCMP_GREATER?.5f:0.f))/255.f:-1.f;
        p->type=type;p->base=0;p->min=0;p->vertices=p->mesh().vertexCount;p->start=0;p->count=count;p->indexed=indexed;
        if(sample){if(priority){++skinnedAccepted;acceptedSkinnedBytes+=p->mesh().byteSize();}else acceptedOtherBytes+=p->mesh().byteSize();}
        p->constantGroup=replays.empty()?0:replays.back()->constantGroup+
            (NorthlightReplayCaptureConstants::samePose(*replays.back(),*p,sample?&constantEpochStats:nullptr)?0:1);
        phase.next(CaptureActor);
        appendActor(current,*p,sample);
        phase.next(CaptureFinalize);
        replays.emplace_back(p.release());if(detailed)++capturePhaseAccepted;
    }

    bool render(IDirect3DSurface9* targetSurface,IDirect3DTexture9* depth,UINT w,UINT h,D3DFORMAT fmt,float nearZ,float farZ,float minZ,float maxZ,int debug,NorthlightGpuProfile* profile,IDirect3DTexture9* waterMask){
        shadowsComposited=false;
        if(debug!=1)gpuDiagnosticArmed=true;
        if(workerFault())return false;
        DWORD submissionStart=GetTickCount();
        finishActorScene();
        {std::lock_guard<std::mutex> lock(mutex);
         if(valid&&completedActorSceneMap()==lastRequest.map&&!pending&&!workerBusy&&actorJobSerial()!=lastRequest.actorSerial&&submissionStart-actorRequestTick>=250){
             Request r=lastRequest;r.actorJob=completedActorJob();r.actorSerial=actorJobSerial();r.reason=64;
             r.id=request.id+1;r.queuedAt=submissionStart;request=r;lastRequest=r;pending=true;actorRequestTick=submissionStart;wake.notify_one();
         }}
        NorthlightStreaming::Budget streamBudget;
        if(!ready()||!resources(w,h,fmt)||!upload(streamBudget)){
            // Cache maintenance must not depend on the local GI becoming ready.
            // No new GPU allocation competes with a deferred world build here.
            updateStaticCasters(false,&streamBudget);return false;
        }
        streamBudget.pause();
        if(!uploadLiveTerrain()||!uploadReplay()){streamBudget.resume();updateStaticCasters(false,&streamBudget);return false;}
        streamBudget.resume();
        SavedState save(d,&stateBlocks);if(!save.ok)return false;
        if(profile)profile->mark("WorldUpload");
        {auto phase=streamingPhases.measure(NorthlightStreaming::PhaseProfile::Static);
         updateStaticCasters(true,&streamBudget);
         prepareStaticProofs();}
        if(profile)profile->mark("StaticCasterUpload");
        streamingCpuPeakMs=std::max(streamingCpuPeakMs,streamBudget.elapsedMs());
        streamingBudgetOverruns+=streamBudget.elapsedMs()>1.0;
        if(captureSampled){logf("WORLD streaming optionalCpuPeakMs=%.3f softBudgetOverruns=%u retiredMaterialMiB=%.2f",streamingCpuPeakMs,streamingBudgetOverruns,double(retiredMaterials.bytes())/1048576);streamingCpuPeakMs=0;streamingBudgetOverruns=0;}
        if(captureSampled){const auto& p=streamingPhases.peaks;
            logf("WORLD streaming phases peakMs admission=%.3f buffers=%.3f copies=%.3f preload=%.3f materials=%.3f commit=%.3f probes=%.3f static=%.3f",p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7]);streamingPhases.clear();}
        const V pivot=staticPivotReady?staticFramePivot:shadowPivot();
        bool sourceActive[2]={NorthlightGI::dot(sourceColors[0],sourceColors[0])>1e-10f,NorthlightGI::dot(sourceColors[1],sourceColors[1])>1e-10f};
        int firstSource=sourceActive[0]||!sourceActive[1]?0:1;
        NorthlightWorldMath::ShadowFrame cascadeFrames[2][2];
        for(int source=0;source<2;++source)for(int cascade=0;cascade<2;++cascade){
            const float radius=cascade==0?48.f:192.f;
            cascadeFrames[source][cascade]=NorthlightWorldMath::shadowFrame(pivot,sourceDirections[source],radius);
            NorthlightWorldMath::shadowMatrixFrom(cascadeFrames[source][cascade],radius,sourceMatrices[source][cascade]);
        }
        const uint64_t chunkHash=liveChunkHash();
        pointCalculateReplayBounds(); // replay bounds cull both cascades and the point cube
        const unsigned diagnosticCapture=debug==1&&gpuDiagnosticArmed&&gpuDiagnosticCaptures<4?gpuDiagnosticCaptures+1:0;
        if(diagnosticCapture&&gpuDiagnosticDirectory.empty()){
            const std::string parent=root+"graphics-work/renderer/runtime-diagnostics-0.3.96";
            CreateDirectoryA(parent.c_str(),nullptr);
            gpuDiagnosticDirectory=parent+"/session-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount());
            CreateDirectoryA(gpuDiagnosticDirectory.c_str(),nullptr);
            logf("WORLD GPU diagnostic directory=%s",gpuDiagnosticDirectory.c_str());
        }
        d->SetDepthStencilSurface(nullptr);for(int i=1;i<4;++i)d->SetRenderTarget(i,nullptr);
        for(int i=0;i<14;++i)d->SetTexture(i,nullptr);
        const struct{D3DRENDERSTATETYPE s;DWORD v;} states[]={
            {D3DRS_ZENABLE,TRUE},{D3DRS_ZWRITEENABLE,TRUE},{D3DRS_ZFUNC,D3DCMP_LESSEQUAL},{D3DRS_ALPHATESTENABLE,FALSE},{D3DRS_ALPHABLENDENABLE,FALSE},{D3DRS_SEPARATEALPHABLENDENABLE,FALSE},{D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_COLORWRITEENABLE,15},{D3DRS_FOGENABLE,FALSE},{D3DRS_STENCILENABLE,FALSE},{D3DRS_SCISSORTESTENABLE,FALSE},{D3DRS_CLIPPLANEENABLE,0},{D3DRS_SRGBWRITEENABLE,FALSE},{D3DRS_DEPTHBIAS,0},{D3DRS_SLOPESCALEDEPTHBIAS,0},{D3DRS_FILLMODE,D3DFILL_SOLID},{D3DRS_MULTISAMPLEMASK,0xffffffff}};
        for(auto& s:states)d->SetRenderState(s.s,s.v);
        d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);d->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);d->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);d->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);
        unsigned culledBatches[2]={},drawnBatches[2]={};
        size_t replayConstantBytes=0,replayConstantCalls=0,replayPosePrepared=0,replayPoseReused=0;
        for(int source=0;source<2;++source){
          if(!sourceActive[source])continue;
          auto& matrices=sourceMatrices[source];
          for(int cascade=0;cascade<2;++cascade){
            const auto phaseStart=std::chrono::steady_clock::now();
            auto phaseNow=[&](){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-phaseStart).count();};
            double selectMs=0,localMs=0,staticMs=0,dynamicMs=0;
            uint64_t staticDraws=0,staticInstances=0,replayDraws=0,replayTriangles=0;
            unsigned localDraws=0;
            const int slot=source*2+cascade;const float radius=cascade==0?48.f:192.f;
            const auto& now=cascadeFrames[source][cascade];ShadowCacheKey& key=shadowCacheKey[slot];
            long offX=0,offY=0;float dz=0;const char* reason=nullptr;
            if(!key.valid)reason="invalid";
            else if(different(key.direction,sourceDirections[source],1e-6f))reason="direction";
            else if(!NorthlightWorldMath::cacheOffset(now,key.frame,offX,offY,dz))reason="frame";
            else if(std::labs(offX)>long(ShadowCacheMargin)||std::labs(offY)>long(ShadowCacheMargin))reason="travel";
            d->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);d->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
            float cachedMatrix[16];
            NorthlightWorldMath::shadowMatrixFrom(reason?now:key.frame,radius*float(ShadowCacheSize)/1024.f,cachedMatrix);
            if(!reason&&NorthlightLocalShadowSignature::needsRefresh(key.localContentKnown,key.localContent,key.serial,
                bool(uploadedLocalShadowRecords),localShadowSignature(key,cachedMatrix),meshGeneration))reason="local-content";
            if(!reason&&key.staticSignature!=staticSignature(cachedMatrix))reason="static-models";
            selectMs=phaseNow();
            if(reason){
                // Static batches only, over the margin extent, keyed for reuse.
                NorthlightWorldMath::shadowMatrixFrom(now,radius*float(ShadowCacheSize)/1024.f,cachedMatrix);
                d->SetDepthStencilSurface(nullptr);if(!check(d->SetRenderTarget(0,shadowCacheSurface[slot]),"shadow cache target"))return false;
                d->SetDepthStencilSurface(shadowCacheDepth);D3DVIEWPORT9 cvp={0,0,ShadowCacheSize,ShadowCacheSize,0,1};d->SetViewport(&cvp);
                if(!check(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xffffffff,1,0),"clear shadow cache"))return false;
                d->SetVertexDeclaration(shadowDecl);d->SetStreamSource(0,vertices,0,sizeof(NorthlightGI::WorldVertex));for(int i=0;i<4;++i)d->SetStreamSourceFreq(i,1);d->SetIndices(indices);
                d->SetVertexShader(cachedShadowVS);d->SetVertexShaderConstantF(0,cachedMatrix,4);d->SetPixelShader(cachedShadowPS);
                IDirect3DPixelShader9* boundCachePS=cachedShadowPS;
                UINT boundPage=UINT_MAX;
                for(auto& b:batches){
                    if(b.terrain&&!fixedTerrainChunks.count({b.chunkX,b.chunkY}))continue;
                    if(NorthlightShadowBounds::directionalClipReject(b.boundsLow,b.boundsHigh,cachedMatrix)){++culledBatches[cascade];continue;}
                    ++drawnBatches[cascade];
                    const bool interior=NorthlightShadowBounds::depthFullyInside(b.boundsLow,b.boundsHigh,cachedMatrix);
                    const bool opaque=uploadedAlphaCutoffs[b.material]<=0;
                    IDirect3DPixelShader9* ps=interior?(opaque&&cachedOpaqueFastPS?cachedOpaqueFastPS:cachedFastPS):(opaque?cachedOpaquePS:cachedShadowPS);
                    if(!ps)ps=cachedShadowPS;
                    if(ps!=boundCachePS){if(!check(d->SetPixelShader(ps),"static cache depth variant"))return false;boundCachePS=ps;}
                    float material[]={1,1,1,uploadedAlphaCutoffs[b.material]};d->SetPixelShaderConstantF(0,material,1);d->SetTexture(0,materials[b.material]);
                    if(!bindMeshPage(b,boundPage)||!check(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,b.minVertex,b.vertexCount,b.start,b.count),"static shadow cache draw"))return false;
                }
                localDraws=drawnBatches[cascade];localMs=phaseNow()-selectMs;
                const double staticStart=phaseNow();
                const bool staticOK=drawStaticCasters(cachedMatrix);
                staticMs=phaseNow()-staticStart;staticDraws=staticCasters.stats().drawCalls;staticInstances=staticCasters.stats().instances;
                key.valid=staticOK;key.frame=now;key.direction=sourceDirections[source];key.serial=meshGeneration;key.chunkHash=chunkHash;key.staticSignature=staticSignature(cachedMatrix);key.localContent=localShadowSignature(key,cachedMatrix);key.localContentKnown=bool(uploadedLocalShadowRecords);offX=offY=0;dz=0;++shadowCacheRenders;
                DWORD tick=GetTickCount();if(!shadowCacheLogAt||tick-shadowCacheLogAt>=1000){shadowCacheLogAt=tick;logf("WORLD shadow cache rerender source=%d cascade=%d reason=%s",source,cascade,reason);}
            } else {key.serial=meshGeneration;++shadowCacheReuses;}
            if(diagnosticCapture){
                char name[80];std::snprintf(name,sizeof name,"s%d-c%d-static",source,cascade);
                NorthlightWorldDiagnostics::dump(d,shadowCacheSurface[slot],gpuDiagnosticDirectory,name,diagnosticCapture);
                NorthlightWorldDiagnostics::shadowFrame(gpuDiagnosticDirectory,diagnosticCapture,source,cascade,cachedMatrix,matrices[cascade],offX,offY,dz,key.serial,key.staticSignature,reason);
            }
            const double dynamicStart=phaseNow();
            // Dynamic part of this frame: live terrain and replays into the scratch map.
            d->SetDepthStencilSurface(nullptr);if(!check(d->SetRenderTarget(0,shadowScratchSurface),"shadow scratch target"))return false;
            d->SetDepthStencilSurface(shadowDepth);D3DVIEWPORT9 vp={0,0,1024,1024,0,1};d->SetViewport(&vp);
            if(!check(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xffffffff,1,0),"clear shadow"))return false;
            d->SetVertexDeclaration(shadowDecl);d->SetStreamSource(0,vertices,0,sizeof(NorthlightGI::WorldVertex));for(int i=0;i<4;++i)d->SetStreamSourceFreq(i,1);d->SetIndices(indices);
            d->SetVertexShader(shadowVS);d->SetVertexShaderConstantF(0,matrices[cascade],4);d->SetPixelShader(shadowPS);
            // Cached-mesh terrain chunks that the game did not draw live this frame.
            UINT terrainBoundPage=UINT_MAX;
            for(auto& b:batches){if(!b.terrain||fixedTerrainChunks.count({b.chunkX,b.chunkY})||liveTerrainChunks.count({b.chunkX,b.chunkY}))continue;if(NorthlightShadowBounds::clipReject(b.boundsLow,b.boundsHigh,matrices[cascade])){++culledBatches[cascade];continue;}++drawnBatches[cascade];float material[]={1,1,1,uploadedAlphaCutoffs[b.material]};d->SetPixelShaderConstantF(0,material,1);d->SetTexture(0,materials[b.material]);if(!bindMeshPage(b,terrainBoundPage)||!check(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,b.minVertex,b.vertexCount,b.start,b.count),"static shadow cache draw"))return false;}
            if(!liveDirectionalIndices.empty()){
                float opaque[]={1,1,1,-1};d->SetPixelShaderConstantF(0,opaque,1);d->SetTexture(0,nullptr);
                d->SetStreamSource(0,liveTerrainGPU.vertices(),0,sizeof(NorthlightGI::WorldVertex));d->SetIndices(liveIndicesGPU);
                if(!check(d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,liveTerrainGPU.vertexCapacity(),UINT(liveTerrainIndices.size()),UINT(liveDirectionalIndices.size()/3)),"live terrain shadow"))return false;
            }
            // Original model transforms and bone palette survive; replace view->clip only.
            if(diagnosticCapture){
                char name[80];std::snprintf(name,sizeof name,"s%d-c%d-terrain",source,cascade);
                NorthlightWorldDiagnostics::dump(d,shadowScratchSurface,gpuDiagnosticDirectory,name,diagnosticCapture);
            }
            float rows[16];NorthlightWorldMath::replayProjection(context.inverseView,matrices[cascade],rows);
            d->SetPixelShader(replayPS);
            // Fresh banks after the static pass. Fold the shadow projection into
            // desired constants first; compare only the shader's captured ranges.
            NorthlightReplayPoses::Pass<BOOL> poseConstants;
            NorthlightReplayDrawState::Cache replayBindings(d);
            for(auto& p:replays){
                // Only full geometric proof plus cached-volume containment can
                // replace a live rigid draw. Dynamic/alpha/unknown draws survive.
                if(key.valid&&(p->staticProofMask&(1u<<slot))&&staticCasters.stats().readyModels&&
                   StaticShadow::containsBounds(cachedMatrix,p->staticProofLow,p->staticProofHigh)){
                    ++staticDedupSkipped;continue;
                }
                if(p->pointBounds.valid&&NorthlightShadowBounds::clipReject(V(p->pointBounds.low[0],p->pointBounds.low[1],p->pointBounds.low[2]),V(p->pointBounds.high[0],p->pointBounds.high[1],p->pointBounds.high[2]),matrices[cascade])){++culledReplayDraws;continue;}
                if(!check(replayBindings.geometry(*p),"replay geometry state"))return false;
                if(!poseConstants.prepare(*p,rows))return false;
                const float* desired=poseConstants.desired();
                auto f=poseConstants.floats;auto b=poseConstants.booleans;auto i=poseConstants.integers;
                if(f.count){if(!check(d->SetVertexShaderConstantF(f.first,desired+4*f.first,f.count),"replay float constants"))return false;replayConstantBytes+=f.count*16;++replayConstantCalls;}
                if(b.count){if(!check(d->SetVertexShaderConstantB(b.first,p->bools+b.first,b.count),"replay bool constants"))return false;replayConstantBytes+=b.count*4;++replayConstantCalls;}
                if(i.count){if(!check(d->SetVertexShaderConstantI(i.first,p->ints+4*i.first,i.count),"replay integer constants"))return false;replayConstantBytes+=i.count*16;++replayConstantCalls;}
                if(!check(replayBindings.material(*p),"replay material state"))return false;
                HRESULT hr=p->indexed?d->DrawIndexedPrimitive(p->type,p->base,p->min,p->vertices,p->start,p->count):d->DrawPrimitive(p->type,p->start,p->count);if(!check(hr,"animated shadow draw"))return false;
                ++replayDraws;replayTriangles+=p->count;
            }
            replayPosePrepared+=poseConstants.prepared;replayPoseReused+=poseConstants.reused;
            // Union: min(dynamic scratch, cached static at its integer texel offset + exact depth delta).
            if(diagnosticCapture){
                char name[80];std::snprintf(name,sizeof name,"s%d-c%d-live",source,cascade);
                NorthlightWorldDiagnostics::dump(d,shadowScratchSurface,gpuDiagnosticDirectory,name,diagnosticCapture);
            }
            d->SetDepthStencilSurface(nullptr);if(!check(d->SetRenderTarget(0,shadowSurface[slot]),"shadow target"))return false;
            d->SetRenderState(D3DRS_ZENABLE,FALSE);d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
            d->SetVertexShader(nullptr);d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);d->SetStreamSourceFreq(0,1);
            d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
            d->SetTexture(0,shadowScratch);d->SetTexture(1,shadowCache[slot]);
            for(unsigned sampler=0;sampler<2;++sampler){d->SetSamplerState(sampler,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);d->SetSamplerState(sampler,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);d->SetSamplerState(sampler,D3DSAMP_MINFILTER,D3DTEXF_POINT);d->SetSamplerState(sampler,D3DSAMP_MAGFILTER,D3DTEXF_POINT);d->SetSamplerState(sampler,D3DSAMP_MIPFILTER,D3DTEXF_NONE);d->SetSamplerState(sampler,D3DSAMP_SRGBTEXTURE,FALSE);}
            float unionConstants[2][4]={{float(long(ShadowCacheMargin)+offX),float(long(ShadowCacheMargin)-offY),dz,float(ShadowCacheSize)},{1.f/1024,1.f/float(ShadowCacheSize),0,0}};
            d->SetPixelShaderConstantF(0,&unionConstants[0][0],2);d->SetPixelShader(unionPS);
            if(!check(quad(1024,1024),"shadow union"))return false;
            d->SetTexture(0,nullptr);d->SetTexture(1,nullptr);
            d->SetRenderState(D3DRS_ZENABLE,TRUE);d->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
            d->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);d->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
            if(profile)profile->mark(source==0?(cascade==0?"SunNear":"SunFar"):(cascade==0?"MoonNear":"MoonFar"));
            dynamicMs=phaseNow()-dynamicStart;
            const double phaseMs=phaseNow();const DWORD phaseTick=GetTickCount();
            if(captureSampled||(phaseMs>4.0&&(!shadowPhaseLogAt||phaseTick-shadowPhaseLogAt>=1000))){
                shadowPhaseLogAt=phaseTick;
                logf("WORLD shadow phase gpuFrame=%llu worldFrame=%u source=%d cascade=%d cache=%s cpuMs=%.3f selectMs=%.3f localMs=%.3f staticMs=%.3f dynamicMs=%.3f localDraws=%u staticDraws=%llu staticInstances=%llu replayDraws=%llu replayTriangles=%llu",
                    (unsigned long long)(profile?profile->sampledFrame():0),frames,source,cascade,reason?reason:"reuse",phaseMs,selectMs,localMs,staticMs,dynamicMs,localDraws,
                    (unsigned long long)staticDraws,(unsigned long long)staticInstances,(unsigned long long)replayDraws,(unsigned long long)replayTriangles);
            }
        }
        }
        shadowFrameReady=true;
        // The local-lamp shadow correction subtracts an estimate of the lamp's
        // baked light where the lamp is occluded. By day that darkens sunlit
        // ground next to lamp posts (and costs ~6 ms), so it runs only while the
        // sun's weight is below one half (dusk, night, dawn).
        if(debug==0&&sourceWeights[0]<.5f)renderPointShadow();else pointReady=false;
        if(profile)profile->mark("PointShadow");
        d->SetDepthStencilSurface(nullptr);d->SetTexture(0,nullptr);
        if(!check(d->StretchRect(targetSurface,nullptr,colorSurface,nullptr,D3DTEXF_NONE),"world color copy"))return false;
        d->SetVertexShader(nullptr);d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);d->SetStreamSourceFreq(0,1);d->SetRenderState(D3DRS_ZENABLE,FALSE);d->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
        d->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);d->SetRenderState(D3DRS_WRAP0,0);
        float c[68][4]={};c[0][0]=1.f/w;c[0][1]=1.f/h;c[0][2]=nearZ;c[0][3]=farZ;
        c[67][0]=celestialValid?std::max(celestial.sun.angularRadius,celestial.moon.angularRadius):.03f;c[67][1]=sourceVisValid?.85f:0.f;
        // Temporal history is valid only for the same map, a continuous camera
        // and normal rendering; teleports, F10 and diagnostics restart it.
        const bool useHistory=temporalValid&&temporalMap==active->map&&!different(vec(context.camera),previousCamera,40)&&debug==0;
        memcpy(c[53],previousView,64);c[57][0]=useHistory?.75f:0.f;
        // Up to 16 authored lamps, sent through existing SM3-safe 8-light
        // direct / 4-light fog batches. Never expand the shader register bank.
        const float lampNight=NorthlightRegionalFog::nightFactor(celestialValid?celestial.dayFraction:-1.f);
        const float lampGain=.9f*NorthlightLocalLightSelection::nightGain(lampNight);
        auto localLights=active?NorthlightLocalLightSelection::select(active->localLights,context.camera):NorthlightLocalLightSelection::Selection{};
        localDirectCount=localLights.count;localDirectNearest=localLights.nearest;
        c[52][0]=float(std::min(localDirectCount,NorthlightLocalLightSelection::DirectBatchSize));c[52][1]=.9f*lampGain;
        // Near fade: no added fog within 3.5 units of the viewer, full at 15.5.
        // Same soft ramp, shifted 0.5 world units closer.
        c[58][0]=3.5f;c[58][1]=1.f/12;c[58][2]=13.f*lampGain;c[58][3]=.156f*lampGain;
        if(uploadedFogField){c[31][0]=uploadedFogField->originX;c[31][1]=uploadedFogField->originY;}
        c[31][2]=1.f/(NorthlightRegionalFog::N*NorthlightRegionalFog::Spacing);
        c[31][3]=NorthlightRegionalFog::nightFactor(celestialValid?celestial.dayFraction:-1.f);
        // Neutral scattering albedo preserves the zone/source palette. Atmospheric
        // style is separate from ground fog and surface irradiance.
        c[22][0]=c[22][1]=c[22][2]=1;c[22][3]=.0035f+.0031f*c[31][3]; // generic forest day .0035, night .0066 (+20%)
        // Forest atmosphere follows verified render time, independently of source
        // visibility. Smooth nightFactor avoids a camera-driven density switch.
        // STV total air including the shared .0017: .0054 day (2x),
        // .01245 night (1.5x). Duskwood and general forest air retain their density.
        c[32][0]=.0055f+.0089f*c[31][3];c[32][1]=.0037f+.00705f*c[31][3];
        const auto& volumePalette=paletteFrameValid?framePalette:celestialProfiles.fallback;
        const float volumeHeightScale=NorthlightCelestialProfiles::fogHeightScale(volumePalette,c[31][3]);
        c[32][2]=64.f*volumeHeightScale;c[32][3]=48.f*volumeHeightScale;
        // Extinction at each picked light for its glow in the fog: same ground
        // and air policy as the shader, evaluated at the light's own position.
        if(active&&uploadedFogField){
            const auto& field=*uploadedFogField;const float night=c[31][3];
            for(unsigned i=0;i<localDirectCount;++i){
                const float lx=localLights.position[i][0],ly=localLights.position[i][1],lz=localLights.position[i][2];
                const float fx=(lx-field.originX)/NorthlightRegionalFog::Spacing,fy=(ly-field.originY)/NorthlightRegionalFog::Spacing;
                float sigma=0;
                if(fx>=0&&fy>=0&&fx<NorthlightRegionalFog::N-1&&fy<NorthlightRegionalFog::N-1){
                    const auto& t=field.texels[unsigned(fy)*NorthlightRegionalFog::N+unsigned(fx)];
                    if(t.height>0){
                        const float altitude=lz-t.ground;
                        const float profile=std::clamp((t.height-2.5f)/2.5f,0.f,1.f),generalForest=1-std::clamp((t.height-1.25f)/1.25f,0.f,1.f);
                        const float groundHeight=t.height+(6-t.height)*night*(1-profile);
                        const float vertical=std::clamp(1-altitude/std::max(groundHeight,.001f),0.f,1.f);
                        const float ground=altitude>=0?std::max(t.day+t.nightExtra*night,0.f)*vertical*vertical:0;
                        float airBase=c[32][1]+(c[32][0]-c[32][1])*profile;airBase=airBase+(c[22][3]-airBase)*generalForest;
                        airBase=.0017f+airBase*std::clamp((t.height-.625f)/.625f,0.f,1.f);
                        const float airHeight=c[32][3]+(c[32][2]-c[32][3])*profile;
                        const float airVertical=std::clamp(1-altitude/std::max(airHeight,.001f),0.f,1.f);
                        sigma=altitude>=0?ground+airBase*airVertical*airVertical:0;
                    }
                }
                localLights.fog[i][0]=sigma;
            }
        }
        c[33][0]=float(w);c[33][1]=float(h);c[33][2]=float(w/2);c[33][3]=float(h/2);
        c[1][0]=projection[0];c[1][1]=projection[1];c[1][2]=projection[2];c[1][3]=minZ;
        c[2][0]=1.f/(maxZ-minZ);c[2][1]=48;c[2][2]=192;c[2][3]=1.f/1024;
        memcpy(c[3],context.inverseView,64);memcpy(c[7],sourceMatrices[firstSource][0],64);memcpy(c[11],sourceMatrices[firstSource][1],64);
        memcpy(c[15],context.camera,12);memcpy(c[16],context.lightDirection,12);memcpy(c[17],context.direct,12);memcpy(c[18],context.ambient,12);
        c[18][3]=NorthlightTwilightFill::gain(celestialValid,celestialValid?celestial.dayFraction:-1.,
            celestialValid?celestialLight.sun.direction[2]:0.f,celestialValid?celestialLight.moon.direction[2]:0.f);
        c[19][0]=active->origin.x;c[19][1]=active->origin.y;c[19][2]=active->origin.z;c[19][3]=8;
        c[20][0]=float(NorthlightGI::ProbeAtlasN);c[20][1]=.5f;c[20][2]=.85f;c[20][3]=active->serial?1.f:0.f;
        DWORD now=GetTickCount();
        c[21][0]=uploadedFogField&&(uploadedFogField->fogCells||uploadedFogField->airCells)?1.f:0.f;c[21][1]=1.2f;c[21][2]=.38f;c[21][3]=128;
        memcpy(c[23],context.camera,12);c[24][0]=NorthlightWorldMath::ShadowBiasWorld*NorthlightWorldMath::InverseShadowDepth;c[24][1]=2;c[24][2]=float(debug);c[24][3]=float(DWORD(now-animationEpoch))*.001f;
        memcpy(c[25],legacyFog.parameters,16);memcpy(c[26],legacyFog.color,16);
        memcpy(c[28],context.lightDirection,12);memcpy(c[29],context.direct,12);
        c[27][2]=float(DWORD(now-animationEpoch))*.001f;
        c[27][3]=c[21][0];
        c[35][0]=NorthlightWorldMath::InverseShadowDepth;
        c[30][0]=waterMask?1.f:0.f;d->SetPixelShaderConstantF(0,&c[0][0],68);
        IDirect3DTexture9* textures[]={color,depth,shadow[0],shadow[1],probe[0],probe[1],probe[2],probe[3],nullptr,nullptr,probe[4],waterMask,nullptr,regionalFogTexture};
        for(int i=0;i<14;++i){d->SetTexture(i,textures[i]);d->SetSamplerState(i,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);d->SetSamplerState(i,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);d->SetSamplerState(i,D3DSAMP_MINFILTER,(i==0||i==9)?D3DTEXF_LINEAR:D3DTEXF_POINT);d->SetSamplerState(i,D3DSAMP_MAGFILTER,(i==0||i==9)?D3DTEXF_LINEAR:D3DTEXF_POINT);d->SetSamplerState(i,D3DSAMP_MIPFILTER,D3DTEXF_NONE);d->SetSamplerState(i,D3DSAMP_SRGBTEXTURE,FALSE);}
        auto setSource=[&](int source,bool first,bool volume=false){
            d->SetTexture(2,shadow[source*2]);d->SetTexture(3,shadow[source*2+1]);
            d->SetPixelShaderConstantF(7,sourceMatrices[source][0],4);d->SetPixelShaderConstantF(11,sourceMatrices[source][1],4);
            float dir[4]={sourceDirections[source].x,sourceDirections[source].y,sourceDirections[source].z,source==1?1.f:0.f}; // w selects moon surface relighting
            // Match the shadow-render activity threshold exactly. The mandatory
            // ambient-only volume pass must never read an unrendered map for
            // a sub-threshold celestial source.
            float rgb[4]={sourceActive[source]?sourceColors[source].x:0.f,sourceActive[source]?sourceColors[source].y:0.f,sourceActive[source]?sourceColors[source].z:0.f,0};
            d->SetPixelShaderConstantF(16,dir,1);d->SetPixelShaderConstantF(17,rgb,1);
            if(volume){c[21][1]=1.2f*volumePalette.fogGain[source];c[21][2]=source==0?.38f:.24f;d->SetPixelShaderConstantF(21,c[21],1);d->SetTexture(15,sourceVis[source][1-sourceVisIndex]);}
            c[27][0]=first?(volume?1.f:1.f-authoredFill):0.f;c[27][1]=sourceWeights[source];d->SetPixelShaderConstantF(27,c[27],1);
        };
        d->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);d->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);d->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
        // Smoothed normals for the lighting, GI and local light passes (s14).
        d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);d->SetRenderState(D3DRS_COLORWRITEENABLE,15);
        d->SetRenderTarget(0,normalSurface);d->SetPixelShader(normalsPS);if(!check(quad(w/2,h/2),"world normals pass"))return false;
        d->SetTexture(14,normalBuffer);d->SetSamplerState(14,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);d->SetSamplerState(14,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);d->SetSamplerState(14,D3DSAMP_MINFILTER,D3DTEXF_POINT);d->SetSamplerState(14,D3DSAMP_MAGFILTER,D3DTEXF_POINT);d->SetSamplerState(14,D3DSAMP_MIPFILTER,D3DTEXF_NONE);d->SetSamplerState(14,D3DSAMP_SRGBTEXTURE,FALSE);
        d->SetRenderTarget(0,lightSurface);d->SetPixelShader(lightingPS);
        bool first=true;
        // A strength-zero source still subtracts its authored surface share.
        for(int source=0;source<2;++source){if(sourceWeights[source]<=0&&source!=firstSource)continue;
            setSource(source,first);d->SetRenderState(D3DRS_ALPHABLENDENABLE,!first);
            if(!check(d->SetRenderTarget(1,first?baselineSurface:nullptr),"baseline MRT"))return false;
            d->SetRenderState(D3DRS_COLORWRITEENABLE1,15);
            if(!check(quad(w/2,h/2),"celestial shadows pass"))return false;first=false;
        }
        d->SetRenderTarget(1,nullptr);
        d->SetRenderState(D3DRS_COLORWRITEENABLE,15);
        if(profile)profile->mark("WorldLighting");
        d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);d->SetRenderState(D3DRS_COLORWRITEENABLE,7);d->SetPixelShader(giPS);
        if(!check(quad(w/2,h/2),"world GI pass"))return false;
        if(localDirectCount&&debug==0){
            d->SetPixelShader(localDirectPS);
            for(unsigned firstLight=0;firstLight<localDirectCount;firstLight+=NorthlightLocalLightSelection::DirectBatchSize){
                const auto batch=localLights.batch<NorthlightLocalLightSelection::DirectBatchSize>(firstLight);
                const float info[4]={float(batch.count),c[52][1],0,0};
                d->SetPixelShaderConstantF(36,batch.position[0].data(),NorthlightLocalLightSelection::DirectBatchSize);
                d->SetPixelShaderConstantF(44,batch.color[0].data(),NorthlightLocalLightSelection::DirectBatchSize);
                d->SetPixelShaderConstantF(52,info,1);
                if(!check(quad(w/2,h/2),"local direct light batch"))return false;
            }
        }
        if(profile)profile->mark("GI");d->SetRenderState(D3DRS_COLORWRITEENABLE,15);
        if(debug==0&&pointReady){
            renderPointLighting(depth,waterMask,w,h,nearZ,farZ,minZ,maxZ);
            // Optional pass temporarily owns PS c0..9 and s0..2. Restore the
            // entire world bank and texture/filter contract before fog/final.
            d->SetPixelShaderConstantF(0,&c[0][0],68);
            for(unsigned sampler=0;sampler<3;++sampler){
                d->SetTexture(sampler,textures[sampler]);
                d->SetSamplerState(sampler,D3DSAMP_MINFILTER,sampler==0?D3DTEXF_LINEAR:D3DTEXF_POINT);
                d->SetSamplerState(sampler,D3DSAMP_MAGFILTER,sampler==0?D3DTEXF_LINEAR:D3DTEXF_POINT);
            }
            d->SetRenderState(D3DRS_COLORWRITEENABLE,15);
        }
        {   // Temporal stabilization: light + history -> temporalLight[cur] (MRT with view distance).
            const unsigned cur=temporalIndex,prev=1-temporalIndex;
            d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);d->SetRenderState(D3DRS_COLORWRITEENABLE,15);
            if(!check(d->SetRenderTarget(0,temporalLightSurface[cur]),"temporal light target")||!check(d->SetRenderTarget(1,temporalDepthSurface[cur]),"temporal depth target"))return false;
            d->SetRenderState(D3DRS_COLORWRITEENABLE1,15);
            d->SetTexture(8,light);d->SetTexture(14,temporalLight[prev]);d->SetTexture(15,temporalDepth[prev]);
            for(unsigned sampler=14;sampler<16;++sampler){d->SetSamplerState(sampler,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);d->SetSamplerState(sampler,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);d->SetSamplerState(sampler,D3DSAMP_MINFILTER,D3DTEXF_POINT);d->SetSamplerState(sampler,D3DSAMP_MAGFILTER,D3DTEXF_POINT);d->SetSamplerState(sampler,D3DSAMP_MIPFILTER,D3DTEXF_NONE);d->SetSamplerState(sampler,D3DSAMP_SRGBTEXTURE,FALSE);}
            d->SetPixelShader(temporalPS);if(!check(quad(w/2,h/2),"temporal light pass"))return false;
            d->SetRenderTarget(1,nullptr);d->SetTexture(14,nullptr);d->SetTexture(15,nullptr);
            temporalIndex=prev;temporalValid=true;memcpy(previousView,context.view,64);previousCamera=vec(context.camera);temporalMap=active->map;
        }
        if(profile)profile->mark("PointLighting");
        // Source visibility (1x1 per source, temporally smoothed): drives the
        // fog aureole when the body is behind geometry; broad scattering uses
        // world shadow visibility independently of the body's screen position.
        {const unsigned cur=sourceVisIndex,prev=1-sourceVisIndex;
         d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);d->SetRenderState(D3DRS_COLORWRITEENABLE,15);d->SetPixelShader(sourceVisPS);
         for(int source=0;source<2;++source){if(!sourceActive[source]&&source!=firstSource)continue;
            float dir[4]={sourceDirections[source].x,sourceDirections[source].y,sourceDirections[source].z,0};d->SetPixelShaderConstantF(16,dir,1);
            d->SetRenderTarget(0,sourceVisSurface[source][cur]);d->SetTexture(15,sourceVis[source][prev]);
            d->SetSamplerState(15,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);d->SetSamplerState(15,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);d->SetSamplerState(15,D3DSAMP_MINFILTER,D3DTEXF_POINT);d->SetSamplerState(15,D3DSAMP_MAGFILTER,D3DTEXF_POINT);d->SetSamplerState(15,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
            if(!check(quad(1,1),"source visibility pass"))return false;}
         sourceVisIndex=prev;sourceVisValid=true;}
        d->SetRenderTarget(0,fogSurface);d->SetPixelShader(fogPS);first=true;
        for(int source=0;source<2;++source){if(!sourceActive[source]&&source!=firstSource)continue;
            setSource(source,first,true);d->SetRenderState(D3DRS_ALPHABLENDENABLE,!first);d->SetRenderState(D3DRS_COLORWRITEENABLE,first?15:7);
            if(!check(quad(w/2,h/2),"celestial volumetric raymarch"))return false;first=false;
        }
        if(localDirectCount&&debug==0){
            d->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);d->SetRenderState(D3DRS_COLORWRITEENABLE,7);d->SetPixelShader(localFogPS);
            // Analytic lamp glow can reach farther without extending the
            // celestial raymarch or adding samples to it. Restore after use.
            const float lampFogInfo[4]={c[21][0],c[21][1],c[21][2],localLights.fogDistance};
            d->SetPixelShaderConstantF(21,lampFogInfo,1);
            for(unsigned firstLight=0;firstLight<localDirectCount;firstLight+=NorthlightLocalLightSelection::FogBatchSize){
                const auto batch=localLights.batch<NorthlightLocalLightSelection::FogBatchSize>(firstLight);
                d->SetPixelShaderConstantF(36,batch.position[0].data(),NorthlightLocalLightSelection::FogBatchSize);
                d->SetPixelShaderConstantF(44,batch.color[0].data(),NorthlightLocalLightSelection::FogBatchSize);
                d->SetPixelShaderConstantF(59,batch.fog[0].data(),NorthlightLocalLightSelection::FogBatchSize);
                if(!check(quad(w/2,h/2),"local fog glow batch"))return false;
            }
            d->SetPixelShaderConstantF(21,c[21],1);
        }
        if(profile)profile->mark("Fog");d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);d->SetRenderState(D3DRS_COLORWRITEENABLE,15);
        // Separable depth-aware blur: fog -> fogBlurred (horizontal) -> fog (vertical).
        {float blur[4]={2,0,0,0};d->SetPixelShader(fogBlurPS);
         d->SetRenderTarget(0,fogBlurredSurface);d->SetTexture(9,fog);d->SetPixelShaderConstantF(34,blur,1);if(!check(quad(w/2,h/2),"volume blur horizontal"))return false;
         blur[0]=0;blur[1]=2;d->SetRenderTarget(0,fogSurface);d->SetTexture(9,fogBlurred);d->SetPixelShaderConstantF(34,blur,1);if(!check(quad(w/2,h/2),"volume blur vertical"))return false;}
        d->SetRenderTarget(0,targetSurface);d->SetTexture(8,temporalLight[1-temporalIndex]);d->SetTexture(9,fog);d->SetTexture(12,baselineLight);d->SetPixelShader(finalPS);if(!check(quad(w,h),"world composite"))return false;if(profile)profile->mark("WorldComposite");
        if(diagnosticCapture){
            gpuDiagnosticArmed=false;unsigned capture=++gpuDiagnosticCaptures;
            const std::string& directory=gpuDiagnosticDirectory;
            float actual[68][4]={};HRESULT read=d->GetPixelShaderConstantF(0,actual[0],68);
            if(SUCCEEDED(read)){
                char suffix[80];std::snprintf(suffix,sizeof suffix,"/capture-%u-constants.f32",capture);
                if(FILE* f=std::fopen((directory+suffix).c_str(),"wb")){std::fwrite(actual,sizeof actual,1,f);std::fclose(f);}
            }
            logf("WORLD GPU diagnostic capture=%u map=%s eye=(%.3f %.3f %.3f) moon=(%.6f %.6f %.6f) constantsHRESULT=%08lx",capture,active->map.c_str(),context.camera[0],context.camera[1],context.camera[2],sourceDirections[1].x,sourceDirections[1].y,sourceDirections[1].z,(unsigned long)read);
            using NorthlightWorldDiagnostics::dump;
            dump(d,shadowSurface[firstSource*2],directory,"shadow-near",capture);
            dump(d,shadowSurface[firstSource*2+1],directory,"shadow-far",capture);
            dump(d,normalSurface,directory,"normals",capture);
            dump(d,baselineSurface,directory,"baseline",capture);
            dump(d,lightSurface,directory,"light",capture);
            dump(d,temporalLightSurface[1-temporalIndex],directory,"temporal-light",capture);
            dump(d,temporalDepthSurface[1-temporalIndex],directory,"distance",capture);
            dump(d,fogSurface,directory,"fog",capture);
            dump(d,colorSurface,directory,"scene",capture);
        }
        if(++frames==1||frames%600==0)logf("WORLD frame=%u GI probes=%u valid=%u rays=64 bounces=3 cacheTriangles=%zu replayDraws=%zu liveTerrainChunks=%zu terrainAttempts=%u terrainSnapshots=%u cascadesPerSource=2x1024 volumeStepsMax=49 fogWorldSpacing=2.667 fogShadowFilter=1.5 fogLightHeight=12 fogAmbient=0.35",frames,unsigned(NorthlightGI::ProbeGridCount),active->validProbes,active->bvh->triangleCount(),replays.size(),liveTerrainChunks.size(),terrainAttempts,terrainSnapshots);
        if(captureSampled)logf("VOLUME sources sunRGB=%.5f,%.5f,%.5f moonRGB=%.5f,%.5f,%.5f ambientRGB=%.5f,%.5f,%.5f sunGain=1.2 moonGain=1.2 directCaps=0.38,0.24 airCells=%u airDensity=%.5f,%.5f forestAir=%.4f night=%.3f",
            sourceColors[0].x,sourceColors[0].y,sourceColors[0].z,sourceColors[1].x,sourceColors[1].y,sourceColors[1].z,
            context.ambient[0],context.ambient[1],context.ambient[2],uploadedFogField?uploadedFogField->airCells:0,c[32][0],c[32][1],c[22][3],c[31][3]);
        if(frames==1||frames%600==0)logf("LOCAL direct lights=%u limit=%u nearestReach=%.1f visibilityEnd=%.1f fogDistance=%.1f strength=%.3f lampGain=%.3f available=%zu",localDirectCount,NorthlightLocalLightSelection::Limit,localDirectNearest,NorthlightLocalLightSelection::VisibilityEnd,localLights.fogDistance,c[52][1],lampGain,active?active->localLights.size():size_t(0));
        if(frames==1||frames%600==0)logf("CELESTIAL valid=%d gameDay=%.6f source0Weight=%.4f moonWeight=%.4f authoredFill=%.4f sunZ=%.4f moonZ=%.4f regionalFog=%.4f",celestialValid,celestialValid?celestial.dayFraction:0.f,sourceWeights[0],sourceWeights[1],authoredFill,sourceDirections[0].z,sourceDirections[1].z,c[27][3]);
        if(frames==1||frames%600==0){logf("WORLD shadow batches near=%u far=%u culledNear=%u culledFar=%u liveUploadBytes=%zu cacheRenders=%u cacheReuses=%u culledReplays=%u",drawnBatches[0],drawnBatches[1],culledBatches[0],culledBatches[1],terrainUploadBytes,shadowCacheRenders,shadowCacheReuses,culledReplayDraws);shadowCacheRenders=shadowCacheReuses=culledReplayDraws=0;}
        if(captureSampled)logf("WORLD terrain reuse=%u snapshots=%zu uploadBytes=%zu vertexUploadBytes=%zu reusedVertices=%zu arenaMiB=%.2f rollovers=%u",unsigned(terrainUploadReused),frameTerrain.size(),terrainUploadBytes,liveTerrainGPU.uploadedBytes,liveTerrainGPU.reusedVertices,double(liveTerrainGPU.bytes())/1048576,liveTerrainGPU.rollovers);
        if(frames==1||frames%600==0)logf("WORLD replay constants bytes=%zu previousBytes=%zu calls=%zu previousCalls=%zu",replayConstantBytes,replays.size()*2*(1024*4+16*4+64*4+16*4),replayConstantCalls,replays.size()*2*4);
        if(frames==1||frames%600==0)logf("WORLD replay poses draws=%zu groups=%u prepared=%zu reused=%zu scope=directional exact=1",replays.size(),replays.empty()?0:replays.back()->constantGroup+1,replayPosePrepared,replayPoseReused);
        DWORD elapsed=GetTickCount()-submissionStart;
        if(elapsed>40&&slowReports++<12)logf("WORLD slow submission: %lu ms; replay=%zu liveTerrainChunks=%zu",(unsigned long)elapsed,replays.size(),liveTerrainChunks.size());
        shadowsComposited=(sourceActive[0]||sourceActive[1])&&debug==0;
        return true;
    }
};
