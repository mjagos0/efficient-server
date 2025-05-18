#include <array>
#include <mutex>

#define UNASSIGNED 0
#define MAX_NODES 120000
#define MAX_EDGES 5

struct Edge {
    Edge() : to(UNASSIGNED), sum(0), updates(0) { }

    uint32_t to;
    uint32_t sum;
    uint32_t updates;
};

struct FlatHeap {
    using Node = std::pair<uint32_t, uint32_t>;
    std::vector<Node> heap;

    void push(Node item) {
        heap.push_back(item);
        siftUp(heap.size() - 1);
    }

    Node pop() {
        std::swap(heap[0], heap.back());
        Node result = heap.back();
        heap.pop_back();
        siftDown(0);
        return result;
    }

    bool empty() const {
        return heap.empty();
    }

    void clear() {
        heap.clear();
    }

    void reserve(size_t n) {
        heap.reserve(n);
    }

    void siftUp(size_t i) {
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (heap[i].first >= heap[parent].first) break;
            std::swap(heap[i], heap[parent]);
            i = parent;
        }
    }

    void siftDown(size_t i) {
        size_t size = heap.size();
        while (true) {
            size_t left = 2 * i + 1;
            size_t right = 2 * i + 2;
            size_t smallest = i;

            if (left < size && heap[left].first < heap[smallest].first) smallest = left;
            if (right < size && heap[right].first < heap[smallest].first) smallest = right;
            if (smallest == i) break;

            std::swap(heap[i], heap[smallest]);
            i = smallest;
        }
    }
};

struct DijkstraContext {
    std::array<uint32_t, MAX_NODES> dist;
    std::array<uint32_t, MAX_NODES> timestamps;
    FlatHeap heap;
    uint32_t generation = 1;

    DijkstraContext() {
        heap.reserve(200000);
    }
};

struct CityGraph {
    std::array<Edge, MAX_NODES * MAX_EDGES> edges;
    std::array<std::mutex, MAX_NODES> nodeLocks;

    inline uint32_t getEdgeOffset(uint32_t nodeId, uint32_t edgeIdx) const {
        return nodeId * MAX_EDGES + edgeIdx;
    }

    void addPath(uint32_t from, uint32_t to, uint32_t len) {
        std::lock_guard<std::mutex> lock(nodeLocks[from]);

        for (uint32_t i = 0; i != MAX_EDGES; ++i) {
            auto& edge = edges[getEdgeOffset(from, i)];

            if (edge.to == to) {
                edge.sum += len;
                edge.updates++;
                return;
            } else if (edge.to == UNASSIGNED) {
                edge.to = to;
                edge.sum = len;
                edge.updates = 1;
                return;
            }
        }
    }

    uint32_t o2o(DijkstraContext& ctx, uint32_t start, uint32_t goal) {
        FlatHeap& heap = ctx.heap;
        heap.clear();

        ctx.dist[start] = 0;
        ctx.timestamps[start] = ctx.generation;
        heap.push({0, start});

        while (!heap.empty()) {
            auto [currDist, u] = heap.pop();

            if (u == goal) {
                ctx.generation++;
                return currDist;
            }

            if (currDist > ctx.dist[u]) continue;

            for (uint32_t i = 0; i != MAX_EDGES; ++i) {
                const Edge& edge = edges[getEdgeOffset(u, i)];
                if (edge.to == UNASSIGNED) break;

                uint32_t v = edge.to;
                uint32_t avgLen = edge.updates ? (edge.sum / edge.updates) : edge.sum;
                uint32_t newDist = ctx.dist[u] + avgLen;

                if (ctx.timestamps[v] != ctx.generation || newDist < ctx.dist[v]) {
                    ctx.dist[v] = newDist;
                    ctx.timestamps[v] = ctx.generation;
                    heap.push({newDist, v});
                }
            }
        }

        ctx.generation++;
        return std::numeric_limits<uint32_t>::max();
    }

    uint64_t o2a(DijkstraContext& ctx, uint32_t start) {
        FlatHeap heap;
        heap.reserve(128);

        ctx.dist[start] = 0;
        ctx.timestamps[start] = ctx.generation;
        heap.push({0, start});

        while (!heap.empty()) {
            auto [currDist, u] = heap.pop();

            if (currDist > ctx.dist[u]) continue;

            for (uint32_t i = 0; i != MAX_EDGES; ++i) {
                const Edge& edge = edges[getEdgeOffset(u, i)];
                if (edge.to == UNASSIGNED) break;

                uint32_t v = edge.to;
                uint32_t avgLen = edge.updates ? (edge.sum / edge.updates) : edge.sum;
                uint32_t newDist = ctx.dist[u] + avgLen;

                if (ctx.timestamps[v] != ctx.generation || newDist < ctx.dist[v]) {
                    ctx.dist[v] = newDist;
                    ctx.timestamps[v] = ctx.generation;
                    heap.push({newDist, v});
                }
            }
        }

        uint64_t total = 0;
        for (uint32_t i = 0; i < MAX_NODES; ++i) {
            if (i != start && ctx.timestamps[i] == ctx.generation) {
                total += ctx.dist[i];
            }
        }

        ctx.generation++;
        return total;
    }
};