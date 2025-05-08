

#include "fl/math.h"
#include "fl/point.h"

namespace fl {



// struct Vec2 {
//     float x, y;
// };

// struct GridCell {
//     int x, y;
// };

// class GridVisitor {
// public:
//     virtual void visit(const GridCell& cell) = 0;
//     virtual ~GridVisitor() = default;
// };

template<typename GridVisitor>
inline void traverseGridSegment(
    const point_xy_float& start,
    const point_xy_float& end,
    GridVisitor& visitor)
{
    int x0 = static_cast<int>(fl::floor(start.x));
    int y0 = static_cast<int>(fl::floor(start.y));
    int x1 = static_cast<int>(fl::floor(end.x));
    int y1 = static_cast<int>(fl::floor(end.y));

    int stepX = (x1 > x0) ? 1 : (x1 < x0) ? -1 : 0;
    int stepY = (y1 > y0) ? 1 : (y1 < y0) ? -1 : 0;

    float dx = end.x - start.x;
    float dy = end.y - start.y;

    float tDeltaX = (dx != 0.0f) ? fl::abs(1.0f / dx) : FLT_MAX;
    float tDeltaY = (dy != 0.0f) ? fl::abs(1.0f / dy) : FLT_MAX;

    float nextX = (stepX > 0) ? (fl::floor(start.x) + 1) : fl::floor(start.x);
    float nextY = (stepY > 0) ? (fl::floor(start.y) + 1) : fl::floor(start.y);

    float tMaxX = (dx != 0.0f) ? fl::abs((nextX - start.x) / dx) : FLT_MAX;
    float tMaxY = (dy != 0.0f) ? fl::abs((nextY - start.y) / dy) : FLT_MAX;

    float maxT = 1.0f;

    int currentX = x0;
    int currentY = y0;

    while (true) {
        visitor.visit(currentX, currentY);
        float t = MIN(tMaxX, tMaxY);
        if (t > maxT) break;

        if (tMaxX < tMaxY) {
            tMaxX += tDeltaX;
            currentX += stepX;
        } else {
            tMaxY += tDeltaY;
            currentY += stepY;
        }
    }

    // Ensure the end cell (x1, y1) is visited at least once
    if (currentX != x1 || currentY != y1) {
        visitor.visit(x1, y1);
    }
}

}