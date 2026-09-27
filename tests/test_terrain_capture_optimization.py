#!/usr/bin/env python3
# northlight-test: requires=cxx manual  (needs a prior header argument)
"""Compare immediate snapshots with a preserved prior header; no game process.

Usage: python3 test_terrain_capture_optimization.py <prior terrain_snapshot.h>
Both implementations run against the same mutable fake D3D API and 145-vertex,
256-triangle ADT topology. The baseline must be captured before the optimization.
"""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import ast,hashlib,json,subprocess,sys,tempfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
old=Path(sys.argv[1]).resolve()
tree=ast.parse((HERE/'test_terrain_snapshot.py').read_text())
strings={node.targets[0].id:ast.literal_eval(node.value) for node in tree.body
         if isinstance(node,ast.Assign) and isinstance(node.targets[0],ast.Name)
         and node.targets[0].id in ('stub','harness')}
fixture=strings['harness'].split('int main(){')[0]
source=r'''
#include "terrain_capture_bounds.h"
#define NorthlightTerrainCapture BaselineTerrainCapture
#include "baseline.h"
#undef NorthlightTerrainCapture
#include <chrono>
#include <algorithm>
'''+fixture+r'''
template<class A,class B>void same(const A&a,const B&b){
    assert(a.positions.size()==b.positions.size()&&a.indices==b.indices);
    for(size_t i=0;i<a.positions.size();++i){assert(a.positions[i].x==b.positions[i].x);assert(a.positions[i].y==b.positions[i].y);assert(a.positions[i].z==b.positions[i].z);}
    assert(a.bounds.chunks.size()==b.bounds.chunks.size());
    for(unsigned j=0;j<3;++j){assert(a.bounds.low[j]==b.bounds.low[j]);assert(a.bounds.high[j]==b.bounds.high[j]);}
    for(size_t i=0;i<a.bounds.chunks.size();++i){const auto&x=a.bounds.chunks[i];const auto&y=b.bounds.chunks[i];
        assert(x.x==y.x&&x.y==y.y&&x.minX==y.minX&&x.minY==y.minY&&x.maxX==y.maxX&&x.maxY==y.maxY);}
}
template<class Cache,class Snapshot>double bench(Device&d,Cache&cache,int iterations){
    auto begin=std::chrono::steady_clock::now();unsigned total=0;
    for(int i=0;i<iterations;++i){cache.clearFrame();Snapshot out;
        assert(cache.readMesh(&d,D3DPT_TRIANGLELIST,0,0,145,unsigned(i%256)*768,256,d.view,out));total+=unsigned(out.indices.size());}
    assert(total==unsigned(iterations)*768);
    return std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/iterations;
}
int main(){
    Device d;d.vb.bytes.resize(16+145*24);
    int rows[17];unsigned vertex=0;const double origin=17066.666666666666,step=100./3.;
    const double x0=origin-821*step,y0=origin-548*step;
    for(int row=0;row<17;++row){rows[row]=int(vertex);for(int col=0;col<((row&1)?8:9);++col){
        d.put(vertex++,{float(x0+(col+.5*(row&1))*step/8),float(y0+row*step/16),float(std::sin(row*.23+col*.13)*3)});}}
    assert(vertex==145);std::vector<std::uint16_t> list;
    for(int y=0;y<8;++y)for(int x=0;x<8;++x){
        unsigned a=unsigned(rows[y*2]+x),b=a+1,c=unsigned(rows[y*2+2]+x),e=c+1,m=unsigned(rows[y*2+1]+x);
        for(unsigned index:{a,b,m,b,e,m,e,c,m,c,a,m})list.push_back(std::uint16_t(index));}
    assert(list.size()==768);d.ib.bytes.resize(list.size()*2);std::memcpy(d.ib.bytes.data(),list.data(),d.ib.bytes.size());
    FrameCache optimized;BaselineTerrainCapture::FrameCache baseline;
    MeshSnapshot a;BaselineTerrainCapture::MeshSnapshot b;
    for(int round=0;round<4;++round){optimized.clearFrame();baseline.clearFrame();
        assert(optimized.readMesh(&d,D3DPT_TRIANGLELIST,0,0,145,0,256,d.view,a));
        assert(baseline.readMesh(&d,D3DPT_TRIANGLELIST,0,0,145,0,256,d.view,b));same(a,b);
        auto preserved=a;Position p;std::memcpy(&p,d.vb.bytes.data()+16+4,12);p.z+=2;d.put(0,p);
        assert(a.positions[0].z==preserved.positions[0].z);
        const unsigned count=round&1?128:64;
        assert(optimized.readMeshUP(&d,D3DPT_TRIANGLELIST,0,145,count,list.data(),D3DFMT_INDEX16,d.vb.bytes.data()+16,24,d.view,a));
        assert(baseline.readMeshUP(&d,D3DPT_TRIANGLELIST,0,145,count,list.data(),D3DFMT_INDEX16,d.vb.bytes.data()+16,24,d.view,b));same(a,b);
        auto corrupt=list;corrupt[30]=145;Diagnostics da;BaselineTerrainCapture::Diagnostics db;
        assert(!optimized.readMeshUP(&d,D3DPT_TRIANGLELIST,0,145,256,corrupt.data(),D3DFMT_INDEX16,d.vb.bytes.data()+16,24,d.view,a,&da));
        assert(!baseline.readMeshUP(&d,D3DPT_TRIANGLELIST,0,145,256,corrupt.data(),D3DFMT_INDEX16,d.vb.bytes.data()+16,24,d.view,b,&db));
        assert(unsigned(da.reason)==unsigned(db.reason)&&a.positions.empty()&&b.positions.empty());
    }
    // A failed partial remap must not poison the next capture's reused storage.
    optimized.clearFrame();baseline.clearFrame();Position saved;std::memcpy(&saved,d.vb.bytes.data()+16+24*40+4,12);
    d.put(40,{saved.x,saved.y,std::numeric_limits<float>::quiet_NaN()});
    assert(!optimized.readMesh(&d,D3DPT_TRIANGLELIST,0,0,145,0,256,d.view,a));
    assert(!baseline.readMesh(&d,D3DPT_TRIANGLELIST,0,0,145,0,256,d.view,b));
    d.put(40,saved);
    assert(optimized.readMesh(&d,D3DPT_TRIANGLELIST,0,0,145,0,256,d.view,a));
    assert(baseline.readMesh(&d,D3DPT_TRIANGLELIST,0,0,145,0,256,d.view,b));same(a,b);
    assert(d.vb.locks==d.vb.unlocks&&d.ib.locks==d.ib.unlocks);
    assert(d.vb.refs==1&&d.ib.refs==1&&d.declaration.refs==1);
    // Populate 256 distinct draw contracts, avoiding an unrealistically tiny
    // one-entry lookup benchmark. Every IB span contains the same valid ADT.
    d.ib.bytes.resize(list.size()*2*256);
    for(unsigned draw=0;draw<256;++draw)std::memcpy(d.ib.bytes.data()+draw*list.size()*2,list.data(),list.size()*2);
    double oldTimes[5],newTimes[5];
    for(int pass=0;pass<5;++pass){
        oldTimes[pass]=bench<BaselineTerrainCapture::FrameCache,BaselineTerrainCapture::MeshSnapshot>(d,baseline,2000);
        newTimes[pass]=bench<FrameCache,MeshSnapshot>(d,optimized,2000);}
    std::sort(oldTimes,oldTimes+5);std::sort(newTimes,newTimes+5);
    std::printf("{\"baseline_us_per_draw\":%.6f,\"optimized_us_per_draw\":%.6f,\"speedup\":%.4f,\"same_frame_snapshot_equivalence\":true}\n",oldTimes[2],newTimes[2],oldTimes[2]/newTimes[2]);
}
'''
with tempfile.TemporaryDirectory(prefix='northlight-terrain-opt-') as temp:
    temp=Path(temp);(temp/'d3d9.h').write_text(strings['stub']);(temp/'baseline.h').write_bytes(old.read_bytes());(temp/'test.cpp').write_text(source)
    common=['clang++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(temp),*fp.test_include_flags(),str(temp/'test.cpp')]
    runs={}
    for name,flags in [('native',['-O2']),('sanitizer',['-O1','-g','-fsanitize=address,undefined'])]:
        binary=temp/name;subprocess.run(common+flags+['-o',str(binary)],check=True)
        result=subprocess.run([str(binary)],check=True,capture_output=True,text=True)
        print(name+': '+result.stdout.strip());runs[name]=json.loads(result.stdout)
report={'baseline_sha256':hashlib.sha256(old.read_bytes()).hexdigest(),
        'optimized_sha256':hashlib.sha256(fp.src('terrain_capture_bounds.h').read_bytes()).hexdigest(),
        'fixture':'256 draw contracts,each145 vertices/256 triangles;mutable fake D3D;native ARM64 timings are not game/Wine timings',
        'tests':'buffer and UP equality; growing/shrinking scratch; malformed index and partial nonfinite remap recovery;COM/lock balance',
        'runs':runs,'game_launched':False}
(fp.output_dir()/'terrain-capture-optimization-validation.json').write_text(json.dumps(report,indent=2)+'\n')
