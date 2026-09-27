#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native all-vertex posed replay bounds tests. Never creates a D3D device."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import ast,subprocess,tempfile,json
HERE=Path(__file__).resolve().parent
def literal(path,name):
 return next(ast.literal_eval(n.value) for n in ast.parse(path.read_text()).body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==name for t in n.targets))
stub=literal(HERE/'test_terrain_snapshot.py','stub')
fixture=literal(HERE/'test_actor_deformation.py','harness').split('int main(')[0]
harness=fixture+r'''
#include "replay_bounds.h"
#include <chrono>
using namespace NorthlightReplayBounds;
int main(){
 float constants[1024]={};for(unsigned bone=0;bone<4;++bone){unsigned r=31+3*bone;constants[r*4]=1;constants[(r+1)*4+1]=1;constants[(r+2)*4+2]=1;constants[r*4+3]=100.f*bone;constants[(r+1)*4+3]=10.f*bone;}
 NorthlightDrawSnapshot::Mesh mesh;mesh.vertexCount=3;mesh.primitiveCount=1;mesh.indexed=true;mesh.indices={0,1,2};mesh.streams[0].stride=20;mesh.streams[0].bytes.resize(60);
 float xyz[3][3]={{2,3,4},{5,-2,9},{-7,8,-1}};for(unsigned i=0;i<3;++i){std::memcpy(mesh.streams[0].bytes.data()+i*20,xyz[i],12);mesh.streams[0].bytes[i*20+16]=i;}
 D3DVERTEXELEMENT9 decl[]={{0,0,2,0,0,0},{0,16,5,0,2,0},{0xff,0,17,0,0,0}};
 float inverse[16]={};for(unsigned j=0;j<4;++j)inverse[j*5]=1;inverse[12]=-10000;inverse[13]=-900;inverse[14]=40;
 Program program;Bounds b;Budget budget;unsigned tested=0;
 for(unsigned major=1;major<=3;++major){auto words=code(major);assert(compile(words.data(),words.size(),program));budget.reset();assert(calculate(program,mesh,decl,3,constants,inverse,budget,b)==Status::Valid);assert(b.valid&&budget.vertices==3);
  std::vector<Position> positions;assert(worldPositions(program,mesh,decl,3,constants,inverse,positions));for(auto p:positions){float values[]={p.x,p.y,p.z};for(unsigned j=0;j<3;++j)assert(b.low[j]<=values[j]&&b.high[j]>=values[j]);}
  // The last vertex uses another bone and extends ~200 units from the first.
  assert(b.high[0]>-9807&&b.low[0]<-9998);++tested;
 }
 auto original=b;float rotated[1024];std::memcpy(rotated,constants,sizeof rotated);
 for(unsigned bone=0;bone<4;++bone){unsigned r=31+3*bone;float row0[]={0,1,0,10.f*bone},row1[]={-1,0,0,-100.f*bone};std::memcpy(rotated+r*4,row0,16);std::memcpy(rotated+(r+1)*4,row1,16);}
 float orbit[16];std::memcpy(orbit,inverse,sizeof orbit);orbit[0]=orbit[5]=0;orbit[1]=1;orbit[4]=-1;budget.reset();assert(calculate(program,mesh,decl,3,rotated,orbit,budget,b)==Status::Valid);
 for(unsigned i=0;i<3;++i){assert(std::fabs(b.low[i]-original.low[i])<.1f);assert(std::fabs(b.high[i]-original.high[i])<.1f);}
 budget.maxVertices=2;assert(calculate(program,mesh,decl,3,constants,inverse,budget,b)==Status::Budget&&!b.valid);budget=Budget{};budget.maxOperations=1;assert(calculate(program,mesh,decl,3,constants,inverse,budget,b)==Status::Budget&&!b.valid);budget=Budget{};
 auto bad=program;bad.operations[0].code=7;assert(calculate(bad,mesh,decl,3,constants,inverse,budget,b)==Status::Unsupported&&!b.valid);
 auto changed=mesh;changed.indices[2]=3;assert(calculate(program,changed,decl,3,constants,inverse,budget,b)==Status::Invalid&&!b.valid);
 changed=mesh;changed.streams[0].bytes[56]=255;assert(calculate(program,changed,decl,3,constants,inverse,budget,b)==Status::Unsupported&&!b.valid);
 changed=mesh;changed.streams[0].bytes.pop_back();assert(calculate(program,changed,decl,3,constants,inverse,budget,b)==Status::Invalid&&!b.valid);
 float invalid[16];std::memcpy(invalid,inverse,sizeof invalid);invalid[1]=INFINITY;assert(calculate(program,mesh,decl,3,constants,invalid,budget,b)==Status::Invalid&&!b.valid);
 // All vertices are included, even if unused by the current indices: conservative.
 changed=mesh;changed.vertexCount=4;changed.streams[0].bytes.resize(80);float far[]={1000,0,0};std::memcpy(changed.streams[0].bytes.data()+60,far,12);budget.reset();assert(calculate(program,changed,decl,3,constants,inverse,budget,b)==Status::Valid&&b.high[0]>=-9000);
 Bounds box;box.valid=true;for(unsigned i=0;i<3;++i){box.low[i]=-1;box.high[i]=1;}float center[]={3,0,0};assert(!outsideSphere(box,center,2));assert(outsideSphere(box,center,1.99f));box.valid=false;assert(!outsideSphere(box,center,0));box.valid=true;box.low[0]=INFINITY;assert(!outsideSphere(box,center,0));
 // Snapshot ownership: source mutations after the accepted draw change only a
 // separate new draw's bounds, never an already-owned packet.
 auto captured=mesh;std::fill(mesh.streams[0].bytes.begin(),mesh.streams[0].bytes.end(),0xff);budget.reset();assert(calculate(program,captured,decl,3,constants,inverse,budget,b)==Status::Valid);
 // Exact cached bounds: original interval output must be bit-identical.
 Cache cache;auto owner=std::make_shared<const NorthlightDrawSnapshot::Mesh>(captured);
 Bounds expected;budget=Budget{};assert(calculate(program,*owner,decl,3,constants,inverse,budget,expected)==Status::Valid);
 cache.beginFrame();budget=Budget{};assert(cache.calculate(program,*owner,owner,decl,3,constants,inverse,budget,b)==Status::Valid&&budget.vertices==3);
 auto sameBounds=[&](const Bounds& a,const Bounds& other){return a.valid==other.valid&&!std::memcmp(a.low,other.low,12)&&!std::memcmp(a.high,other.high,12);};
 assert(sameBounds(b,expected));cache.beginFrame();budget=Budget{};
 assert(cache.calculate(program,*owner,owner,decl,3,constants,inverse,budget,b)==Status::Valid&&cache.hits()==1&&budget.vertices==0&&budget.operations==0);
 assert(sameBounds(b,expected));
 float movedConstants[1024];std::memcpy(movedConstants,constants,sizeof constants);movedConstants[31*4+3]+=10;
 cache.beginFrame();budget=Budget{};assert(cache.calculate(program,*owner,owner,decl,3,movedConstants,inverse,budget,b)==Status::Valid&&budget.vertices==3);
 budget=Budget{};assert(calculate(program,*owner,decl,3,movedConstants,inverse,budget,expected)==Status::Valid&&sameBounds(b,expected));
 // Camera transforms, declarations and whole program definitions are key data.
 float movedInverse[16];std::memcpy(movedInverse,inverse,sizeof inverse);movedInverse[12]+=3;
 cache.beginFrame();budget=Budget{};assert(cache.calculate(program,*owner,owner,decl,3,constants,movedInverse,budget,b)==Status::Valid&&budget.vertices==3);
 auto changedProgram=program;changedProgram.definitions.push_back({250,{1,2,3,4}});
 cache.beginFrame();budget=Budget{};assert(cache.calculate(changedProgram,*owner,owner,decl,3,constants,inverse,budget,b)==Status::Valid&&budget.vertices==3);
 D3DVERTEXELEMENT9 changedDecl[3];std::memcpy(changedDecl,decl,sizeof decl);changedDecl[0].Offset=4;
 cache.beginFrame();budget=Budget{};assert(cache.calculate(program,*owner,owner,changedDecl,3,constants,inverse,budget,b)==Status::Valid&&budget.vertices==3);
 // New immutable geometry, even at same draw layout, has a different lifetime.
 auto altered=*owner;altered.streams[0].bytes[0]^=1;auto owner2=std::make_shared<const NorthlightDrawSnapshot::Mesh>(altered);
 cache.beginFrame();budget=Budget{};assert(cache.calculate(program,*owner2,owner2,decl,3,constants,inverse,budget,b)==Status::Valid&&budget.vertices==3);
 // Budget misses never become stale failures. Cache hits spend no evaluation
 // budget, and can be served after another draw has consumed the frame cap.
 cache.clear();budget=Budget{};budget.maxVertices=2;assert(cache.calculate(program,*owner,owner,decl,3,constants,inverse,budget,b)==Status::Budget&&!b.valid&&cache.entries()==0);
 cache.beginFrame();budget=Budget{};assert(cache.calculate(program,*owner,owner,decl,3,constants,inverse,budget,b)==Status::Valid);
 budget=Budget{};budget.maxVertices=0;assert(cache.calculate(program,*owner,owner,decl,3,constants,inverse,budget,b)==Status::Valid&&budget.vertices==0);
 cache.clear();assert(cache.entries()==0&&cache.bytes()==0);budget=Budget{};
 assert(cache.calculate(program,captured,{},decl,3,constants,inverse,budget,b)==Status::Valid&&budget.vertices==3&&cache.entries()==0);
 // Direct rigid shader depends only on referenced constants. Unused c-register
 // changes should hit; geometry/view/declaration changes still cannot hit.
 Program rigid;rigid.major=3;rigid.positionRegister=0;rigid.inputs.push_back({0,0,0});
 Operation mov;mov.code=1;mov.destination=0x800f0000;mov.source[0].token=0x90e40000;rigid.operations.push_back(mov);
 cache.beginFrame();budget=Budget{};assert(cache.calculate(rigid,*owner,owner,decl,3,constants,inverse,budget,b)==Status::Valid);
 float unused[1024];std::memcpy(unused,constants,sizeof constants);unused[900]=200;
 budget=Budget{};cache.beginFrame();assert(cache.calculate(rigid,*owner,owner,decl,3,unused,inverse,budget,b)==Status::Valid&&cache.hits()==1&&budget.vertices==0);
 auto changedRigid=rigid;changedRigid.operations[0].destination|=1u<<20;
 cache.beginFrame();budget=Budget{};assert(cache.calculate(changedRigid,*owner,owner,decl,3,unused,inverse,budget,b)==Status::Valid&&budget.vertices==3);
 // Random small rotations/scales/weights test CPU outputs enclosed by interval.
 for(unsigned iteration=0;iteration<200;++iteration){float k=(int(iteration)-100)*.013f;float varied[1024];std::memcpy(varied,constants,sizeof varied);for(unsigned bone=0;bone<4;++bone){unsigned r=31+3*bone;varied[r*4]=std::cos(k);varied[r*4+1]=std::sin(k);varied[(r+1)*4]=-std::sin(k);varied[(r+1)*4+1]=std::cos(k);}budget.reset();assert(calculate(program,captured,decl,3,varied,inverse,budget,b)==Status::Valid);std::vector<Position> p;assert(worldPositions(program,captured,decl,3,varied,inverse,p));for(auto v:p){float a[]={v.x,v.y,v.z};for(unsigned j=0;j<3;++j)assert(a[j]>=b.low[j]&&a[j]<=b.high[j]);}++tested;}
 auto large=captured;large.vertexCount=4096;large.primitiveCount=1365;large.indexed=false;large.indices.clear();large.streams[0].bytes.resize(4096*20);for(unsigned i=0;i<4096;++i)std::memcpy(large.streams[0].bytes.data()+i*20,captured.streams[0].bytes.data()+(i%3)*20,20);
 double elapsed=0;for(unsigned repetition=0;repetition<20;++repetition){budget=Budget{};auto start=std::chrono::steady_clock::now();assert(calculate(program,large,decl,3,constants,inverse,budget,b)==Status::Valid);elapsed+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
 auto largeOwner=std::make_shared<const NorthlightDrawSnapshot::Mesh>(large);cache.clear();budget=Budget{};
 assert(cache.calculate(program,*largeOwner,largeOwner,decl,3,constants,inverse,budget,b)==Status::Valid);
 const auto cacheStart=std::chrono::steady_clock::now();cache.beginFrame();
 for(unsigned repeat=0;repeat<2000;++repeat){cache.beginFrame();cache.beginFrame();budget=Budget{};assert(cache.calculate(program,*largeOwner,largeOwner,decl,3,constants,inverse,budget,b)==Status::Valid&&budget.vertices==0);}
 double cachedUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-cacheStart).count()/2000;
 assert(cache.hits()==1&&cache.avoidedVertices()==4096);
 for(unsigned i=0;i<600;++i){cache.beginFrame();float varying[16];std::memcpy(varying,inverse,sizeof varying);varying[12]+=i;cache.beginFrame();budget=Budget{};assert(cache.calculate(rigid,*owner,owner,decl,3,constants,varying,budget,b)==Status::Valid);}
 assert(cache.entries()<=512&&cache.bytes()<=4u*1024u*1024u);
 cache.clear();budget=Budget{};assert(cache.calculate(program,*largeOwner,largeOwner,decl,3,constants,inverse,budget,b)==Status::Valid);
 cache.beginFrame();unsigned probes=0;
 while(cache.canLookup()&&probes<200){budget=Budget{};budget.maxVertices=0;assert(cache.calculate(program,*largeOwner,largeOwner,decl,3,constants,inverse,budget,b)==Status::Valid);++probes;}
 assert(!cache.canLookup()&&probes<=128);budget=Budget{};budget.maxVertices=0;
 assert(cache.calculate(program,*largeOwner,largeOwner,decl,3,constants,inverse,budget,b)==Status::Budget&&!b.valid);

 std::printf("{\"status\":\"pass\",\"cases\":%u,\"bounds4096_ms\":%.6f,\"cached_bounds4096_us\":%.6f,\"cache_exact_equality\":true,\"program_constants_camera_decl_geometry_invalidation\":true,\"bounded_memory\":true,\"all_vertices\":true,\"budget_fail_open\":true,\"numeric_enclosure\":true}\n",tested,elapsed/20,cachedUs);
}
'''
with tempfile.TemporaryDirectory(prefix='northlight-replay-bounds-') as tmp:
 p=Path(tmp);(p/'d3d9.h').write_text(stub);(p/'test.cpp').write_text(harness);results=[]
 for flags in (['-O2'],['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']):
  subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-I',str(p),*fp.test_include_flags(),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  results.append(json.loads(subprocess.check_output([str(p/'test')],text=True)))
 report=dict(status='pass',native_tests=results,runtime_gpu_test=False,game_launched=False)
 (fp.output_dir()/'replay-bounds-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
