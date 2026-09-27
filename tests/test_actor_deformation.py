#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Native deformation evaluator tests using actual shader corpus plus bone fixtures."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
import client_fixtures  # the real client programs, from the tester's client
from pathlib import Path
import ast,subprocess,tempfile
HERE=Path(__file__).resolve().parent
stub=next(ast.literal_eval(n.value) for n in ast.parse((HERE/'test_terrain_snapshot.py').read_text()).body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='stub' for t in n.targets))
harness=r'''
#include "actor_deformation.h"
#include <cassert>
#include <cstdio>
using namespace NorthlightActorDeformation;
std::vector<Word> code(unsigned major){
 std::vector<Word> w={major==1?0xfffe0101u:major==2?0xfffe0200u:0xfffe0300u};
 auto op=[&](unsigned opcode,std::initializer_list<Word> args){w.push_back(opcode|(major==1?0:unsigned(args.size())<<24));w.insert(w.end(),args);};
 op(81,{0xa00f0000,0x40400000,0x3f800000,0,0});
 op(31,{0x80000000,0x900f0000});op(31,{0x80000002,0x900f0001});op(31,{0x80000005,0x900f0003});
 if(major==3){op(31,{0x80000000,0xe00f0000});op(31,{0x80000005,0xe00f0001});}
 op(1,{0x80080000,0xa0550000});op(5,{0x80010001,0x90000001,0xa0000000});op(major==1?1:46,{0xb0010000,0x80000001});
 for(unsigned row=0;row<3;++row){if(major==1)op(9,{0x80000000u|(1u<<(16+row)),0xa0e4201fu+row,0x90e40000});else op(9,{0x80000000u|(1u<<(16+row)),0xa0e4201fu+row,0xb0000000,0x90e40000});}
 for(unsigned row=0;row<4;++row)op(9,{(major==3?0xe0000000u:0xc0000000u)|(1u<<(16+row)),0xa0e40002u+row,0x80e40000});
 op(4,{0x80030002,0x90e40003,0xa0e40006,0xa0e40007});op(1,{major==3?0xe0030001u:0xe0030000u,0x80e40002});
 w.push_back(0xffff);return w;
}
int main(int argc,char**argv){
 float constants[1024]={};for(unsigned bone=0;bone<4;++bone){unsigned r=31+3*bone;constants[r*4]=1;constants[(r+1)*4+1]=1;constants[(r+2)*4+2]=1;constants[r*4+3]=100.f*bone;constants[(r+1)*4+3]=10.f*bone;}
 Four input[16]={};input[0]={2,3,4,1};input[1]={1,0,0,0};
 for(unsigned major=1;major<=3;++major){auto w=code(major);Program p;assert(compile(w.data(),w.size(),p));assert(p.inputs.size()==2);
  Program uv;assert(compile(w.data(),w.size(),uv,true));assert(uv.inputs.size()==1&&uv.inputs[0].reg==3&&uv.operations.size()==2);
  input[3]={.1f,.2f,0,1};constants[6*4]=2;constants[6*4+1]=3;constants[7*4]=.25f;constants[7*4+1]=-.5f;
  Four uvResult;assert(evaluate(uv,input,constants,uvResult)&&std::fabs(uvResult[0]-.45f)<1e-6f&&std::fabs(uvResult[1]-.1f)<1e-6f);
  Four out;assert(evaluate(p,input,constants,out));assert(out==Four({102,13,4,1}));
  input[1][0]=1.2f;assert(evaluate(p,input,constants,out)); // 3.6 floors to3 VS1, rounds to4 VS2/3
  if(major==1)assert(out==Four({102,13,4,1}));else assert(out!=Four({102,13,4,1}));input[1][0]=1;
  NorthlightDrawSnapshot::Mesh mesh;mesh.vertexCount=1;mesh.streams[0].stride=20;mesh.streams[0].bytes.resize(20);float xyz[]={2,3,4};std::memcpy(mesh.streams[0].bytes.data(),xyz,12);mesh.streams[0].bytes[16]=1;
  D3DVERTEXELEMENT9 decl[]={{0,0,2,0,0,0},{0,16,5,0,2,0},{0xff,0,17,0,0,0}};
  float inverse[16]={};for(int j=0;j<4;++j)inverse[j*5]=1;inverse[12]=1000;inverse[13]=-20;
  std::vector<Position> positions;assert(worldPositions(p,mesh,decl,3,constants,inverse,positions));assert(positions[0].x==1102&&positions[0].y==-7&&positions[0].z==4);
  // Rotate camera and corresponding bone-to-view matrices: world pose is invariant.
  float rotatedConstants[1024];std::memcpy(rotatedConstants,constants,sizeof constants);
  float row0[]={0,1,0,10},row1[]={-1,0,0,-100};std::memcpy(rotatedConstants+34*4,row0,16);std::memcpy(rotatedConstants+35*4,row1,16);
  inverse[0]=inverse[5]=0;inverse[1]=1;inverse[4]=-1;
  assert(worldPositions(p,mesh,decl,3,rotatedConstants,inverse,positions));assert(positions[0].x==1102&&positions[0].y==-7&&positions[0].z==4);

  mesh.streams[0].bytes[16]=255;assert(!worldPositions(p,mesh,decl,3,constants,inverse,positions)&&positions.empty());
  w[1]=0x1c;assert(!compile(w.data(),w.size(),p));
 }
 Four v;std::uint8_t color[]={10,20,30,255};assert(decodeElement(color,4,v));assert(v[0]==30/255.f&&v[2]==10/255.f&&v[3]==1);
 std::int16_t shortn[]={-32768,32767};assert(decodeElement(reinterpret_cast<std::uint8_t*>(shortn),9,v)&&v[0]==-1&&v[1]==1&&v[3]==1);
 assert(half(0x3c00)==1&&half(0xc000)==-2&&half(1)>0);
 // Actual client SM3 shader: four independently indexed bone rows blended by
 // vertex weights. This proves vector a0 addressing and preprojection slicing.
 if(argc>2){FILE* bf=std::fopen(argv[2],"rb");assert(bf);std::fseek(bf,0,SEEK_END);long size=std::ftell(bf);std::rewind(bf);std::vector<Word> words(size/4);assert(std::fread(words.data(),1,size,bf)==std::size_t(size));std::fclose(bf);
  Program p;assert(compile(words.data(),words.size(),p));Four in[16]={};in[0]={2,3,4,1};in[1]={.1f,.2f,.3f,.4f};in[2]={0,1,2,3};in[3]={0,0,1,0};Four result;
  assert(evaluate(p,in,constants,result));assert(std::fabs(result[0]-202)<.0001f&&std::fabs(result[1]-23)<.0001f&&std::fabs(result[2]-4)<.0001f);
 }
 if(argc>1){FILE*f=std::fopen(argv[1],"rb");assert(f);Word bytes;unsigned total=0,ok=0,uv=0,count[4]={};
 while(std::fread(&bytes,4,1,f)==1){assert(bytes%4==0&&bytes<1000000);std::vector<Word>w(bytes/4);assert(std::fread(w.data(),1,bytes,f)==bytes);Program p;++total;if(compile(w.data(),w.size(),p,true))++uv;if(compile(w.data(),w.size(),p)){++ok;++count[p.major];}}
 std::fclose(f);std::printf("UV programs %u/%u\n",uv,total);std::printf("Corpus %u/%u supported model prefixes: SM1=%u SM2=%u SM3=%u\n",ok,total,count[1],count[2],count[3]);assert(ok==9090&&total==9812&&uv==9812);}
 std::puts("CPU skinning: real bone offsets, VS1 floor / VS2+ round, declaration decode, inverse camera, bounds gates passed");
}
'''
with tempfile.TemporaryDirectory(prefix='northlight-actor-deform-') as tmp:
 p=Path(tmp);(p/'d3d9.h').write_text(stub);(p/'test.cpp').write_text(harness)
 for flags in (['-O2'],['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']):
  subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-I',str(p),*fp.test_include_flags(),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  args=[str(p/'test')];corpus=fp.shader_corpus()
  if corpus and client_fixtures.shaders_available():args.extend([str(corpus),str(client_fixtures.four_bone_vs3())])
  else:print('SKIP sub-case: captured shader corpus (needs NORTHLIGHT_SHADER_CORPUS, a client and StormLib)')
  subprocess.run(args,check=True)
