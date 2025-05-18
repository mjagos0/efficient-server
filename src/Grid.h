#ifndef GRID_H
#define GRID_H

#include <atomic>

#define MAX_RADIUS 500
#define CELL_SIZE 250
#define UNASSIGED 0

struct Cell {
    int32_t nodeId;
    int32_t x, y;
    std::atomic<bool> consistent;

    Cell(int32_t nodeId, int32_t x, int32_t y)
        : nodeId(nodeId), x(x), y(y), consistent(false) { }
};

struct Grid {
    std::atomic<uint32_t> nodeIdSequence;
    
    inline uint64_t getCellIdx(const uint32_t x, const uint32_t y) {
        const uint32_t gridX = x / CELL_SIZE;
        const uint32_t gridY = y / CELL_SIZE;
        return (static_cast<uint64_t>(gridX) << 32) | static_cast<uint32_t>(gridY); 
    }

    inline uint32_t addPoint(const uint32_t x, const uint32_t y) {
        const uint64_t cellIdx = getCellIdx(x, y);

        return 0;
    }

    bool isWithinRadius(const int32_t x1, const int32_t y1, const int32_t x2, const int32_t y2) {
        const int dx = std::abs(x2 - x1);
        if (dx > MAX_RADIUS) return false;
        const int dy = std::abs(y2 - y1);
        if (dy > MAX_RADIUS) return false;

        return (dx * dx + dy * dy) <= MAX_RADIUS;
    }
};

#endif // GRID_H