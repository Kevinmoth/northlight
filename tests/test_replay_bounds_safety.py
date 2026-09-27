#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native undefined-channel / homogeneous bounds regressions. No D3D device."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import ast
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
tree = ast.parse((HERE / 'test_terrain_snapshot.py').read_text())
stub = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
            and any(isinstance(t, ast.Name) and t.id == 'stub' for t in n.targets))
harness = r'''
#include "replay_bounds.h"
#include <cassert>
#include <cstdio>
using namespace NorthlightReplayBounds;
using namespace NorthlightActorDeformation;
static Operation mov(uint32_t destination,uint32_t source,uint32_t address=0){
    Operation op;op.code=1;op.destination=destination;op.source[0].token=source;op.source[0].address=address;return op;
}
int main(){
    float constants[1024]={};constants[3]=1;constants[7]=1;
    detail::Vector inputs[16]={},position;inputs[0][3]={1,0};
    Program p;p.major=3;p.positionRegister=0;p.inputs.push_back({0,0,0});
    p.operations={mov(0x800f0000,0x90e40000)};assert(detail::evaluateIntervals(p,inputs,constants,position));
    // A GPU temporary is undefined before a write; host zero-init cannot prove
    // a zero-radius position. This was previously accepted as (0,0,0,1).
    p.operations={mov(0x80070000,0x80e40001),mov(0x80080000,0xa0ff0000)};
    assert(!detail::evaluateIntervals(p,inputs,constants,position));
    // An explicitly written x can be consumed without reading undefined yz/w.
    p.operations={mov(0x80010001,0xa0000000),mov(0x80010000,0x80000001),mov(0x800e0000,0xa0e40000)};
    assert(detail::evaluateIntervals(p,inputs,constants,position));
    // A swizzle that reads unwritten y must still fail despite destination x.
    p.operations[1].source[0].token=0x80550001;
    assert(!detail::evaluateIntervals(p,inputs,constants,position));
    // Final position itself must have all four initialized components.
    p.operations={mov(0x80080000,0xa0ff0000)};
    assert(!detail::evaluateIntervals(p,inputs,constants,position));
    // Relative palette access must have a valid write to its actual a0 lane.
    p.operations={mov(0x800f0000,0xa0e42001,0xb0000000)};
    assert(!detail::evaluateIntervals(p,inputs,constants,position));
    p.operations.insert(p.operations.begin(),mov(0xb0010000,0xa0000000));
    assert(detail::evaluateIntervals(p,inputs,constants,position));
    p.operations[0].destination=0xb0040000;
    assert(!detail::evaluateIntervals(p,inputs,constants,position));
    p.operations={mov(0x800f0000,0x90e40000)};p.inputs.clear();
    assert(!detail::evaluateIntervals(p,inputs,constants,position));
    p.inputs.push_back({0,0,0});
    // Ordinary world Vec3 bounds have implicit W=1. Even tiny homogeneous-W
    // deviations are unsafe at large camera/light translations.
    inputs[0][3]={1.00005f,0};assert(!detail::evaluateIntervals(p,inputs,constants,position));
    inputs[0][3]={1.f,1e-6};assert(!detail::evaluateIntervals(p,inputs,constants,position));
    inputs[0][3]={1.f,0};assert(detail::evaluateIntervals(p,inputs,constants,position));

    auto mesh=std::make_shared<NorthlightDrawSnapshot::Mesh>();mesh->vertexCount=64;mesh->primitiveCount=21;mesh->indexed=false;mesh->topology=D3DPT_TRIANGLELIST;
    mesh->streams[0].stride=16;mesh->streams[0].bytes.resize(64*16);
    const float vertex[]={0,0,0,1.00005f};for(unsigned n=0;n<64;++n)std::memcpy(mesh->streams[0].bytes.data()+n*16,vertex,16);
    D3DVERTEXELEMENT9 decl[]={{0,0,3,0,0,0},{0xff,0,17,0,0,0}};
    float inverse[16]={};for(unsigned n=0;n<4;++n)inverse[n*5]=1;inverse[12]=5000;
    Bounds bounds;Budget budget;
    assert(calculate(p,*mesh,decl,2,constants,inverse,budget,bounds)==Status::Unsupported&&!bounds.valid);
    EnvelopeCache cache;Status status=Status::Budget;
    for(unsigned attempt=0;attempt<100&&status==Status::Budget;++attempt){cache.beginFrame();budget=Budget{};status=cache.calculate(p,*mesh,mesh,decl,2,constants,inverse,budget,bounds);}
    assert(status==Status::Unsupported&&!bounds.valid);
    // Previously produced [4999.98877,5000.01123], excluding homogeneous
    // x=5000.24951; both exact/envelope paths now preserve the draw instead.
    std::puts("PASS initialized temporary/address/input lanes, masked/swizzled reads, homogeneous W fail-open in exact and envelope paths");
}
'''
with tempfile.TemporaryDirectory(prefix='northlight-bounds-safety-') as tmp:
    root = Path(tmp)
    (root / 'd3d9.h').write_text(stub)
    (root / 'test.cpp').write_text(harness)
    for flags in (['-O2'], ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']):
        subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', *flags,
                        '-I', str(root), *fp.test_include_flags(), str(root / 'test.cpp'), '-o', str(root / 'test')], check=True)
        subprocess.run([str(root / 'test')], check=True)
