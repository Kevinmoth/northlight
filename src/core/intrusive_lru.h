#pragma once

// Nodes are owned by the caller. Links survive hash-table rehashes; touching
// or removing a resident node never allocates or scans the resident set.
namespace NorthlightIntrusiveLRU {
template<class Node> struct Links {Node* older=nullptr;Node* newer=nullptr;};
template<class Node> class List {
    Node* oldest_=nullptr;Node* newest_=nullptr;
public:
    Node* oldest()const noexcept{return oldest_;}
    void clear()noexcept{oldest_=newest_=nullptr;}
    void remove(Node& node)noexcept{
        if(node.lru.older)node.lru.older->lru.newer=node.lru.newer;else oldest_=node.lru.newer;
        if(node.lru.newer)node.lru.newer->lru.older=node.lru.older;else newest_=node.lru.older;
        node.lru={};
    }
    void append(Node& node)noexcept{
        node.lru.older=newest_;node.lru.newer=nullptr;
        if(newest_)newest_->lru.newer=&node;else oldest_=&node;
        newest_=&node;
    }
    void touch(Node& node)noexcept{if(newest_!=&node){remove(node);append(node);}}
};
}
