#pragma once

/*

Line simplification based of an improved Douglas-Peucker algorithm with only
O(n) extra memory. Memory structures are inlined so that most simplifications
can be done with zero heap allocations.

There are two versions here, one that simplifies using a threshold, and another
version which will simplify to an exact number of points, however the latter is
expensive since it must re-run the algorithm multiple times to find the right
threshold. The first version is much faster and should be used in most cases.

*/

#include "fl/bitset.h"
#include "fl/math.h"
#include "fl/math_macros.h"
#include "fl/pair.h"
#include "fl/point.h"
#include "fl/span.h"
#include "fl/vector.h"

namespace fl {

template <typename NumberT = float> class LineSimplifier {
  public:
    // This line simplification algorithm will remove vertices that are close
    // together upto a distance of mMinDistance. The algorithm is based on the
    // Douglas-Peucker but with some tweaks for memory efficiency. Most common
    // usage of this class for small sized inputs (~20) will produce no heap
    // allocations.
    using Point = fl::vec2<NumberT>;
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
    void simplify(const fl::span<const Point> &polyLine,
                  fl::vector<Point> *out) {
        simplifyT(polyLine, out);
    }
    template <typename VectorType>
    void simplify(const fl::span<Point> &polyLine, VectorType *out) {
        simplifyT(polyLine, out);
    }

    template <typename VectorType>
    static void removeOneLeastError(VectorType *_poly) {
        bitset<256> keep;
        VectorType &poly = *_poly;
        keep.assign(poly.size(), 1);
        const int n = poly.size();
        NumberT bestErr = INFINITY_DOUBLE;
        int bestIdx = -1;

        // scan all interior “alive” points
        for (int i = 1; i + 1 < n; ++i) {
            if (!keep[i])
                continue;

            // find previous alive
            int L = i - 1;
            while (L >= 0 && !keep[L])
                --L;
            // find next alive
            int R = i + 1;
            while (R < n && !keep[R])
                ++R;

            if (L < 0 || R >= n)
                continue; // endpoints

            // compute perp‐distance² to the chord L→R
            NumberT dx = poly[R].x - poly[L].x;
            NumberT dy = poly[R].y - poly[L].y;
            NumberT vx = poly[i].x - poly[L].x;
            NumberT vy = poly[i].y - poly[L].y;
            NumberT len2 = dx * dx + dy * dy;
            NumberT err =
                (len2 > NumberT(0))
                    ? ((dx * vy - dy * vx) * (dx * vy - dy * vx) / len2)
                    : (vx * vx + vy * vy);

            if (err < bestErr) {
                bestErr = err;
                bestIdx = i;
            }
        }

        // now “remove” that one point
        if (bestIdx >= 0)
            // keep[bestIdx] = 0;
            poly.erase(poly.begin() + bestIdx);
    }

  private:
    template <typename VectorType> void simplifyInplaceT(VectorType *polyLine) {
        // run the simplification algorithm
        span<Point> slice(polyLine->data(), polyLine->size());
        simplifyT(slice, polyLine);
    }

    template <typename VectorType>
    void simplifyT(const fl::span<const Point> &polyLine, VectorType *out) {
        // run the simplification algorithm
        simplifyInternal(polyLine);

        // copy the result to the output slice
        out->assign(mSimplified.begin(), mSimplified.end());
    }
    // Runs in O(n) allocations: one bool‐array + one index stack + one output
    // vector
    void simplifyInternal(const fl::span<const Point> &polyLine) {
        mSimplified.clear();
        int n = polyLine.size();
        if (n < 2) {
            if (n) {
                mSimplified.assign(polyLine.data(), polyLine.data() + n);
            }
            return;
        }
        const NumberT minDist2 = mMinDistance * mMinDistance;

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

            if (maxDist2 > minDist2) {
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
    using Point = vec2<NumberT>;

    LineSimplifierExact(int count) : mCount(count) {}

    void setCount(u32 count) { mCount = count; }

    template <typename VectorType = fl::vector<Point>>
    void simplifyInplace(VectorType *polyLine) {
        return simplify(*polyLine, polyLine);
    }

    template <typename VectorType = fl::vector<Point>>
    void simplify(const fl::span<const Point> &polyLine, VectorType *out) {
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
            auto diff = max - min;
            const bool done = (diff < 0.01f);
            out->clear();
            mLineSimplifier.setMinimumDistance(mid);
            mLineSimplifier.simplify(polyLine, out);

            fl::size n = out->size();

            if (n == mCount) {
                return; // we are done
            }

            // Handle the last few iterations manually. Often the algo will get
            // stuck here.
            if (n == mCount + 1) {
                // Just one more left, so peel it off.
                mLineSimplifier.removeOneLeastError(out);
                return;
            }

            if (n == mCount + 2) {
                // Just two more left, so peel them off.
                mLineSimplifier.removeOneLeastError(out);
                mLineSimplifier.removeOneLeastError(out);
                return;
            }

            if (done) {
                while (out->size() > mCount) {
                    // we have too many points, so we need to increase the
                    // distance
                    mLineSimplifier.removeOneLeastError(out);
                }
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
    static NumberT estimateMaxDistance(const fl::span<const Point> &polyLine) {
        // Rough guess: max distance between endpoints
        if (polyLine.size() < 2)
            return 0;

        const Point &first = polyLine[0];
        const Point &last = polyLine[polyLine.size() - 1];
        NumberT dx = last.x - first.x;
        NumberT dy = last.y - first.y;
        return sqrt(dx * dx + dy * dy);
    }

    template <typename VectorType>
    void safeCopy(const fl::span<const Point> &polyLine, VectorType *out) {
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

    u32 mCount = 10;
    LineSimplifier<NumberT> mLineSimplifier;
};

} // namespace fl
