#pragma once
#include "static_shadow_scene.h"
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace StaticShadow {
/* 0.3.137: the exact local-geometry owner set is built on the GI worker next
   to its immutable mesh plan. The render thread only diffs two prepared maps
   (linear merge, no allocation) instead of building ~3000 string-keyed nodes
   in the commit and static phases. Content and ownerChanged order are the
   same as the 0.3.136 per-frame construction. */
inline constexpr bool PreparedCoveredOwners=true;
using OwnerKey=std::tuple<uint32_t,uint64_t,std::string>;
using OwnerMap=std::map<OwnerKey,std::vector<Placement>>;
struct OwnerSet {size_t placements=0;OwnerMap owners;};
inline bool sameOwner(const Placement& a,const Placement& b){return a.uid==b.uid&&a.category==b.category&&a.modelKey==b.modelKey&&std::memcmp(a.matrix,b.matrix,sizeof(a.matrix))==0&&a.translation.x==b.translation.x&&a.translation.y==b.translation.y&&a.translation.z==b.translation.z;}
/* Exact 0.3.136 setCoveredPlacements grouping: per key, first occurrence
   order, duplicates (samePlacement) dropped. */
inline void addOwners(OwnerMap& map,const std::vector<Placement>& placements){
    for(const auto& p:placements){auto& values=map[OwnerKey{p.category,p.uid,p.modelKey}];bool duplicate=false;for(const auto& old:values)if(sameOwner(old,p)){duplicate=true;break;}if(!duplicate)values.push_back(p);}
}
inline std::shared_ptr<const OwnerSet> makeOwners(std::vector<Placement> placements){
    auto set=std::make_shared<OwnerSet>();set->placements=placements.size();addOwners(set->owners,placements);return set;
}
/* Calls changed(p) for every p of `from` (map order, then vector order) that
   has no samePlacement match under the same key in `to`: the order of the
   0.3.136 find-per-element walk, with one merge cursor into `to`. */
template<class Changed> size_t ownerDifferences(const OwnerMap& from,const OwnerMap& to,Changed changed){
    size_t count=0;auto cursor=to.begin();
    for(const auto& pair:from){
        while(cursor!=to.end()&&cursor->first<pair.first)++cursor;
        const bool present=cursor!=to.end()&&!(pair.first<cursor->first);
        for(const auto& p:pair.second){bool same=false;if(present)for(const auto& q:cursor->second)if(sameOwner(p,q)){same=true;break;}if(!same){++count;changed(p);}}
    }
    return count;
}
} // namespace StaticShadow
