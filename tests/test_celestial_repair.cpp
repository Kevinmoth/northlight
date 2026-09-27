#include "celestial_disc.h"
#include "celestial_profiles.h"
#include <cassert>
#include <iostream>
using namespace NorthlightCelestialDisc;
int main(int argc,char** argv){
    assert(argc==2);unsigned cases=0;
    const auto quantize=[](float z){return float(std::round(double(z)*16777215)/16777215);};
    for(float worldMax:{.7f,.94f,.99f})for(float skyMin:{worldMax,.9990234375f}){
        float depth=0;assert(skyDepth(skyMin,1,worldMax,depth));
        assert(repairVisible(quantize(depth),depth));
        assert(!repairVisible(worldMax,depth)&&repairVisible(1,depth)); // cleared/farther sky
        assert(!repairVisible(depth-4.f/16777215,depth)); // nearer sky mountain
        assert(!repairVisible(NAN,depth));
        // A native additive layer contaminates only half the moon. A late
        // opaque surface restore must make both halves identical while leaves
        // and roofs remain exactly the original foreground colors.
        for(int i=0;i<100;++i){
            float original=.3f+i*.003f,contaminated=std::min(original+.5f,1.f);
            float repaired=repairVisible(quantize(depth),depth)?original:contaminated;
            assert(repaired==original);
            float foreground=.08f;assert((repairVisible(.8f*worldMax,depth)?original:foreground)==foreground);++cases;
        }
    }
    NorthlightCelestialProfiles::Table profiles;std::string error;
    assert(NorthlightCelestialProfiles::load(std::string(argv[1])+"/celestial-profiles.ini",profiles,error));
    Texture moon;assert(loadTexture(std::string(argv[1])+"/world-cache/celestial/moon.fct",moon));
    unsigned oldClipped=0,opaque=0;float low=1,high=0;
    for(std::size_t i=0;i<moon.rgba.size();i+=4)if(moon.rgba[i+3]>250){
        ++opaque;float x=moon.rgba[i]/255.f;oldClipped+=x*1.6f>=1;
        for(const auto* p:{&profiles.at("",0),&profiles.at("Azeroth",85),&profiles.at("Kalimdor",440)}){
            for(unsigned c=0;c<3;++c)assert(moon.rgba[i+c]/255.f*p->gain[1]<1);
            float brightness=x*p->gain[1];assert(brightness<1);low=std::min(low,brightness);high=std::max(high,brightness);
            // The halo's contribution on an opaque moon is zero at every gain.
            assert(.31f*(1-1.f)==0);
        }
    }
    assert(oldClipped>opaque/3&&high-low>.4f);
    std::cout<<"PASS repair: "<<cases<<" depth/foreground cases; "<<opaque<<" original moon pixels, old clipped="<<oldClipped<<", new clipped=0, texture range="<<low<<".."<<high<<"\n";
}
