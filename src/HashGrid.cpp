#include "HashGrid.h"

#include <algorithm>
#include <mutex>

#define FIELD_SIZE 500
#define RADIUS_THRESHOLD (FIELD_SIZE * FIELD_SIZE)
#define MAX_DELTA (FIELD_SIZE + 1)

HashGrid::HashGrid() : nodeIdSequence(0) { }

uint32_t HashGrid::processPoint(int32_t x, int32_t y) {
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
        for (Node* node = buckets[index];
             node != nullptr;
             node = node->next) {
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

inline int64_t HashGrid::xyHash(const int32_t x, const int32_t y) {
    return (static_cast<int64_t>(x) << 32) | static_cast<uint32_t>(y);
}

bool HashGrid::isWithinRadius(const int32_t x1, const int32_t y1,
                                        const int32_t x2, const int32_t y2) {
    const int dx = std::abs(x2 - x1);
    if (dx > MAX_DELTA) return false;
    const int dy = std::abs(y2 - y1);
    if (dy > MAX_DELTA) return false;

    return (dx * dx + dy * dy) <= RADIUS_THRESHOLD;
}
