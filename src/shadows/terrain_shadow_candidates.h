#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <set>
#include <utility>
#include <stdexcept>
#include <vector>

// Reuse only membership work within one render call. Batches and both chunk
// sets must remain unchanged from prepare through the last directional pass.
// Only scratch capacity survives: no eligibility, bounds or resource refs do.
namespace NorthlightTerrainCandidates {
// 0.3.175 (S2): the live terrain chunks of one frame. The ordered set (iteration, size, hash) plus a
// flat bitmap of the global ADT chunk grid (64 tiles x 16 chunks per axis, 128 KiB, allocated at the
// first insertion) for O(1) membership; coordinates outside the grid (-1: not one chunk) use the set.
class ChunkSet {
    std::set<std::pair<int,int>> set_;std::vector<std::uint64_t> bits_;
    static constexpr int Grid=1024;
    static bool inGrid(int x,int y){return x>=0&&y>=0&&x<Grid&&y<Grid;}
    static std::size_t bit(int x,int y){return std::size_t(y)*Grid+std::size_t(x);}
public:
    bool emplace(int x,int y){
        if(inGrid(x,y)&&bits_.empty())bits_.assign(std::size_t(Grid)*Grid/64,0); /* before the set: a failed allocation changes nothing */
        const bool added=set_.emplace(x,y).second;if(added&&inGrid(x,y)){const auto i=bit(x,y);bits_[i>>6]|=std::uint64_t(1)<<(i&63);}return added;}
    bool contains(int x,int y)const{if(!inGrid(x,y))return set_.count({x,y})!=0;if(bits_.empty())return false;const auto i=bit(x,y);return (bits_[i>>6]>>(i&63))&1;}
    std::size_t count(const std::pair<int,int>& c)const{return contains(c.first,c.second)?1:0;}
    void clear(){if(!bits_.empty())for(const auto& c:set_)if(inGrid(c.first,c.second)){const auto i=bit(c.first,c.second);bits_[i>>6]&=~(std::uint64_t(1)<<(i&63));}set_.clear();}
    std::size_t size()const{return set_.size();}
    bool empty()const{return set_.empty();}
    auto begin()const{return set_.begin();}
    auto end()const{return set_.end();}
};
struct Stats {
    std::uint64_t scanned=0,membershipChecks=0,reusedMembershipChecks=0;
    unsigned passes=0,fallbackPasses=0;
};
template<class Allocator> class Selection;
template<class Allocator=std::allocator<std::uint32_t>> class Scratch {
    friend class Selection<Allocator>;
    std::vector<std::uint32_t,Allocator> indices_;
    bool leased_=false;
public:
    Scratch()=default;
    explicit Scratch(const Allocator& allocator):indices_(allocator){}
    Scratch(const Scratch&)=delete;Scratch& operator=(const Scratch&)=delete;
    bool empty()const{return indices_.empty();}
    std::size_t capacity()const{return indices_.capacity();}
};
template<class Allocator=std::allocator<std::uint32_t>> class Selection {
    Scratch<Allocator>& scratch_;
    std::vector<std::uint32_t,Allocator>& indices_;
    bool ownsScratch_=false;
    bool ready_=false,sampled_=false;
    std::uint64_t preparedChecks_=0;
    unsigned completedPasses_=0;
    const void* source_=nullptr;
    const void* fixed_=nullptr;
    const void* live_=nullptr;
    std::size_t sourceSize_=0;
    Stats stats_;
    template<class Batch,class Fixed,class Live> bool eligible(const Batch& batch,const Fixed& fixed,const Live& live){
        if(sampled_)++stats_.scanned;
        if(!batch.terrain)return false;
        if(sampled_)++stats_.membershipChecks;
        if(fixed.count({batch.chunkX,batch.chunkY}))return false;
        if(sampled_)++stats_.membershipChecks;
        return !live.count({batch.chunkX,batch.chunkY});
    }
public:
    static constexpr std::size_t MaxCandidates=65536; // 256 KiB requested index payload
    explicit Selection(Scratch<Allocator>& scratch):scratch_(scratch),indices_(scratch.indices_){
        if(!scratch.leased_){ownsScratch_=true;scratch.leased_=true;indices_.clear();}
    }
    Selection(const Selection&)=delete;Selection& operator=(const Selection&)=delete;
    ~Selection(){if(ownsScratch_){indices_.clear();scratch_.leased_=false;}}
    template<class Batches,class Fixed,class Live> void prepare(const Batches& batches,const Fixed& fixed,const Live& live,bool sampled=false){
        ready_=false;sampled_=sampled;stats_={};completedPasses_=0;preparedChecks_=0;
        if(!ownsScratch_)return; // Nested render: original scan; leave the outer list untouched.
        indices_.clear();
        source_=batches.data();sourceSize_=batches.size();fixed_=&fixed;live_=&live;
        if(sourceSize_>UINT32_MAX)return;
        try{
            for(std::size_t index=0;index<batches.size();++index){
                if(!eligible(batches[index],fixed,live))continue;
                if(indices_.size()==MaxCandidates){indices_.clear();return;}
                if(indices_.empty()&&indices_.capacity()<std::min(batches.size(),MaxCandidates))
                    indices_.reserve(std::min(batches.size(),MaxCandidates));
                indices_.push_back(static_cast<std::uint32_t>(index));
            }
        }catch(const std::bad_alloc&){indices_.clear();return;}
        catch(const std::length_error&){indices_.clear();return;} // Optional allocation cannot remove a caster.
        ready_=true;preparedChecks_=stats_.membershipChecks;
    }
    // 0.3.175 (S2): the same selection from `list`: the indices, in batch order, of the terrain
    // batches outside `fixed` (per mesh generation, built by the renderer from these batches and this
    // fixed set). Only the live membership is tested; the candidates and their order are prepare()'s.
    template<class Batches,class List,class Fixed,class Live> void prepareListed(const Batches& batches,const List& list,const Fixed& fixed,const Live& live,bool sampled=false){
        ready_=false;sampled_=sampled;stats_={};completedPasses_=0;preparedChecks_=0;
        if(!ownsScratch_)return;
        indices_.clear();
        source_=batches.data();sourceSize_=batches.size();fixed_=&fixed;live_=&live;
        if(sourceSize_>UINT32_MAX)return;
        try{
            for(const auto index:list){if(index>=batches.size()){indices_.clear();return;}const auto& batch=batches[index];
                if(sampled_){++stats_.scanned;++stats_.membershipChecks;}
                if(live.count({batch.chunkX,batch.chunkY}))continue;
                if(indices_.size()==MaxCandidates){indices_.clear();return;}
                if(indices_.empty()&&indices_.capacity()<std::min<std::size_t>(list.size(),MaxCandidates))indices_.reserve(std::min<std::size_t>(list.size(),MaxCandidates));
                indices_.push_back(static_cast<std::uint32_t>(index));}
        }catch(const std::bad_alloc&){indices_.clear();return;}
        catch(const std::length_error&){indices_.clear();return;}
        ready_=true;preparedChecks_=stats_.membershipChecks;
    }
    template<class Batches,class Fixed,class Live,class Draw> bool forEach(const Batches& batches,const Fixed& fixed,const Live& live,Draw&& draw){
        if(sampled_)++stats_.passes;
        if(ready_&&source_==batches.data()&&sourceSize_==batches.size()&&fixed_==&fixed&&live_==&live){
            for(auto index:indices_)if(!draw(batches[index]))return false;
            if(completedPasses_++&&sampled_)stats_.reusedMembershipChecks+=preparedChecks_;
        }else{
            if(sampled_)++stats_.fallbackPasses;
            for(const auto& batch:batches)if(eligible(batch,fixed,live)&&!draw(batch))return false;
        }
        return true;
    }
    bool ready()const{return ready_;}
    std::size_t candidates()const{return ready_?indices_.size():0;}
    std::size_t bytes()const{return indices_.capacity()*sizeof(std::uint32_t);}
    const Stats& stats()const{return stats_;}
};
}
