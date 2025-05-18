#ifndef HASH_GRID_H
#define HASH_GRID_H

#include <atomic>
#include <cstdint>
#include <cmath>
#include <memory>
#include <mutex>
#include <algorithm>

struct HashGrid {
    static constexpr int GRID_BUCKET_COUNT = 1 << 20;
    static constexpr int FIELD_SIZE = 500;
    static constexpr int RADIUS_THRESHOLD = FIELD_SIZE * FIELD_SIZE;
    static constexpr int MAX_DELTA = FIELD_SIZE + 1;

    struct GridLocation {
        uint32_t nodeId;
        int32_t x, y;
        GridLocation(uint32_t id, int32_t x_, int32_t y_) : nodeId(id), x(x_), y(y_) {}
    };

    struct Node {
        GridLocation data;
        Node* next;
        Node(GridLocation d, Node* n) : data(d), next(n) {}
    };

    Node* buckets[GRID_BUCKET_COUNT]{};
    std::mutex bucketLocks[GRID_BUCKET_COUNT];
    std::atomic<uint32_t> nodeIdSequence{1}; // 0 is reserved for unassigned

    uint32_t processPoint(const int32_t x, const int32_t y) {
        const int gridX = x / FIELD_SIZE;
        const int gridY = y / FIELD_SIZE;

        size_t neighborhoodIndices[9];
        size_t selfBucketIndex = 0;
        int i = 0;
        for (int y_ = gridY - 1; y_ <= gridY + 1; ++y_) {
            for (int x_ = gridX - 1; x_ <= gridX + 1; ++x_) {
                int64_t hash = xyHash(x_, y_);
                size_t idx = static_cast<size_t>(hash) % GRID_BUCKET_COUNT;
                neighborhoodIndices[i] = idx;
                if (x_ == gridX && y_ == gridY) {
                    selfBucketIndex = idx;
                }
                ++i;
            }
        }

        std::sort(neighborhoodIndices, neighborhoodIndices + 9);
        size_t* end = std::unique(neighborhoodIndices, neighborhoodIndices + 9);
        int count = static_cast<int>(end - neighborhoodIndices);

        for (int j = 0; j < count; ++j) {
            bucketLocks[neighborhoodIndices[j]].lock();
        }

        for (int j = 0; j < count; ++j) {
            size_t index = neighborhoodIndices[j];
            for (Node* node = buckets[index]; node != nullptr; node = node->next) {
                if (isWithinRadius(x, y, node->data.x, node->data.y)) {
                    for (int k = 0; k < count; ++k) {
                        bucketLocks[neighborhoodIndices[k]].unlock();
                    }
                    return node->data.nodeId;
                }
            }
        }

        const uint32_t nodeId = nodeIdSequence.fetch_add(1, std::memory_order_relaxed);
        Node* newNode = new Node(GridLocation(nodeId, x, y), buckets[selfBucketIndex]);
        buckets[selfBucketIndex] = newNode;

        for (int j = 0; j < count; ++j) {
            bucketLocks[neighborhoodIndices[j]].unlock();
        }

        return nodeId;
    }

    static int64_t xyHash(int32_t x, int32_t y) {
        return (static_cast<int64_t>(x) << 32) | static_cast<uint32_t>(y);
    }

    static bool isWithinRadius(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
        int dx = std::abs(x2 - x1);
        if (dx > MAX_DELTA) return false;
        int dy = std::abs(y2 - y1);
        if (dy > MAX_DELTA) return false;
        return (dx * dx + dy * dy) <= RADIUS_THRESHOLD;
    }
};

#endif // HASH_GRID_H
