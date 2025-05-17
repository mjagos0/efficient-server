#ifndef ATOMICGRAPH_H
#define ATOMICGRAPH_H

#include <vector>
#include <cstdint>
#include <atomic>
#include <mutex>

class CityGraph {
    public:
        struct Edge {
            uint32_t to;
            std::atomic<uint64_t> lenSum;
            std::atomic<uint32_t> writes;
        
            Edge(uint32_t to_, uint64_t len)
                : to(to_), lenSum(len), writes(1) {}
        
            Edge(Edge&& other) noexcept
                : to(other.to),
                lenSum(other.lenSum.load(std::memory_order_relaxed)),
                writes(other.writes.load(std::memory_order_relaxed)) {}
        
            Edge(const Edge& other)
                : to(other.to),
                lenSum(other.lenSum.load(std::memory_order_relaxed)),
                writes(other.writes.load(std::memory_order_relaxed)) {}
        
            Edge& operator=(Edge&& other) noexcept {
                to = other.to;
                lenSum.store(other.lenSum.load(std::memory_order_relaxed));
                writes.store(other.writes.load(std::memory_order_relaxed));
                return *this;
            }
        
            Edge& operator=(const Edge& other) {
                to = other.to;
                lenSum.store(other.lenSum.load(std::memory_order_relaxed));
                writes.store(other.writes.load(std::memory_order_relaxed));
                return *this;
            }
        };

        
        using EdgeList = std::vector<Edge>;
    
        CityGraph(uint32_t nodeCount);
    
        void addPath(uint32_t from, uint32_t to, uint64_t len);
        const EdgeList& getPaths(uint32_t from) const;
        uint32_t size() const;
        uint64_t o2o(uint32_t from, uint32_t to, std::vector<uint64_t>& dist, 
            std::vector<uint32_t>& gen, 
            uint32_t& currentGen) const;
        uint64_t o2a(uint32_t from, std::vector<uint64_t>& dist, 
            std::vector<uint32_t>& gen, 
            uint32_t& currentGen) const;
        void reset();
    
    private:
        std::vector<EdgeList> adj;
        std::vector<std::mutex> locks;
};
#endif // ATOMICGRAPH_H
