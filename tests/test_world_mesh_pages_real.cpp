#include "world_mesh_pages.h"
#include "shadow_terrain.h"
#include "shadow_bounds.h"
#include "world_math.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>

using namespace NorthlightGI;
namespace P=NorthlightWorldMeshPages;
int main(int argc,char** argv) {
    assert(argc==2);const std::string root=argv[1];std::string error;
    struct Fixture{const char* name;Vec3 center;};
    for(const auto& fixture:{Fixture{"Stormwind",{-8833,628,97}},Fixture{"Elwynn",{-9450,-200,65}}}) {
        const auto c=fixture.center;
        int tx=int(std::floor((17066.6666667-c.y)/533.3333333)),ty=int(std::floor((17066.6666667-c.x)/533.3333333));
        std::vector<std::string> nearFiles,farFiles;
        for(int y=ty-2;y<=ty+2;++y)for(int x=tx-2;x<=tx+2;++x){
            auto p=root+"/Azeroth/"+std::to_string(x)+"_"+std::to_string(y)+".fg3";
            if(!std::filesystem::exists(p))continue;
            farFiles.push_back(p);if(std::abs(x-tx)<=1&&std::abs(y-ty)<=1)nearFiles.push_back(p);
        }
        WorldScene local,far;
        assert(loadInstancedScenes(nearFiles,root+"/models",c-Vec3(288,288,320),c+Vec3(288,288,320),local,error));
        const float radius=NorthlightShadowTerrain::Radius;
        assert(loadInstancedScenes(farFiles,root+"/models",c-Vec3(radius,radius,radius),c+Vec3(radius,radius,radius),far,error,1));
        NorthlightWorldMesh::WorldMeshUploadPlan input;
        assert(NorthlightShadowTerrain::build(local,far,c,input,error));
        P::Plan pages;auto start=std::chrono::steady_clock::now();assert(P::build(input,pages,error));
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        assert(input.triangleCount>0);
        size_t originalBatch=0,originalTriangle=0,triangles=0;
        for(const auto& page:pages.pages) {
            assert(page.vertices.size()*sizeof(WorldVertex)<=P::PageVertexByteLimit);
            assert(page.indices.size()*sizeof(uint32_t)<=P::PageIndexByteLimit);
        }
        // Identity transform, normals and UVs are copied bit-for-bit. Therefore
        // identical VS/matrices produce identical vertex outputs, irrespective
        // of source index numbering or page. This does not execute a GPU shader.
        for(const auto& batch:pages.batches) {
            const auto& before=input.batches.at(originalBatch);const auto& page=pages.pages.at(batch.page);
            assert(batch.material==before.material&&batch.terrain==before.terrain&&batch.chunkX==before.chunkX&&batch.chunkY==before.chunkY);
            assert(originalTriangle+batch.count<=before.count);
            for(uint32_t t=0;t<batch.count;++t)for(unsigned k=0;k<3;++k) {
                const uint32_t prior=input.indices.at(before.start+(originalTriangle+t)*3+k);
                const uint32_t index=page.indices.at(batch.start+t*3+k);
                const auto& vertex=page.vertices.at(index);
                assert(index>=batch.minVertex&&index<batch.minVertex+batch.vertexCount);
                assert(std::memcmp(&vertex,&input.source->vertices.at(prior),sizeof(WorldVertex))==0);
                const auto v=vertex.position;
                assert(v.x>=batch.boundsLow.x&&v.x<=batch.boundsHigh.x&&v.y>=batch.boundsLow.y&&v.y<=batch.boundsHigh.y&&v.z>=batch.boundsLow.z&&v.z<=batch.boundsHigh.z);
            }
            originalTriangle+=batch.count;triangles+=batch.count;
            if(originalTriangle==before.count){++originalBatch;originalTriangle=0;}
        }
        assert(triangles==input.triangleCount&&originalBatch==input.batches.size());
        // At low sun/moon directions, every rejected page subbatch must also
        // reject each constituent triangle's AABB. A tighter subdivision must
        // never remove a triangle intersecting the current shadow clip volume.
        unsigned rejected=0,directionalRejected=0;
        for(float degrees:{0.f,2.f,5.f,15.f,38.f})for(float azimuth:{0.f,1.7f,3.1f})for(float extent:{48.f,192.f}) {
            const float angle=degrees*.017453292519943295f;
            Vec3 direction(std::cos(angle)*std::cos(azimuth),std::cos(angle)*std::sin(azimuth),std::sin(angle));
            float matrix[16];NorthlightWorldMath::shadowMatrix(c,direction,extent,matrix);
            for(bool directional:{false,true})for(const auto& b:pages.batches) {
                auto reject=[&](Vec3 lo,Vec3 hi){return directional?NorthlightShadowBounds::directionalClipReject(lo,hi,matrix):NorthlightShadowBounds::clipReject(lo,hi,matrix);};
                if(!reject(b.boundsLow,b.boundsHigh))continue;
                if(directional)++directionalRejected;else ++rejected;
                const auto& page=pages.pages[b.page];
                for(uint32_t t=0;t<b.count;++t) {
                    Vec3 lo=page.vertices[page.indices[b.start+t*3]].position,hi=lo;
                    for(unsigned k=1;k<3;++k){const auto v=page.vertices[page.indices[b.start+t*3+k]].position;
                        lo.x=std::min(lo.x,v.x);lo.y=std::min(lo.y,v.y);lo.z=std::min(lo.z,v.z);
                        hi.x=std::max(hi.x,v.x);hi.y=std::max(hi.y,v.y);hi.z=std::max(hi.z,v.z);}
                    assert(reject(lo,hi));
                }
            }
        }
        const uint64_t originalCpu=input.cpuBytes();
        const size_t expectedOwnerCount=input.source->completePlacements.size();
        const auto localVertexCount=local.vertices.size();
        auto ownedPages=std::make_shared<P::Plan>(std::move(pages));
        assert(P::seal(input,ownedPages,error));assert(!P::validateSealed(input));
        assert(input.completePlacements.size()==expectedOwnerCount);
        const uint64_t sealedCpu=input.cpuBytes();
        assert(local.vertices.size()==localVertexCount);
        std::printf("OWNERSHIP %s original plan %.3f MiB -> pages + retained metadata %.3f MiB (delta %+.3f MiB)\n",fixture.name,double(originalCpu)/1048576.,double(sealedCpu)/1048576.,(double(sealedCpu)-double(originalCpu))/1048576.);
        const auto& sealedPages=*ownedPages;
        std::printf("PASS %s full local + far terrain: vertices %u -> %llu; triangles %u exact; batches %zu -> %zu; pages %zu; worker %.3f ms; pages CPU %.3f MiB; conservative rejected subbatches clip=%u directional=%u at 0/2/5/15/38 degrees\n",fixture.name,input.vertexCount,(unsigned long long)(sealedPages.vertexBytes/sizeof(WorldVertex)),input.triangleCount,input.batches.size(),sealedPages.batches.size(),sealedPages.pages.size(),ms,double(sealedPages.cpuBytes())/1048576.,rejected,directionalRejected);
    }
}
