#include "world_camera.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
using namespace NorthlightWorldCamera;
struct Memory {
 std::map<uintptr_t,unsigned char> bytes;uintptr_t mutateAddress=0;unsigned mutationRead=0,reads=0;
 template<class T>void put(uintptr_t address,const T& value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=p[i];}
 bool read(uintptr_t address,void* output,size_t size){
  if(address==mutateAddress&&++reads==mutationRead){uint32_t bad=0;put(address,bad);}
  for(size_t i=0;i<size;++i)if(!bytes.count(address+i))return false;
  for(size_t i=0;i<size;++i)static_cast<unsigned char*>(output)[i]=bytes.at(address+i);return true;
 }
};
static constexpr uint32_t frame=0x110000,cam=0x220000,dev=0x330000;
static float baseCamera[12]={-10001.25f,-902.5f,61.125f,0,0,1,1,0,0,0,1,0};
static float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
static Memory fixture(unsigned slot=0){Memory m;m.put(0xb7436c,frame);m.put(frame+0x7e20,cam);m.put(cam,uint32_t(0xa1ea54));m.put(cam+8,baseCamera);m.put(cam+0xa0,uint64_t(0));m.put(0xc5df88,dev);m.put(dev,uint32_t(0xa2e718));m.put(dev+0x1af8,slot);m.put(dev+0x1b00+slot*64,identity);return m;}
static bool read(Memory& m,Camera& out,Diagnostics& d){return readCurrent([&](uintptr_t a,void* o,size_t n){return m.read(a,o,n);},1,out,&d);}
static void expectReject(Memory m,Reject why){Camera result;result.camera[0]=1234;Diagnostics d;assert(!read(m,result,d));assert(d.reason==why);assert(result.camera[0]==1234);}
static void matrixTests(){
 for(float angle: {0.f,.6f,1.8f,3.14f})for(float pitch:{-.6f,0.f,.8f})for(float sign:{-1.f,1.f}){
  // Independent basis: rotate a world look direction; side/up are perpendicular.
  float f[3]={std::cos(pitch)*std::cos(angle),std::cos(pitch)*std::sin(angle),std::sin(pitch)};
  float side[3]={-std::sin(angle),std::cos(angle),0};float up[3]={-std::sin(pitch)*std::cos(angle),-std::sin(pitch)*std::sin(angle),std::cos(pitch)};
  float data[12];std::memcpy(data,baseCamera,sizeof data);for(unsigned k=0;k<3;++k){data[3+k]=f[k];data[6+k]=side[k];data[9+k]=up[k];}
  float view[16]={};view[15]=1;for(unsigned k=0;k<3;++k){view[k*4]=side[k];view[k*4+1]=up[k];view[k*4+2]=f[k]*sign;}
  Camera c;assert(decode(data,view,sign,c));
  for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j){double n=0;for(unsigned k=0;k<4;++k)n+=double(c.view[i*4+k])*c.inverseView[k*4+j];assert(std::fabs(n-(i==j?1.:0.))<.003);}
  // camera -> origin; +10 along physical forward -> positive clip-w.
  float point[4]={data[0]+10*f[0],data[1]+10*f[1],data[2]+10*f[2],1};double z=0;for(unsigned k=0;k<4;++k)z+=double(point[k])*c.view[k*4+2];assert(std::fabs(z*sign-10)<.003);
  float light[12]={0,0,1,0,.1f,.2f,.3f,0,1,1,1,0};NorthlightWorldContext::TerrainContext terrain;
  assert(NorthlightWorldContext::decodeTerrain(c.view,light,terrain));assert(NorthlightWorldContext::cameraAgrees(terrain,c.camera,.01f));assert(agreesWithTerrain(c,terrain.view));
  float stale[16];std::memcpy(stale,c.view,sizeof stale);stale[0]+=.01f;assert(!agreesWithTerrain(c,stale));std::memcpy(stale,c.view,sizeof stale);stale[12]+=.2f;assert(!agreesWithTerrain(c,stale));
 }
 Camera unchanged;unchanged.camera[0]=999;Diagnostics why;float data[12],view[16];
 for(unsigned mutation=0;mutation<10;++mutation){std::memcpy(data,baseCamera,sizeof data);std::memcpy(view,identity,sizeof view);float sign=1;
  switch(mutation){case 0:view[1]=.2f;break;case 1:view[12]=1;break;case 2:view[15]=0;break;case 3:view[2]=std::numeric_limits<float>::quiet_NaN();break;case 4:data[3]=std::numeric_limits<float>::infinity();break;case 5:data[0]=100001;break;case 6:data[5]=-1;break;case 7:data[10]=-1;break;case 8:sign=0;break;case 9:data[6]=2;break;}
  assert(!decode(data,view,sign,unchanged,&why));assert(unchanged.camera[0]==999);
 }
}
static void basisTests(){
 // Sky phase: the view stack holds a non-affine sky matrix; the basis reader
 // must still produce the exact camera the world reader produces.
 for(float angle:{0.f,.6f,1.8f,3.14f})for(float pitch:{-.6f,0.f,.8f})for(float sign:{-1.f,1.f}){
  float f[3]={std::cos(pitch)*std::cos(angle),std::cos(pitch)*std::sin(angle),std::sin(pitch)};
  float side[3]={-std::sin(angle),std::cos(angle),0};float up[3]={-std::sin(pitch)*std::cos(angle),-std::sin(pitch)*std::sin(angle),std::cos(pitch)};
  float data[12];std::memcpy(data,baseCamera,sizeof data);for(unsigned k=0;k<3;++k){data[3+k]=f[k];data[6+k]=side[k];data[9+k]=up[k];}
  for(float sideSign:{1.f,-1.f}){
  float view[16]={};view[15]=1;for(unsigned k=0;k<3;++k){view[k*4]=side[k]*sideSign;view[k*4+1]=up[k];view[k*4+2]=f[k]*sign;}
  Memory world=fixture();world.put(cam+8,data);world.put(dev+0x1b00,view);
  float sky[16]={};for(unsigned i=0;i<16;++i)sky[i]=float(i)*.37f;Memory skyPhase=world;skyPhase.put(dev+0x1b00,sky);
  auto reader=[&](Memory& m){return [&](uintptr_t a,void* o,size_t n){return m.read(a,o,n);};};
  Camera a,b;Diagnostics d;
  assert(readCurrent(reader(world),sign,a,&d));
  assert(!readCurrent(reader(skyPhase),sign,b,&d)&&d.reason==Reject::Matrix);
  assert(readCurrentBasis(reader(skyPhase),sign,b,&d));
  assert(sideColumnSign()==sideSign);
  for(unsigned i=0;i<16;++i){assert(std::fabs(a.view[i]-b.view[i])<1e-6f);assert(std::fabs(a.inverseView[i]-b.inverseView[i])<1e-6f);}
  for(unsigned i=0;i<3;++i)assert(a.camera[i]==b.camera[i]);
  }
 }
 // Without a prior view-stack read the handedness is unknown: fail closed.
 {sideColumnSign()=0;Memory m=fixture();Camera c;Diagnostics d;assert(!readCurrentBasis([&](uintptr_t a,void* o,size_t n){return m.read(a,o,n);},1,c,&d)&&d.reason==Reject::Basis);sideColumnSign()=1;}
 // The basis reader keeps every memory validation: wrong camera type, attachment, non-orthonormal basis.
 {Memory m=fixture();m.put(cam,uint32_t(0x1234));Camera c;Diagnostics d;assert(!readCurrentBasis([&](uintptr_t a,void* o,size_t n){return m.read(a,o,n);},1,c,&d)&&d.reason==Reject::CameraType);}
 {Memory m=fixture();m.put(cam+0xa0,uint64_t(1));Camera c;Diagnostics d;assert(!readCurrentBasis([&](uintptr_t a,void* o,size_t n){return m.read(a,o,n);},1,c,&d)&&d.reason==Reject::AttachedCamera);}
 {Memory m=fixture();float data[12];std::memcpy(data,baseCamera,sizeof data);data[3]=.5f;m.put(cam+8,data);Camera c;Diagnostics d;assert(!readCurrentBasis([&](uintptr_t a,void* o,size_t n){return m.read(a,o,n);},1,c,&d)&&d.reason==Reject::Basis);}
}
int main(){basisTests();
 matrixTests();Camera c;Diagnostics d;
 for(unsigned slot=0;slot<4;++slot){auto m=fixture(slot);assert(read(m,c,d));assert(c.camera[0]==baseCamera[0]);assert(c.view[12]==-baseCamera[0]);assert(d.reason==Reject::None);}
 auto m=fixture();m.put(cam,uint32_t(0xa1e864));expectReject(m,Reject::CameraType);
 m=fixture();m.put(dev,uint32_t(0xa2f500));expectReject(m,Reject::DeviceType);
 for(unsigned offset:{0u,4u}){m=fixture();m.put(cam+0xa0+offset,uint32_t(1));expectReject(m,Reject::AttachedCamera);}
 m=fixture();m.put(dev+0x1af8,uint32_t(4));expectReject(m,Reject::Stack);
 for(uint32_t bad:{0u,1u,0xfffffff0u}){m=fixture();m.put(0xb7436c,bad);expectReject(m,Reject::Read);m=fixture();m.put(frame+0x7e20,bad);expectReject(m,Reject::Read);m=fixture();m.put(0xc5df88,bad);expectReject(m,Reject::Read);}
 for(uintptr_t absent:{uintptr_t(cam+0xa0),uintptr_t(cam+8+47),uintptr_t(dev+0x1b00+63),uintptr_t(dev+0x1af8)}){m=fixture();m.bytes.erase(absent);expectReject(m,Reject::Read);}
 for(uintptr_t address:{uintptr_t(0xb7436c),uintptr_t(frame+0x7e20),uintptr_t(0xc5df88)}){m=fixture();m.mutateAddress=address;m.mutationRead=2;expectReject(m,Reject::Changed);}
 m=fixture();m.put(dev+0x1af8,uint32_t(1));m.put(dev+0x1b40,identity);m.mutateAddress=dev+0x1af8;m.mutationRead=2;expectReject(m,Reject::Changed);
 assert(!verifySignatures([](uintptr_t,void*,size_t){return false;}));
 assert(!verifySignatures([](uintptr_t,void* p,size_t n){std::memset(p,0,n);return true;}));
 // A fresh orbit changes view without changing position. Nothing persists from prior reads.
 m=fixture();assert(read(m,c,d));auto first=c;float camera2[12];std::memcpy(camera2,baseCamera,sizeof camera2);camera2[3]=1;camera2[5]=0;camera2[6]=0;camera2[8]=-1;float rotated[16]={0,0,1,0,0,1,0,0,-1,0,0,0,0,0,0,1};m.put(cam+8,camera2);m.put(dev+0x1b00,rotated);assert(read(m,c,d));assert(!agreesWithTerrain(c,first.view));assert(c.camera[0]==first.camera[0]);
 std::puts("{\"status\":\"pass\",\"rotated_camera_cases\":24,\"pointer_and_type_failures\":true,\"same_position_orbit\":true,\"no_stale_output\":true}");
}
