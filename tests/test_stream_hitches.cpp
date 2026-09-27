/* 0.3.137 render-thread hitch fixes: prepared covered-owner diff parity with
   the 0.3.136 setCoveredPlacements walk, and bounded CPU retirement. */
#include "static_shadow_owners.h"
#include "cpu_retirement.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <random>
#include <thread>
using namespace StaticShadow;
/* Verbatim 0.3.136 algorithm; `changed` records ownerChanged calls. */
struct Reference {
    OwnerMap covered_;
    std::vector<Placement> calls;
    void set(const std::vector<Placement>& placements){
        std::map<OwnerKey,std::vector<Placement>> next;
        for(const auto& p:placements){auto& values=next[OwnerKey{p.category,p.uid,p.modelKey}];bool duplicate=false;for(const auto& old:values)if(sameOwner(old,p)){duplicate=true;break;}if(!duplicate)values.push_back(p);}
        bool equal=next.size()==covered_.size();
        if(equal)for(const auto& pair:next){auto old=covered_.find(pair.first);if(old==covered_.end()||old->second.size()!=pair.second.size()){equal=false;break;}for(const auto& p:pair.second){bool found=false;for(const auto& q:old->second)if(sameOwner(p,q)){found=true;break;}if(!found){equal=false;break;}}if(!equal)break;}
        if(!equal){
            auto differences=[&](const auto& from,const auto& to){for(const auto& pair:from)for(const auto& p:pair.second){auto found=to.find(pair.first);bool same=false;if(found!=to.end())for(const auto& q:found->second)if(sameOwner(p,q)){same=true;break;}if(!same)calls.push_back(p);}};
            differences(covered_,next);differences(next,covered_);covered_=std::move(next);
        }
    }
};
/* Production shape of GpuCache::setCoveredOwners. */
struct Prepared {
    std::shared_ptr<const OwnerMap> covered_;
    std::vector<Placement> calls;
    void set(std::shared_ptr<const OwnerMap> next){
        if(next==covered_)return;
        static const OwnerMap none;
        const OwnerMap& from=covered_?*covered_:none;const OwnerMap& to=next?*next:none;
        const size_t changes=ownerDifferences(from,to,[&](const Placement& p){calls.push_back(p);})+ownerDifferences(to,from,[&](const Placement& p){calls.push_back(p);});
        if(!changes&&covered_)return;
        covered_=std::move(next);
    }
    bool covered(const Placement& p)const{if(!covered_)return false;auto f=covered_->find(OwnerKey{p.category,p.uid,p.modelKey});if(f==covered_->end())return false;for(const auto& q:f->second)if(sameOwner(q,p))return true;return false;}
};
static bool covered(const OwnerMap& m,const Placement& p){auto f=m.find(OwnerKey{p.category,p.uid,p.modelKey});if(f==m.end())return false;for(const auto& q:f->second)if(sameOwner(q,p))return true;return false;}
static bool same(const std::vector<Placement>& a,const std::vector<Placement>& b){
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i)if(!sameOwner(a[i],b[i])||a[i].low.x!=b[i].low.x||a[i].high.z!=b[i].high.z)return false;
    return true;
}
struct Probe {std::atomic<int>& alive;~Probe(){--alive;}};
int main(){
    std::mt19937 rng(137);
    auto placement=[&]{Placement p;p.uid=rng()%40;p.category=rng()%3;p.modelKey="m"+std::to_string(rng()%12);
        p.translation={float(rng()%4),float(rng()%3),0};p.matrix[0]=float(1+rng()%2);p.low={float(rng()%5),0,0};p.high={0,0,float(rng()%5)};return p;};
    size_t sequences=0,calls=0,probes=0;
    for(unsigned run=0;run<400;++run){
        Reference ref;Prepared fast;std::vector<Placement> current;
        for(unsigned step=0;step<25;++step){
            /* Mutate: keep, remove, duplicate, move, add. Empty sets included. */
            std::vector<Placement> next;
            for(const auto& p:current){const unsigned r=rng()%10;if(r==0)continue;next.push_back(p);if(r==1)next.push_back(p);if(r==2){next.back().translation.x+=1;}}
            const unsigned adds=step%7==6?0:rng()%30;for(unsigned i=0;i<adds;++i)next.push_back(placement());
            if(step%11==10)next.clear();
            std::shuffle(next.begin(),next.end(),rng);
            if(step%5==4){fast.set(fast.covered_);assert(fast.calls.empty());continue;} /* same pointer: no work */
            ref.set(next);
            auto set=makeOwners(next);
            fast.set(next.empty()&&step%2?nullptr:std::shared_ptr<const OwnerMap>(set,&set->owners));
            assert(same(ref.calls,fast.calls));calls+=ref.calls.size();ref.calls.clear();fast.calls.clear();
            for(unsigned i=0;i<60;++i){auto p=i<next.size()?next[i]:placement();assert(covered(ref.covered_,p)==fast.covered(p));++probes;}
            current=next;++sequences;
        }
    }
    /* Grouping parity of makeOwners itself. */
    for(unsigned i=0;i<200;++i){std::vector<Placement> v;for(unsigned j=0;j<rng()%50;++j){v.push_back(placement());if(j%3==0)v.push_back(v.back());}
        Reference r;r.set(v);auto m=makeOwners(v);assert(m->placements==v.size()&&m->owners.size()==r.covered_.size());
        for(const auto& pair:r.covered_){auto f=m->owners.find(pair.first);assert(f!=m->owners.end()&&same(f->second,pair.second));}}
    /* Retirement: oversized accepted only when nothing is charged. */
    std::atomic<int> alive{0};
    {   /* 0.3.137 default (switch off): oversized refused exactly as 0.3.136. */
        NorthlightStreaming::CpuRetirement standard;
        ++alive;auto refused=std::shared_ptr<Probe>(new Probe{alive});
        assert(!NorthlightStreaming::RetireOversizedWhenIdle||standard.retire(refused,size_t(512)<<20));
        if(!NorthlightStreaming::RetireOversizedWhenIdle){assert(!standard.retire(refused,size_t(512)<<20)&&refused&&standard.stats().refused==1);
            assert(standard.retire(refused,size_t(128)<<20)&&!refused);}
        while(alive)std::this_thread::yield();
    }
    {
        NorthlightStreaming::CpuRetirement queue(true); /* switch-on behaviour */
        ++alive;auto big=std::shared_ptr<Probe>(new Probe{alive});
        assert(queue.retire(big,(size_t(512)<<20))&&!big);
        auto stats=queue.stats();assert(stats.accepted==1&&stats.oversized==1);
        ++alive;auto second=std::shared_ptr<Probe>(new Probe{alive});
        while(alive!=1)std::this_thread::yield();
        for(int i=0;i<10000000&&!queue.retire(second,size_t(300)<<20);++i)std::this_thread::yield(); /* charge drains after destruction */
        assert(!second);while(alive)std::this_thread::yield();
        /* Backlog: refused items park, retry later, full backlog frees here. */
        NorthlightStreaming::RetirementBacklog backlog;
        std::vector<std::shared_ptr<Probe>> items;
        for(int i=0;i<6;++i){++alive;items.push_back(std::shared_ptr<Probe>(new Probe{alive}));}
        std::mutex gate;std::unique_lock<std::mutex> hold(gate);
        ++alive;auto blocker=std::shared_ptr<Probe>(new Probe{alive});
        struct Slow {std::shared_ptr<Probe> p;std::mutex& m;~Slow(){std::lock_guard<std::mutex> l(m);}};
        auto slow=std::shared_ptr<Slow>(new Slow{blocker,gate});blocker.reset();
        assert(queue.retire(slow,size_t(100)<<20)); /* in flight, 100 MiB charged */
        for(auto& item:items)backlog.retire(queue,item,size_t(200)<<20); /* each refused: 4 parked, 2 synchronous */
        assert(backlog.parked()==4&&backlog.deferred==4&&backlog.synchronous==2);
        for(auto& item:items)assert(!item);
        hold.unlock();
        for(int i=0;i<1000&&backlog.parked();++i){backlog.retry(queue);std::this_thread::sleep_for(std::chrono::milliseconds(5));} /* one retry per frame */
        assert(backlog.parked()==0&&backlog.synchronous==2);
        /* Age bound: a permanently refused item is freed after MaxRetries. */
        std::unique_lock<std::mutex> hold2(gate);
        ++alive;auto blocker2=std::shared_ptr<Probe>(new Probe{alive});
        auto slow2=std::shared_ptr<Slow>(new Slow{blocker2,gate});blocker2.reset();
        while(!queue.retire(slow2,size_t(100)<<20))std::this_thread::yield();
        ++alive;auto aged=std::shared_ptr<Probe>(new Probe{alive});backlog.retire(queue,aged,size_t(200)<<20);assert(backlog.parked()==1);
        /* Age bound is wall time, not frames: many fast retries keep it parked. */
        const auto t0=std::chrono::steady_clock::now();
        for(unsigned i=0;i<1000;++i){backlog.retry(queue,t0);assert(backlog.parked()==1);}
        backlog.retry(queue,t0+std::chrono::milliseconds(249));assert(backlog.parked()==1);
        const int before=alive;backlog.retry(queue,t0+std::chrono::milliseconds(1000));assert(backlog.parked()==0&&backlog.synchronous==3&&alive==before-1);
        /* 0.3.138 generation owners: accepted despite the byte cap (reaper is
           busy with 100 MiB here), never parked; only a full queue frees here. */
        ++alive;auto generation=std::shared_ptr<Probe>(new Probe{alive});const int live=alive;
        backlog.retireOrFree(queue,generation,size_t(200)<<20);assert(!generation&&alive==live&&backlog.generationFrees==0&&backlog.parked()==0);
        std::vector<std::shared_ptr<std::vector<char>>> fillers;size_t queued=0;
        for(int i=0;i<32;++i){auto f=std::make_shared<std::vector<char>>(1);if(queue.retireGeneration(f,0))++queued;else fillers.push_back(f);}
        assert((queued==15||queued==14)&&fillers.size()==32-queued); /* 16 slots: slow2 in flight (dequeued), generation queued */
        ++alive;auto overflow=std::shared_ptr<Probe>(new Probe{alive});const int full=alive;
        backlog.retireOrFree(queue,overflow,size_t(200)<<20);assert(!overflow&&alive==full-1&&backlog.generationFrees==1&&backlog.parked()==0);
        hold2.unlock();while(alive)std::this_thread::yield(); /* reaper done before gate dies */
    }
    assert(alive==0);
    std::cout<<"stream hitches: "<<sequences<<" owner-set transitions, "<<calls<<" ownerChanged calls in identical order, "<<probes<<" membership probes; oversized idle retirement and time-bounded small-set backlog, generation owners queued without byte cap, full-queue synchronous fallback passed\n";
}
