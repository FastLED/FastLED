#pragma once

/*
 Iterative, in‐place Douglas–Peucker line simplifier with O(n) extra memory.
*/

#include "fl/bitset.h"
#include "fl/math_macros.h"
#include "fl/pair.h"
#include "fl/point.h"
#include "fl/slice.h"
#include "fl/vector.h"
#include "math.h"

namespace fl {

template <typename FloatT> class LineSimplifier {
  public:
    using Point = fl::point_xy<FloatT>;
    using VectorPoint = fl::vector<Point>;

    LineSimplifier() : mMinDistance(EPSILON_F) {}
    explicit LineSimplifier(FloatT e) : mMinDistance(e) {}

    void setMinimumDistance(FloatT eps) { mMinDistance = eps; }

    void simplifyInplace(fl::vector<Point> *polyline) {
        // run the simplification algorithm
        const fl::Slice<Point> slice(polyline->data(), polyline->size());
        simplifyT(slice, polyline);
    }

    template <typename VectorType>
    void simplifyInplace(VectorType *polyLine) {
        // run the simplification algorithm
        const fl::Slice<Point> slice(polyLine.data(), polyLine.size());
        simplifyT(slice, &polyLine);
    }

    void simplify(const fl::Slice<Point> &polyLine, fl::vector<Point> *out) {
        // run the simplification algorithm
        simplifyT(polyLine, out);
    }

    template <typename VectorType>
    void simplifyT(const fl::Slice<Point> &polyLine, VectorType *out) {
        // run the simplification algorithm
        simplifyInternal(polyLine);

        // copy the result to the output slice
        out->assign(mSimplified.begin(), mSimplified.end());
    }

  private:
    // Runs in O(n) allocations: one bool‐array + one index stack + one output
    // vector
    void simplifyInternal(const fl::Slice<Point> &polyLine) {
        int n = polyLine.size();
        if (n < 2) {
            mSimplified.clear();
            return;
        }

        // mark all points as “kept” initially
        keep.assign(n, 1);

        // explicit stack of (start,end) index pairs
        indexStack.clear();
        indexStack.reserve(64);
        indexStack.push_back({0, n - 1});

        // process segments
        while (!indexStack.empty()) {
            // auto [i0, i1] = indexStack.back();
            int i0 = indexStack.back().first;
            int i1 = indexStack.back().second;
            indexStack.pop_back();

            // find farthest point in [i0+1 .. i1-1]
            FloatT maxDist = 0;
            int split = i0;
            for (int i = i0 + 1; i < i1; ++i) {
                if (!keep[i])
                    continue;
                FloatT d = PerpendicularDistance(polyLine[i], polyLine[i0],
                                                 polyLine[i1]);
                if (d > maxDist) {
                    maxDist = d;
                    split = i;
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
        mSimplified.clear();
        mSimplified.reserve(n);
        for (int i = 0; i < n; ++i) {
            if (keep[i])
                mSimplified.push_back(polyLine[i]);
        }
    }

  private:
    FloatT mMinDistance;

    // workspace buffers
    fl::vector<char> keep;                     // marks which points survive
    fl::vector<fl::pair<int, int>> indexStack; // manual recursion stack
    VectorPoint mSimplified;                   // output buffer

    static FloatT PerpendicularDistance(const Point &pt, const Point &a,
                                        const Point &b) {
        FloatT dx = b.x - a.x;
        FloatT dy = b.y - a.y;
        FloatT mag = sqrt(dx * dx + dy * dy);
        if (mag > 0) {
            dx /= mag;
            dy /= mag;
        }
        FloatT vx = pt.x - a.x;
        FloatT vy = pt.y - a.y;
        // project (vx,vy) onto (dx,dy)
        FloatT proj = dx * vx + dy * vy;
        // subtract projection
        FloatT ux = vx - proj * dx;
        FloatT uy = vy - proj * dy;
        return sqrt(ux * ux + uy * uy);
    }
};

} // namespace fl
