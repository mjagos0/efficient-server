#include <atomic>
#include <cstdint>
#include <cmath>
#include <memory>
#include <mutex>

constexpr int GRID_BUCKET_COUNT = 1 << 20;

struct GridLocation {
    uint32_t nodeId;
    int32_t x, y;
    GridLocation(uint32_t id, int32_t x_, int32_t y_) : nodeId(id), x(x_), y(y_) {}
};

class HashGrid {
public:
    HashGrid();

    uint32_t processPoint(int32_t x, int32_t y);

private:
    struct Node {
        GridLocation data;
        Node* next;
        Node(GridLocation d, Node* n) : data(d), next(n) {}
    };

    Node* buckets[GRID_BUCKET_COUNT];
    std::mutex bucketLocks[GRID_BUCKET_COUNT];

    std::atomic<uint32_t> nodeIdSequence;

    static int64_t xyHash(int32_t x, int32_t y);
    static bool isWithinRadius(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
};