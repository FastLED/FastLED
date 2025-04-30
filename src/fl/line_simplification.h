#pragma once

/*
 Iterative, in‐place Douglas–Peucker line simplifier with O(n) extra memory.
*/

#include "fl/pair.h"
#include "fl/vector.h"
#include "fl/slice.h"
#include "math.h"
#include "fl/math_macros.h"
#include "fl/bitset.h"

namespace fl {

template <typename FloatT>
class LineSimplifier {
public:
    using Point       = fl::pair<FloatT, FloatT>;
    using VectorPoint = fl::vector<Point>;

    LineSimplifier() : mMinDistance(EPSILON_F) {}
    explicit LineSimplifier(FloatT e) : mMinDistance(e) {}

    void setMinimumDistance(FloatT eps) { mMinDistance = eps; }

    // Runs in O(n) allocations: one bool‐array + one index stack + one output vector
    fl::Slice<Point> simplify(const fl::Slice<Point>& polyLine) {
        int n = polyLine.size();
        if (n < 2) {
            simplified.clear();
            return fl::Slice<Point>(simplified);
        }

        // mark all points as “kept” initially
        keep.assign(n, 1);

        // explicit stack of (start,end) index pairs
        indexStack.clear();
        indexStack.reserve(64);
        indexStack.push_back({0, n - 1});

        // process segments
        while (!indexStack.empty()) {
            auto [i0, i1] = indexStack.back();
            indexStack.pop_back();

            // find farthest point in [i0+1 .. i1-1]
            FloatT maxDist = 0;
            int    split  = i0;
            for (int i = i0 + 1; i < i1; ++i) {
                if (! keep[i]) continue;
                FloatT d = PerpendicularDistance(
                    polyLine[i], polyLine[i0], polyLine[i1]
                );
                if (d > maxDist) {
                    maxDist = d;
                    split   = i;
                }
            }

            if (maxDist > mMinDistance) {
                // need to keep that split point and recurse on both halves
                indexStack.push_back({i0, split});
                indexStack.push_back({split, i1});
            } else {
                // drop all interior points in this segment
                for (int i = i0 + 1; i < i1; ++i) {
                    keep[i] = 0;
                }
            }
        }

        // collect survivors
        simplified.clear();
        simplified.reserve(n);
        for (int i = 0; i < n; ++i) {
            if (keep[i]) simplified.push_back(polyLine[i]);
        }

        return fl::Slice<Point>(simplified);
    }

private:
    FloatT mMinDistance;

    // workspace buffers
    fl::vector<char>               keep;       // marks which points survive
    fl::vector<fl::pair<int,int>>      indexStack; // manual recursion stack
    VectorPoint                    simplified; // output buffer

    static FloatT PerpendicularDistance(
        const Point& pt,
        const Point& a,
        const Point& b
    ) {
        FloatT dx = b.first  - a.first;
        FloatT dy = b.second - a.second;
        FloatT mag = sqrt(dx*dx + dy*dy);
        if (mag > 0) {
            dx /= mag;
            dy /= mag;
        }
        FloatT vx = pt.first  - a.first;
        FloatT vy = pt.second - a.second;
        // project (vx,vy) onto (dx,dy)
        FloatT proj = dx*vx + dy*vy;
        // subtract projection
        FloatT ux = vx - proj*dx;
        FloatT uy = vy - proj*dy;
        return sqrt(ux*ux + uy*uy);
    }
};

} // namespace fl
