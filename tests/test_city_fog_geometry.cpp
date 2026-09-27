#include "regional_fog.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
using namespace NorthlightGI;
int main(int argc,char** argv){
    assert(argc==2);std::string root=argv[1];
    struct Fixture{const char* map;Vec3 camera;unsigned zone;float low,high;};
    const Fixture fixtures[]={{"Azeroth",{-8833,628,97},1519,90,98},{"Kalimdor",{1500,-4415,32},1637,20,27}};
    for(const auto& fixture:fixtures){
        const auto c=fixture.camera;int tx=int(std::floor((17066.6666667-c.y)/533.3333333)),ty=int(std::floor((17066.6666667-c.x)/533.3333333));
        std::vector<std::string> paths;
        for(int y=ty-1;y<=ty+1;++y)for(int x=tx-1;x<=tx+1;++x){auto p=root+"/"+fixture.map+"/"+std::to_string(x)+"_"+std::to_string(y)+".fg3";if(std::filesystem::exists(p))paths.push_back(p);}
        WorldScene scene;std::string error;
        assert(loadInstancedScenes(paths,root+"/models",c-Vec3(288,288,320),c+Vec3(288,288,320),scene,error));
        auto region=NorthlightRegionalFog::loadRegion(root+"/fog",fixture.map,c.x,c.y);
        assert(NorthlightRegionalFog::zoneAt(region,c.x,c.y)==fixture.zone);
        auto begin=std::chrono::steady_clock::now();
        auto fixed=NorthlightRegionalFog::buildField(scene,region,c.x,c.y);
        auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        // Suppressing category metadata reproduces the terrain-only old policy.
        for(auto& material:scene.materials)material.wmo=false;
        auto legacy=NorthlightRegionalFog::buildField(scene,region,c.x,c.y);
        const auto& before=legacy.texels[32*64+32];const auto& after=fixed.texels[32*64+32];
        assert(before.height==0&&after.height>0&&after.ground>before.ground);
        assert(after.ground>fixture.low&&after.ground<fixture.high);
        assert(fixed.airCells>legacy.airCells&&fixed.citySurfaceCells>0);
        assert(c.z>after.ground&&c.z-after.ground<12);
        std::cout<<"city="<<fixture.zone<<" oldGround="<<before.ground<<" newGround="<<after.ground
                 <<" oldAir="<<legacy.airCells<<" newAir="<<fixed.airCells<<" cityCells="<<fixed.citySurfaceCells
                 <<" fieldBuildMs="<<milliseconds<<" triangles="<<scene.triangles.size()<<"\n";
    }
    std::cout<<"PASS real city geometry; no game or GPU executed\n";
}
