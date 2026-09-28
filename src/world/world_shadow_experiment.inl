    // Selection runs on capture frames only; with model capture skipped between them
    // (both shadow intervals >1) its frame counts are rescaled to rendered frames.
    NorthlightActorShadowSelection::Tuning selectionTuning()const{
        return NorthlightActorShadowSelection::atCadence(NorthlightActorShadowSelection::active(),
            effects.shadows&&NorthlightQuality::captureSkipPossible(quality,true)?quality.nearShadowInterval:1);}
    void selectShadowReplays(){
        if(shadowSelectionDone)return;shadowSelectionDone=true;
        auto finishFate=[this]{finishShadowFate();};struct FateGuard {decltype(finishFate)& finish;~FateGuard(){finish();}} fateGuard{finishFate};
        const auto start=captureSampled?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
        sampledVertices.beginFrame();
        const size_t captured=replays.size(),budget=size_t(actorShadowBudgetMiB)*1048576;
        size_t actorBytes=0,actors=0,small=0,distanceTests=0,distanceReused=0;
        for(const auto& p:replays){small+=!p->shadowSelected;
            if(p->shadowSelected&&p->shadowSkinned){actorBytes+=p->mesh().byteSize();++actors;}}
        NorthlightReplayShadowPolicy::Result result;result.kept=actors;result.keptBytes=actorBytes;NorthlightActorShadowSelection::Result stable;bool ranked=false;
        auto transition=[this](bool over){actorShadowTransitions+=over!=actorShadowOver;actorShadowOver=over;}; /* over<->under budget crossings per log window */
        const bool radius=NorthlightActorShadowSelection::Enabled&&quality.actorShadowRadius;
        if(!radius)transition(budget&&actorBytes>budget);
        try{
            // Ranking is necessary only if the captured actor geometry exceeds
            // this optional replay quota. Capture safety still uses Frame's
            // independent byte/draw limits, including on immutable cache hits.
            // ActorShadowRadius>0 runs the stable selection on every frame (grouping,
            // attachments, radius identities); the quota then ranks, as before, but on
            // the bytes inside the radius only (choose() decides with shouldRank).
            if(radius){stable=selectStableActors(budget,distanceTests,distanceReused);ranked=stable.ranked;actorShadowRadiusToggles+=stable.radiusToggles;actorShadowRadiusFlicker+=stable.radiusFlicker;actorShadowRadiusRekeyed+=stable.radiusRekeyed;
                transition(budget&&stable.radiusInsideBytes>budget);
                if(ranked){actorShadowToggles+=stable.toggles;++actorShadowFrames;actorShadowCapBinds+=unsigned(stable.exemptCapBinds);}}
            else if((ranked=NorthlightActorShadowSelection::Enabled&&budget&&actorShadowHistory.shouldRank(actorBytes,budget,selectionTuning()))){stable=selectStableActors(budget,distanceTests,distanceReused);actorShadowToggles+=stable.toggles;++actorShadowFrames;actorShadowCapBinds+=unsigned(stable.exemptCapBinds);}
            else if(budget&&actorBytes>budget){
                shadowCandidates.clear();shadowCandidates.reserve(actors);
                unsigned previousGroup=UINT_MAX;IDirect3DVertexShader9* previousShader=nullptr;
                IDirect3DVertexDeclaration9* previousDecl=nullptr;float previousDistance=0;bool previousKnown=false;
                for(size_t index=0;index<replays.size();++index){const auto& p=*replays[index];
                    if(!p.shadowSelected||!p.shadowSkinned)continue;
                    NorthlightReplayShadowPolicy::Candidate item;item.index=index;item.bytes=p.mesh().byteSize();
                    if(p.constantGroup==previousGroup&&p.originalShader==previousShader&&p.decl==previousDecl){
                        item.known=previousKnown;item.distanceSquared=previousDistance;++distanceReused;
                    }else{
                        ++distanceTests;const auto program=actorPrograms.find(p.originalShader);
                        const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;
                        item.known=program!=actorPrograms.end()&&declarationCache.get(p.decl,elements,count)&&
                            sampledVertices.distance(program->second,p.mesh(),p.shared,p.decl,elements,count,
                                p.constants,context.inverseView,context.camera,item.distanceSquared);
                    }
                    previousGroup=p.constantGroup;previousShader=p.originalShader;previousDecl=p.decl;
                    previousKnown=item.known;previousDistance=item.distanceSquared;shadowCandidates.push_back(item);
                }
                result=NorthlightReplayShadowPolicy::choose(shadowCandidates,budget);
                for(const auto& item:shadowCandidates)replays[item.index]->shadowSelected=item.keep;
            }
            else if(NorthlightActorShadowSelection::Enabled){if(budget)actorShadowHistory.keptAll();else actorShadowHistory.clear();}
            if(stable.actors+stable.rigidActors){result.kept=stable.kept;result.dropped=stable.dropped;result.keptBytes=stable.keptBytes;result.droppedBytes=stable.droppedBytes;result.unknown=stable.unknown;}
            // Do not destroy excluded constant-bank owners. GI has already
            // copied its packets; shadows alone see this reduced, ordered list.
            if(shadowFate.active())for(const auto& p:replays)shadowFate.record(p->fateSlot,NorthlightShadowFate::Fate(p->fateClass),p->shadowSelected,p->fateDistance);
            rigidMemoryObserve(); /* 0.3.172 rigid memory: every captured group, before the unselected leave */
            NorthlightReplayShadowPolicy::retainSelected(replays,heldShadowReplays);
            rigidMemoryInject(); /* remembered groups the game did not draw: after selection, before bounds and upload */
        }catch(...){
            for(auto& p:replays)p->shadowSelected=true;actorShadowHistory.clear();
            logf("SHADOW experiment selection allocation failed; current captured shadows retained");return;
        }
        if(captureSampled){const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            if(stable.actors+stable.rigidActors){deferLogf("MODEL shadow actors ranked=%zu kept=%zu toggles=%zu togglesSinceLog=%zu rankedFramesSinceLog=%u matched=%zu retained=%zu rigidActors=%zu rigidDraws=%zu rigidBytes=%zu attached=%zu attachedBytes=%zu orphans=%zu freeNearBody=%zu attachRadius=%.1f rigidHits=%u rigidMisses=%u rigidScannedVertices=%zu history=%zu cut=prefix margin=%.2f origin=%s rigidNew=%zu prepareMs=%.3f chooseMs=%.3f exemptActors=%zu exemptDraws=%zu exemptBytes=%zu cappedExempt=%zu stillRankedSmall=%zu exemptCapBindsSinceLog=%u rooted=%zu rootFallback=%zu rootRejected=%zu locked=%zu exemptNpcLike=%zu",
                stable.actors,stable.actorsKept,stable.toggles,actorShadowToggles,actorShadowFrames,stable.matched,stable.retained,stable.rigidActors,stable.rigidDraws,stable.rigidBytes,stable.attached,stable.attachedBytes,stable.orphans,stable.rigidStuck,double(NorthlightActorShadowSelection::AttachRadius),rigidBones.hits,rigidBones.misses,rigidBones.scannedVertices,actorShadowHistory.size(),double(NorthlightActorShadowSelection::active().margin),NorthlightActorShadowSelection::FlickerFixes&&actorShadowOriginValid?"pivot":"camera",stable.rigidNew,actorShadowPrepareMs,actorShadowChooseMs,stable.exemptActors,stable.exemptDraws,stable.exemptBytes,stable.cappedExempt,stable.stillRankedSmall,actorShadowCapBinds,stable.rooted,stable.rootFallback,stable.rootRejected,stable.locked,stable.exemptNpcLike);actorShadowCapBinds=0;
                actorShadowToggles=0;actorShadowFrames=0;}
            deferLogf("MODEL shadow selection captured=%zu selected=%zu smallGIExcluded=%zu actorCandidates=%zu actorKept=%zu actorDropped=%zu actorBytes=%zu keptActorBytes=%zu droppedActorBytes=%zu budgetBytes=%zu unknownDistance=%zu distanceTests=%zu distanceReused=%zu selectionMs=%.3f inputCacheHits=%u inputCacheMisses=%u scope=captured-actors distance=sampled-pose-vertex captureBudgetMiB=%u budgetTransitionsSinceLog=%u ranking=%d radius=%u radiusDropped=%zu radiusDroppedDraws=%zu radiusDroppedBytes=%zu radiusTogglesSinceLog=%zu radiusFlickerSinceLog=%zu radiusRekeyedSinceLog=%zu radiusCharacters=%zu radiusSelf=%zu radiusCompanions=%zu radiusInsideBytes=%zu radiusNoPivot=%zu",
                captured,replays.size(),small,actors,result.kept+stable.rigidDraws,result.dropped,actorBytes,result.keptBytes+stable.rigidBytes,result.droppedBytes,budget,result.unknown,distanceTests,distanceReused,ms,sampledVertices.hits,sampledVertices.misses,captureBudgetMiB,actorShadowTransitions,int(ranked&&stable.actors+stable.rigidActors>0),
                quality.actorShadowRadius,stable.radiusDropped,stable.radiusDroppedDraws,stable.radiusDroppedBytes,actorShadowRadiusToggles,actorShadowRadiusFlicker,actorShadowRadiusRekeyed,stable.radiusCharacters,stable.radiusSelf,stable.radiusCompanions,stable.radiusInsideBytes,stable.radiusUnreferenced);
            actorShadowTransitions=0;actorShadowRadiusToggles=actorShadowRadiusFlicker=actorShadowRadiusRekeyed=0;
        }
    }
    // Called once per frame after selection: closes the fate frame and opens the next.
    void finishShadowFate(){
        if(shadowFate.active()){shadowFate.endFrame();if(shadowFate.windowEnded())shadowFate.report([](const char* format,auto... args){logf(format,args...);});}
        // ShadowFateDiagnostics=1 only: 0.2-0.5 ms per sampled window otherwise.
        shadowFate.beginFrame(actorShadowBudgetMiB>0&&shadowFateDiagnostics);
    }
    // Stable per-actor quota (see actor_shadow_selection.h). Distances reuse the
    // same sampled-vertex rule as the legacy ranking; rigid palette tests are
    // cached per immutable snapshot owner.
    NorthlightActorShadowSelection::Result selectStableActors(size_t budget,size_t& distanceTests,size_t& distanceReused){
        const auto started=captureSampled?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
        rigidBones.beginFrame();actorShadowDraws.clear();actorShadowDraws.reserve(replays.size());
        unsigned previousGroup=UINT_MAX;IDirect3DVertexShader9* previousShader=nullptr;
        IDirect3DVertexDeclaration9* previousDecl=nullptr;float previousDistance=0,previousAt[3]={};bool previousKnown=false,groupRigid=true,groupStationary=false;unsigned groupDraw=0;
        const auto tuning=selectionTuning();
        for(size_t index=0;index<replays.size();++index){const auto& p=*replays[index];
            if(!p.shadowSelected||!p.shadowSkinned)continue;
            NorthlightActorShadowSelection::Draw item;item.index=index;item.bytes=p.mesh().byteSize();item.group=p.constantGroup;
            // Program and declaration lookups only for draws that test a distance
            // or a palette (reused-distance draws of multi-bone groups need none).
            auto program=actorPrograms.end();const D3DVERTEXELEMENT9* elements=nullptr;UINT count=0;int declaredState=-1;
            auto declared=[&]{if(declaredState<0){program=actorPrograms.find(p.originalShader);declaredState=program!=actorPrograms.end()&&declarationCache.get(p.decl,elements,count);}return declaredState==1;};
            if(p.constantGroup==previousGroup&&p.originalShader==previousShader&&p.decl==previousDecl){
                item.known=previousKnown;item.distanceSquared=previousDistance;std::memcpy(item.at,previousAt,sizeof item.at);++distanceReused;
            }else{
                ++distanceTests;
                item.known=declared()&&sampledVertices.distance(program->second,p.mesh(),p.shared,p.decl,elements,count,
                    p.constants,context.inverseView,context.camera,item.distanceSquared,item.at);
            }
            // A group is rigid only if every draw is: after its first multi-bone
            // draw the remaining draws need no palette test. Only a group's first
            // draw supplies the actor identity key.
            const bool first=p.constantGroup!=previousGroup;if(first)groupRigid=true;
            item.bone=groupRigid&&declared()?rigidBones.bone(program->second,p.mesh(),p.shared,p.decl,elements,count):NAN;item.rigid=!std::isnan(item.bone);
            groupRigid=item.rigid;
            if(tuning.stableIdentity){
                // Stable per-draw identity: the snapshot-cache entry (VB/IB identity,
                // range, base, declaration) with the shader; a shape hash only for
                // uncached draws. The actor key is the smallest key of its draws.
                std::uint64_t key=14695981039346656037ull;auto mix=[&](uint64_t n){key=(key^n)*1099511628211ull;};
                mix(reinterpret_cast<uintptr_t>(p.originalShader));mix(reinterpret_cast<uintptr_t>(p.decl));
                if(p.shared)mix(reinterpret_cast<uintptr_t>(p.shared.get()));else{mix(p.mesh().vertexCount);mix(p.mesh().primitiveCount);mix(item.bytes);}
                item.key=key;
                // One palette root per constant group (its draws share the pose).
                if(first){if(program==actorPrograms.end())program=actorPrograms.find(p.originalShader);
                    // Only the audited palette template (c31.. row-major 3x4 bones, translation
                    // in w: the skin-envelope specialization) has a provable root.
                    item.hasRoot=program!=actorPrograms.end()&&NorthlightReplayBounds::SkinEnvelope::supports(program->second)&&program->second.paletteBase==31&&
                        NorthlightActorDeformation::rootWorld(program->second,p.constants,context.inverseView,item.root);}}
            else if(first){std::uint64_t key=14695981039346656037ull;auto mix=[&](uint64_t n){key=(key^n)*1099511628211ull;};
                mix(reinterpret_cast<uintptr_t>(p.originalShader));mix(reinterpret_cast<uintptr_t>(p.decl));mix(p.mesh().vertexCount);mix(p.mesh().primitiveCount);mix(item.bytes);
                item.key=key;groupDraw=0;
                // Two extra world samples per draw (first two draws) only for actors
                // the history marks as possibly stationary: the idle-pose check.
                groupStationary=tuning.stationary&&!item.rigid&&item.known&&actorShadowHistory.stationaryHint(key,item.at);}
            if(!tuning.stableIdentity){
                if(groupStationary&&groupDraw<2&&declared())for(unsigned x=0;x<2;++x)
                    if(NorthlightActorDeformation::sampledExtraWorld(program->second,p.mesh(),elements,count,x,p.constants,context.inverseView,item.extra[item.extras]))++item.extras;
                ++groupDraw;}
            previousGroup=p.constantGroup;previousShader=p.originalShader;previousDecl=p.decl;
            previousKnown=item.known;previousDistance=item.distanceSquared;std::memcpy(previousAt,item.at,sizeof previousAt);actorShadowDraws.push_back(item);
        }
        // Stable identity judges stillness by the palette root: no extra vertex samples.
        const auto prepared=captureSampled?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
        // Radius: the eye and the pivot (origin) find the player on the centre ray; the quota
        // ranks (shouldRank) on the bytes inside the radius.
        const NorthlightActorShadowSelection::Radius radius{float(quality.actorShadowRadius),context.camera,true,true};
        const auto result=NorthlightActorShadowSelection::choose(actorShadowDraws,actorShadowScratch,budget,actorShadowHistory,captureSampled,
            NorthlightActorShadowSelection::FlickerFixes&&actorShadowOriginValid?actorShadowOrigin:nullptr,tuning,radius.active()?&radius:nullptr);
        if(captureSampled){const auto chosen=std::chrono::steady_clock::now();
            actorShadowPrepareMs=std::chrono::duration<double,std::milli>(prepared-started).count();actorShadowChooseMs=std::chrono::duration<double,std::milli>(chosen-prepared).count();}
        for(const auto& item:actorShadowDraws)replays[item.index]->shadowSelected=item.keep;
        if(shadowFate.active())for(const auto& a:actorShadowScratch.actors){using namespace NorthlightShadowFate;
            const Fate f=a.rigid?Rigid:(a.body!=SIZE_MAX||a.orphan)?Attached:a.exempt?Exempt:a.waiting?Waiting:a.keep?Kept:Dropped;
            for(size_t i=a.first;i<a.first+a.count;++i){auto& r=*replays[actorShadowDraws[i].index];r.fateClass=f;r.fateDistance=std::sqrt(std::max(0.f,actorShadowDraws[i].distanceSquared));}}
        return result;
    }
