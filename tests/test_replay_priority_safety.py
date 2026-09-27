#!/usr/bin/env python3
# northlight-test: requires=cxx,client,stormlib
"""Independent priority/lifetime checks, including real queued-phase control flow."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import client_fixtures  # the real client programs, from the tester's client
from pathlib import Path
import ast
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent

def literal(path, name):
    return next(ast.literal_eval(n.value) for n in ast.parse(path.read_text()).body
                if isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id == name for t in n.targets))

stub = literal(HERE / 'test_terrain_snapshot.py', 'stub')
fixture = literal(HERE / 'test_replay_skin_safety.py', 'harness').split('int main(')[0]
fixture = fixture.replace('static Status finish(', '[[maybe_unused]] static Status finish(')
harness = fixture + r'''
#include "replay_bounds_schedule.h"
#include <new>
using Stage=EnvelopeCache::Stage;
using Kind=EnvelopeCache::Kind;
using Info=EnvelopeCache::WorkInfo;
static Budget cheapBudget(){Budget b;b.maxVertices=0;b.maxOperations=163840;return b;}
static Budget heavyBudget(){Budget b;b.maxVertices=0;b.maxOperations=32768;return b;}
static Budget buildBudget(){Budget b;b.maxVertices=2048;b.maxOperations=65536;return b;}
static Program rigid(){
    Program p;p.major=3;p.positionRegister=0;p.inputs.push_back({0,0,0});
    Operation op;op.code=1;op.destination=0x800f0000;op.source[0].token=0x90e40000;p.operations.push_back(op);
    op={};op.code=2;op.destination=0x80070000;op.source[0].token=0x80e40000;op.source[1].token=0xa0e4000a;p.operations.push_back(op);return p;
}
static void warm(EnvelopeCache& cache,const Program& p,const std::shared_ptr<const Mesh>& m,const float* c,const float* inverse,size_t* evictions=nullptr){
    for(unsigned attempt=0;attempt<10000;++attempt){cache.beginFrame();auto b=cheapBudget();Bounds out;Info info;
        cache.calculate(p,*m,m,declaration,4,c,inverse,b,out,Stage::Cheap,&info);
        if(info.kind==Kind::Cheap||info.kind==Kind::Heavy)return;
        assert(info.kind==Kind::Cold||info.kind==Kind::Unknown);
        auto build=buildBudget();const double readyMs=cache.readyMilliseconds(),heavyMs=cache.heavyMilliseconds();
        cache.calculate(p,*m,m,declaration,4,c,inverse,build,out,Stage::Build);
        assert(!out.valid&&cache.readyMilliseconds()==readyMs&&cache.heavyMilliseconds()==heavyMs&&cache.stats().heavyAttempts==0);
        if(evictions)*evictions+=cache.stats().evictions;
    }assert(false);
}
static void expectCold(EnvelopeCache& cache,const Program& p,const std::shared_ptr<const Mesh>& m,const float* c,const float* inverse,const D3DVERTEXELEMENT9* decl=declaration){
    for(Stage stage:{Stage::Cheap,Stage::Heavy}){
        cache.beginFrame();auto b=stage==Stage::Cheap?cheapBudget():heavyBudget();Bounds out;out.valid=true;Info info;const size_t entries=cache.entries();
        auto status=cache.calculate(p,*m,m,decl,4,c,inverse,b,out,stage,&info);
        assert(status==Status::Budget&&info.kind==Kind::Cold&&!out.valid&&b.vertices==0&&b.operations==0&&cache.entries()==entries);
    }
}
int main(int argc,char** argv){
    assert(argc==2);Program heavy=load(argv[1]);Operation copy;copy.code=1;copy.destination=0x80070001;copy.source[0].token=0x80e40001;heavy.operations.push_back(copy);
    assert(!SkinEnvelope::supports(heavy));auto cheap=rigid();float c[1024];palette(c,0);float inverse[16]={};for(unsigned i=0;i<4;++i)inverse[i*5]=1;
    auto heavyMesh=std::make_shared<const Mesh>(geometry(384));auto cheapMesh=std::make_shared<const Mesh>(geometry(12));
    EnvelopeCache cache;expectCold(cache,heavy,heavyMesh,c,inverse);assert(cache.entries()==0);warm(cache,heavy,heavyMesh,c,inverse);warm(cache,cheap,cheapMesh,c,inverse);
    // Classifying a costly ready entry must not run its VM or rebuild inputs.
    cache.beginFrame();auto cb=cheapBudget();Bounds out;Info info;
    auto status=cache.calculate(heavy,*heavyMesh,heavyMesh,declaration,4,c,inverse,cb,out,Stage::Cheap,&info);
    assert(status==Status::Budget&&info.kind==Kind::Heavy&&!out.valid&&cb.operations==0&&cb.vertices==0);
    assert(cache.stats().heavyAttempts==0&&cache.stats().buildVertices==0&&cache.stats().heavyCandidates==1);

    // Metadata is only a queue hint. New constants and a new inverse camera
    // always feed evaluation, even with an otherwise identical immutable mesh.
    cache.beginFrame();cb=cheapBudget();Bounds before,after;
    assert(cache.calculate(cheap,*cheapMesh,cheapMesh,declaration,4,c,inverse,cb,before,Stage::Cheap,&info)==Status::Valid);
    c[40]=250;inverse[12]=-1000;cache.beginFrame();cb=cheapBudget();
    assert(cache.calculate(cheap,*cheapMesh,cheapMesh,declaration,4,c,inverse,cb,after,Stage::Cheap,&info)==Status::Valid);
    enclosed(cheap,*cheapMesh,c,inverse,after);assert(after.high[0]<before.low[0]-700);
    c[40]=0;inverse[12]=0;

    // Exact program/DEF, declaration VALUE and mesh-owner identities matter.
    auto changed=cheap;changed.definitions.push_back({10,{5,6,7,0}});expectCold(cache,changed,cheapMesh,c,inverse);
    D3DVERTEXELEMENT9 changedDeclaration[4];std::memcpy(changedDeclaration,declaration,sizeof declaration);changedDeclaration[0].Offset=4;
    expectCold(cache,cheap,cheapMesh,c,inverse,changedDeclaration);
    auto newOwner=std::make_shared<const Mesh>(*cheapMesh);expectCold(cache,cheap,newOwner,c,inverse);

    // Interrupt after the first eight used bones. Neither the last completed
    // output nor the partially visited hull may remain usable for culling.
    SkinEnvelope interrupted;Four input[16]={};input[0]={1,2,3,1};input[1]={.25f,.25f,.25f,.25f};
    for(unsigned bone=0;bone<75;++bone){input[2].fill(float(bone));assert(interrupted.add(input));}
    Bounds completed;assert(interrupted.evaluate(c,inverse,completed)&&completed.valid);out=completed;unsigned checks=0;
    const auto stop=[](void* context){return ++*static_cast<unsigned*>(context)>=2;};
    assert(!interrupted.evaluate(c,inverse,out,stop,&checks)&&checks==2&&!out.valid);

    // Large heavy prefix, then useful cheap draws, then one cold packet.
    // Reproduce production per-frame hints, three rotating passes and budgets.
    struct Packet {bool expensive=false,cold=false;Info info;Bounds bounds;};
    std::vector<Packet> packets(1033);for(size_t i=0;i<1024;++i)packets[i].expensive=true;packets.back().cold=true;
    auto coldMesh=std::make_shared<const Mesh>(geometry(66));size_t cheapCursor=0,heavyCursor=0,buildCursor=0,cheapCovered=0,frames=0,warmFrames=0;
    bool covered[8]={};size_t heavyAttempts=0,coldBuilt=0;
    for(;frames<1000&&(cheapCovered<8||warmFrames<20);++frames){
        cache.beginFrame();cb=cheapBudget();auto hb=heavyBudget(),bb=buildBudget();for(auto& packet:packets){packet.info={};packet.bounds={};}
        auto visit=[&](size_t index,Stage stage,Budget& budget){auto& packet=packets[index];const auto& p=packet.expensive?heavy:cheap;
            const auto& mesh=packet.expensive?heavyMesh:packet.cold?coldMesh:cheapMesh;
            auto s=cache.calculate(p,*mesh,mesh,declaration,4,c,inverse,budget,packet.bounds,stage,stage==Stage::Build?nullptr:&packet.info);
            if(packet.bounds.valid){enclosed(p,*mesh,c,inverse,packet.bounds);if(index>=1024&&index<1032&&!covered[index-1024]){covered[index-1024]=true;++cheapCovered;}}
            return s==Status::Budget;
        };
        cheapCursor=NorthlightReplaySchedule::pass(packets.size(),cheapCursor,[&](){return cache.canReady()&&cb.operations<cb.maxOperations;},[&](size_t index){return visit(index,Stage::Cheap,cb);});
        size_t classifiedHeavy=0;for(const auto& packet:packets)classifiedHeavy+=packet.info.kind==Kind::Heavy;
        assert(cache.stats().heavyAttempts==0&&cache.stats().buildVertices==0);
        heavyCursor=NorthlightReplaySchedule::pass(packets.size(),heavyCursor,[&](){return cache.canHeavy()&&hb.operations<hb.maxOperations;},[&](size_t index){return packets[index].info.kind==Kind::Heavy&&!packets[index].bounds.valid?visit(index,Stage::Heavy,hb):false;});
        assert(cache.stats().heavyAttempts<=classifiedHeavy);heavyAttempts+=cache.stats().heavyAttempts;
        bool anyCold=false;for(const auto& packet:packets)anyCold|=packet.info.kind==Kind::Cold;
        buildCursor=NorthlightReplaySchedule::pass(packets.size(),buildCursor,[&](){return cache.canBuild()&&bb.vertices<bb.maxVertices&&bb.operations<bb.maxOperations;},[&](size_t index){return packets[index].info.kind==Kind::Cold&&!packets[index].bounds.valid?visit(index,Stage::Build,bb):false;});
        if(!anyCold){assert(cache.buildMilliseconds()==0&&cache.stats().buildVertices==0&&cache.stats().buildSkippedReady==0);if(cheapCovered==8)++warmFrames;}
        coldBuilt+=cache.stats().buildVertices;
        assert(cb.vertices==0&&hb.vertices==0&&bb.vertices<=2048&&cb.operations+hb.operations+bb.operations<=262144);
    }
    assert(cheapCovered==8&&warmFrames>=20&&heavyAttempts>0&&coldBuilt==66);

    // Stale queued Heavy metadata after clear may not fetch old source bounds.
    cache.clear();cache.beginFrame();auto hb=heavyBudget();out.valid=true;
    assert(cache.calculate(heavy,*heavyMesh,heavyMesh,declaration,4,c,inverse,hb,out,Stage::Heavy,&info)==Status::Budget);
    assert(info.kind==Kind::Cold&&!out.valid&&hb.operations==0&&cache.entries()==0);

    // Reusing the same object address with a new shared lifetime must not hit.
    alignas(Mesh) unsigned char storage[sizeof(Mesh)];
    auto create=[&](){return std::shared_ptr<const Mesh>(new(storage) Mesh(geometry(12)),[](const Mesh* p){const_cast<Mesh*>(p)->~Mesh();});};
    auto first=create();const auto* address=first.get();warm(cache,cheap,first,c,inverse);std::weak_ptr<const Mesh> weak=first;first.reset();assert(weak.expired());
    auto replacement=create();assert(replacement.get()==address);expectCold(cache,cheap,replacement,c,inverse);replacement.reset();

    // Queue hints also survive neither LRU eviction nor geometry cache churn.
    EnvelopeCache eviction;warm(eviction,cheap,cheapMesh,c,inverse);std::vector<std::shared_ptr<const Mesh>> retained;size_t evicted=0;
    for(unsigned n=0;n<2300;++n){retained.push_back(std::make_shared<const Mesh>(geometry(3)));warm(eviction,cheap,retained.back(),c,inverse,&evicted);assert(eviction.entries()<=2048&&eviction.reservedBytes()<=8u*1024u*1024u);}
    expectCold(eviction,cheap,cheapMesh,c,inverse);assert(cheapMesh.use_count()==1&&evicted>0);
    std::printf("PASS priority fairness: %zu frames, 8/8 cheap packets behind1024 heavy, %zu heavy attempts, cold built%zu vertices; %zu fullywarm frames have zero build work. Currentpose/camera, DEF/declaration/owner identity, reset, same-address lifetime reuse and2300-entry eviction validated. Eviction events observed%zu.\n",frames,heavyAttempts,coldBuilt,warmFrames,evicted);
}
'''
with tempfile.TemporaryDirectory(prefix='northlight-priority-safety-') as tmp:
    root = Path(tmp)
    (root / 'd3d9.h').write_text(stub)
    (root / 'test.cpp').write_text(harness)
    for flags in (['-O2'], ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']):
        subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', *flags,
                        '-I', str(root), *fp.test_include_flags(), str(root / 'test.cpp'), '-o', str(root / 'test')], check=True)
        subprocess.run([str(root / 'test'), str(client_fixtures.four_bone_vs3())], check=True)
