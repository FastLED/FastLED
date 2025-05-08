#pragma once

/*

Line simplification based of an improved Douglas-Peucker algorithm with only
O(n) extra memory. Memory structures are inlined so that most simplifications
can be done with zero heap allocations.

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
    // This line simplification algorithm will remove vertices that are close
    // together upto a distance of mMinDistance. The algorithm is based on the
    // Douglas-Peucker but with some tweaks for memory efficiency. Most common
    // usage of this class for small sized inputs (~20) will produce no heap
    // allocations.
    using Point = fl::point_xy<FloatT>;
    using VectorPoint = fl::vector<Point>;

    LineSimplifier() : mMinDistance(EPSILON_F) {}
    LineSimplifier(const LineSimplifier &other) = default;
    LineSimplifier &operator=(const LineSimplifier &other) = default;
    LineSimplifier(LineSimplifier &&other) = default;
    LineSimplifier &operator=(LineSimplifier &&other) = default;

    explicit LineSimplifier(FloatT e) : mMinDistance(e) {}
    void setMinimumDistance(FloatT eps) { mMinDistance = eps; }

    // simplifyInPlace.
    void simplifyInplace(fl::vector<Point> *polyline) {
        simplifyInplaceT(polyline);
    }
    template <typename VectorType> void simplifyInplace(VectorType *polyLine) {
        simplifyInplaceT(polyLine);
    }

    // simplify to the output vector.
    void simplify(const fl::Slice<const Point> &polyLine,
                  fl::vector<Point> *out) {
        simplifyT(polyLine, out);
    }
    template <typename VectorType>
    void simplify(const fl::Slice<Point> &polyLine, VectorType *out) {
        simplifyT(polyLine, out);
    }

  private:
    template <typename VectorType> void simplifyInplaceT(VectorType *polyLine) {
        // run the simplification algorithm
        Slice<Point> slice(polyLine->data(), polyLine->size());
        simplifyT(slice, polyLine);
    }

    template <typename VectorType>
    void simplifyT(const fl::Slice<const Point> &polyLine, VectorType *out) {
        // run the simplification algorithm
        simplifyInternal(polyLine);

        // copy the result to the output slice
        out->assign(mSimplified.begin(), mSimplified.end());
    }
    // Runs in O(n) allocations: one bool‐array + one index stack + one output
    // vector
    void simplifyInternal(const fl::Slice<const Point> &polyLine) {
        mSimplified.clear();
        int n = polyLine.size();
        if (n < 2) {
            if (n) {
                mSimplified.assign(polyLine.data(), polyLine.data() + n);
            }
            return;
        }

        // mark all points as “kept” initially
        keep.assign(n, 1);

        // explicit stack of (start,end) index pairs
        indexStack.clear();
        // indexStack.reserve(64);
        indexStack.push_back({0, n - 1});

        // process segments
        while (!indexStack.empty()) {
            // auto [i0, i1] = indexStack.back();
            auto pair = indexStack.back();
            int i0 = pair.first;
            int i1 = pair.second;
            indexStack.pop_back();

            // find farthest point in [i0+1 .. i1-1]
            FloatT maxDist2 = 0;
            int split = i0;
            for (int i = i0 + 1; i < i1; ++i) {
                if (!keep[i])
                    continue;
                FloatT d2 = PerpendicularDistance2(polyLine[i], polyLine[i0],
                                                   polyLine[i1]);

                FASTLED_WARN("Perpendicular distance2 between " << polyLine[i]
                             << " and " << polyLine[i0] << " and "
                             << polyLine[i1] << " is " << d2);

                if (d2 > maxDist2) {
                    maxDist2 = d2;
                    split = i;
                }
            }

            if (maxDist2 >= mMinDistance * mMinDistance) {
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
    fl::vector_inlined<char, 64> keep; // marks which points survive
    fl::vector_inlined<fl::pair<int, int>, 64>
        indexStack;          // manual recursion stack
    VectorPoint mSimplified; // output buffer

    static FloatT PerpendicularDistance2(const Point &pt, const Point &a,
                                         const Point &b) {
        // vector AB
        FloatT dx = b.x - a.x;
        FloatT dy = b.y - a.y;
        // vector AP
        FloatT vx = pt.x - a.x;
        FloatT vy = pt.y - a.y;

        // squared length of AB
        FloatT len2 = dx * dx + dy * dy;
        if (len2 <= FloatT(0)) {
            // A and B coincide — just return squared dist from A to P
            return vx * vx + vy * vy;
        }

        // cross‐product magnitude (AB × AP) in 2D is (dx*vy − dy*vx)
        FloatT cross = dx * vy - dy * vx;
        // |cross|/|AB| is the perpendicular distance; we want squared:
        return (cross * cross) / len2;
    }
};

} // namespace fl
