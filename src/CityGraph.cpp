#include "CityGraph.h"

#include <queue>

CityGraph::CityGraph(uint32_t nodeCount)
    : adj(nodeCount), locks(nodeCount) { }

void CityGraph::addPath(uint32_t from, uint32_t to, uint64_t len) {
    std::lock_guard<std::mutex> lock(locks[from]);
    EdgeList& list = adj[from];

    for (Edge& edge : list) {
        if (edge.to == to) {
            edge.lenSum.fetch_add(len, std::memory_order_relaxed);
            edge.writes.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }

    list.emplace_back(to, len);
}

const CityGraph::EdgeList& CityGraph::getPaths(uint32_t from) const {
    return adj[from];
}

uint32_t CityGraph::size() const {
    return static_cast<uint32_t>(adj.size());
}

void CityGraph::reset() {
    for (auto& edges : adj) {
        edges.clear();
    }
}

uint64_t CityGraph::o2o(uint32_t from, uint32_t to, std::vector<uint64_t>& dist, 
    std::vector<uint32_t>& gen, 
    uint32_t& currentGen) const {
    const uint32_t distSize = static_cast<uint32_t>(dist.size()); // Cache dist.size()

    using QueueItem = std::pair<uint64_t, uint32_t>;
    auto cmp = std::greater<QueueItem>();
    std::priority_queue<QueueItem, std::vector<QueueItem>, decltype(cmp)> pq(cmp);

    dist[from] = 0;
    gen[from] = currentGen;
    pq.emplace(0, from);

    while (!pq.empty()) {
        auto [currDist, u] = pq.top();
        pq.pop();

        if (u == to) {
            ++currentGen;
            return currDist;
        }

        if (currDist > dist[u]) continue;

        for (const Edge& edge : getPaths(u)) {
            uint32_t v = edge.to;
            if (v >= distSize) continue;

            uint64_t writes = edge.writes.load(std::memory_order_relaxed);
            uint64_t lenSum = edge.lenSum.load(std::memory_order_relaxed);
            uint64_t avgLen = (writes != 0) ? (lenSum / writes) : lenSum;
            uint64_t newDist = dist[u] + avgLen;

            if (gen[v] != currentGen || newDist < dist[v]) {
                dist[v] = newDist;
                gen[v] = currentGen;
                pq.emplace(newDist, v);
            }
        }
    }

    ++currentGen;
    return -1;
}

uint64_t CityGraph::o2a(uint32_t from, std::vector<uint64_t>& dist, 
    std::vector<uint32_t>& gen, 
    uint32_t& currentGen) const {
    const uint32_t n = size();
    const uint32_t distSize = static_cast<uint32_t>(dist.size()); // Cache dist.size()

    using QueueItem = std::pair<uint64_t, uint32_t>;
    auto cmp = std::greater<QueueItem>();
    std::priority_queue<QueueItem, std::vector<QueueItem>, decltype(cmp)> pq(cmp);

    dist[from] = 0;
    gen[from] = currentGen;
    pq.emplace(0, from);

    while (!pq.empty()) {
        auto [currDist, u] = pq.top();
        pq.pop();

        if (currDist > dist[u]) continue;

        for (const Edge& edge : getPaths(u)) {
            uint32_t v = edge.to;
            if (v >= distSize) continue;

            uint64_t writes = edge.writes.load(std::memory_order_relaxed);
            uint64_t lenSum = edge.lenSum.load(std::memory_order_relaxed);
            uint64_t avgLen = (writes != 0) ? (lenSum / writes) : lenSum;
            uint64_t newDist = dist[u] + avgLen;

            if (gen[v] != currentGen || newDist < dist[v]) {
                dist[v] = newDist;
                gen[v] = currentGen;
                pq.emplace(newDist, v);
            }
        }
    }

    uint64_t total = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (i != from && gen[i] == currentGen) {
            total += dist[i];
        }
    }

    ++currentGen;
    return total;
}
