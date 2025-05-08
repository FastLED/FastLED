#pragma once

/*

Line simplification based of an improved Douglas-Peucker algorithm with only
O(n) extra memory. Memory structures are inlined so that most simplifications
can be done with zero heap allocations.

*/

#include "fl/bitset.h"
#include "fl/math.h"
#include "fl/math_macros.h"
#include "fl/pair.h"
#include "fl/point.h"
#include "fl/slice.h"
#include "fl/vector.h"

namespace fl {

template <typename NumberT = float> class LineSimplifier {
  public:
    // This line simplification algorithm will remove vertices that are close
    // together upto a distance of mMinDistance. The algorithm is based on the
    // Douglas-Peucker but with some tweaks for memory efficiency. Most common
    // usage of this class for small sized inputs (~20) will produce no heap
    // allocations.
    using Point = fl::point_xy<NumberT>;
    using VectorPoint = fl::vector<Point>;

    LineSimplifier() : mMinDistance(EPSILON_F) {}
    LineSimplifier(const LineSimplifier &other) = default;
    LineSimplifier &operator=(const LineSimplifier &other) = default;
    LineSimplifier(LineSimplifier &&other) = default;
    LineSimplifier &operator=(LineSimplifier &&other) = default;

    explicit LineSimplifier(NumberT e) : mMinDistance(e) {}
    void setMinimumDistance(NumberT eps) { mMinDistance = eps; }

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
            const bool has_interior = (i1 - i0) > 1;
            if (!has_interior) {
                // no interior points, just keep the endpoints
                // keep[i0] = 1;
                // keep[i1] = 1;
                continue;
            }

            // find farthest point in [i0+1 .. i1-1]
            NumberT maxDist2 = 0;
            int split = i0;
            for (int i = i0 + 1; i < i1; ++i) {
                if (!keep[i])
                    continue;
                NumberT d2 = PerpendicularDistance2(polyLine[i], polyLine[i0],
                                                    polyLine[i1]);

                // FASTLED_WARN("Perpendicular distance2 between "
                //              << polyLine[i] << " and " << polyLine[i0]
                //              << " and " << polyLine[i1] << " is " << d2);

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
    NumberT mMinDistance;

    // workspace buffers
    fl::bitset<256> keep; // marks which points survive
    fl::vector_inlined<fl::pair<int, int>, 64>
        indexStack;          // manual recursion stack
    VectorPoint mSimplified; // output buffer

    static NumberT PerpendicularDistance2(const Point &pt, const Point &a,
                                          const Point &b) {
        // vector AB
        NumberT dx = b.x - a.x;
        NumberT dy = b.y - a.y;
        // vector AP
        NumberT vx = pt.x - a.x;
        NumberT vy = pt.y - a.y;

        // squared length of AB
        NumberT len2 = dx * dx + dy * dy;
        if (len2 <= NumberT(0)) {
            // A and B coincide — just return squared dist from A to P
            return vx * vx + vy * vy;
        }

        // cross‐product magnitude (AB × AP) in 2D is (dx*vy − dy*vx)
        NumberT cross = dx * vy - dy * vx;
        // |cross|/|AB| is the perpendicular distance; we want squared:
        return (cross * cross) / len2;
    }
};

template <typename NumberT = float> class LineSimplifierExact {
  public:
    LineSimplifierExact() = default;
    using Point = point_xy<NumberT>;

    LineSimplifierExact(int count) : mCount(count) {}

    void setCount(uint32_t count) { mCount = count; }

    template <typename VectorType = fl::vector<Point>>
    void simplifyInplace(VectorType *polyLine) {
        return simplify(*polyLine, polyLine);
    }

    template <typename VectorType = fl::vector<Point>>
    void simplify(const fl::Slice<const Point> &polyLine, VectorType *out) {
        if (mCount > polyLine.size()) {
            safeCopy(polyLine, out);
            return;
        } else if (mCount == polyLine.size()) {
            safeCopy(polyLine, out);
            return;
        } else if (mCount < 2) {
            fl::vector_fixed<Point, 2> temp;
            if (polyLine.size() > 0) {
                temp.push_back(polyLine[0]);
            }
            if (polyLine.size() > 1) {
                temp.push_back(polyLine[polyLine.size() - 1]);
            }
            out->assign(temp.begin(), temp.end());
            return;
        }
        NumberT est_max_dist = estimateMaxDistance(polyLine);
        NumberT min = 0;
        NumberT max = est_max_dist;
        NumberT mid = (min + max) / 2.0f;
        while (true) {
            // min < max;
            auto diff  = max - min;
            const bool done = (diff < 0.01f);
            out->clear();
            mLineSimplifier.setMinimumDistance(mid);
            mLineSimplifier.simplify(polyLine, out);
            if (done || out->size() == mCount) {
                return;
            }
            if (out->size() < mCount) {
                max = mid;
            } else {
                min = mid;
            }
            mid = (min + max) / 2.0f;
        }
    }

  private:
    static NumberT estimateMaxDistance(const fl::Slice<const Point> &polyLine) {
        // Rough guess: max distance between endpoints
        if (polyLine.size() < 2)
            return 0;

        NumberT sum = 0;
        for (size_t i = 1; i < polyLine.size(); ++i) {
            const Point &p0 = polyLine[i - 1];
            const Point &p1 = polyLine[i];
            NumberT dx = p1.x - p0.x;
            NumberT dy = p1.y - p0.y;
            sum += fl::sqrt(dx * dx + dy * dy);
        }
        return sum / 2.0f;
    }

    template <typename VectorType>
    void safeCopy(const fl::Slice<const Point> &polyLine, VectorType *out) {
        auto *first_out = out->data();
        // auto* last_out = first_out + mCount;
        auto *other_first_out = polyLine.data();
        // auto* other_last_out = other_first_out + polyLine.size();
        const bool is_same = first_out == other_first_out;
        if (is_same) {
            return;
        }
        auto *last_out = first_out + mCount;
        auto *other_last_out = other_first_out + polyLine.size();

        const bool is_overlapping =
            (first_out >= other_first_out && first_out < other_last_out) ||
            (other_first_out >= first_out && other_first_out < last_out);

        if (!is_overlapping) {
            out->assign(polyLine.data(), polyLine.data() + polyLine.size());
            return;
        }

        // allocate a temporary buffer
        fl::vector_inlined<Point, 64> temp;
        temp.assign(polyLine.begin(), polyLine.end());
        out->assign(temp.begin(), temp.end());
        return;
    }

    uint32_t mCount = 10;
    LineSimplifier<NumberT> mLineSimplifier;
};

} // namespace fl
