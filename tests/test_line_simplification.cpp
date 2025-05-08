// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/line_simplification.h"
#include "fl/warn.h"

using namespace fl;

TEST_CASE("Test Line Simplification") {
    // default‐constructed bitset is empty
    LineSimplifier<float> ls;
    ls.setMinimumDistance(0.1f);
    fl::vector<point_xy<float>> points;
    points.push_back({0.0f, 0.0f});
    points.push_back({1.0f, 1.0f});
    points.push_back({2.0f, 2.0f});
    points.push_back({3.0f, 3.0f});
    points.push_back({4.0f, 4.0f});
    ls.simplifyInplace(&points);
    REQUIRE_EQ(2,
               points.size()); // Only 2 points on co-linear line should remain.
    REQUIRE_EQ(point_xy<float>(0.0f, 0.0f), points[0]);
    REQUIRE_EQ(point_xy<float>(4.0f, 4.0f), points[1]);
}

TEST_CASE("Test simple triangle") {
    LineSimplifier<float> ls;

    fl::vector<point_xy<float>> points;
    points.push_back({0.0f, 0.0f}); // First point of triangle
    points.push_back({0.5f, 0.5f});
    points.push_back({0.0f, 1.0f});
    float exceeds_thresh = 0.49f;
    float under_thresh = 0.51f;
    ls.setMinimumDistance(exceeds_thresh);
    fl::vector<point_xy<float>> output;
    ls.simplify(points, &output);
    REQUIRE_EQ(3, output.size());
    REQUIRE_EQ(point_xy<float>(0.0f, 0.0f), output[0]);
    REQUIRE_EQ(point_xy<float>(0.5f, 0.5f), output[1]);
    REQUIRE_EQ(point_xy<float>(0.0f, 1.0f), output[2]);

    ls.setMinimumDistance(under_thresh);
    ls.simplify(points, &output);
    REQUIRE_EQ(2, output.size());
    REQUIRE_EQ(point_xy<float>(0.0f, 0.0f), output[0]);
    REQUIRE_EQ(point_xy<float>(0.0f, 1.0f), output[1]);
}

TEST_CASE("Test Line Simplification with Different Distance Thresholds") {
    LineSimplifier<float> ls;

    // Test with a triangle shape - non-collinear points
    ls.setMinimumDistance(0.5f);
    fl::vector<point_xy<float>> points1;
    points1.push_back({0.0f, 0.0f}); // First point of triangle
    points1.push_back({0.3f, 0.3f}); // Should be filtered out (distance < 0.5)
    points1.push_back({1.0f, 1.0f}); // Second point of triangle
    points1.push_back({0.8f, 1.2f}); // Should be filtered out (distance < 0.5)
    points1.push_back({0.0f, 2.0f}); // Third point of triangle
    ls.simplifyInplace(&points1);
    REQUIRE_EQ(3, points1.size()); // Triangle vertices should remain
    REQUIRE_EQ(point_xy<float>(0.0f, 0.0f), points1[0]);
    REQUIRE_EQ(point_xy<float>(1.0f, 1.0f), points1[1]);
    REQUIRE_EQ(point_xy<float>(0.0f, 2.0f), points1[2]);
}

TEST_CASE("Test Line Simplification with Complex Shape") {
    SUBCASE("at threshold") {
        LineSimplifier<float> ls;
        // Test with a more complex shape and smaller threshold
        ls.setMinimumDistance(0.1f);
        fl::vector<point_xy<float>> points2;
        points2.push_back({0.0f, 0.0f}); // Start point
        points2.push_back({0.1f, 0.1f}); // Filtered out
        points2.push_back({0.0f, 0.3f}); // Filtered out
        points2.push_back({0.0f, 1.0f}); // Should be kept (distance > 0.2)
        ls.simplifyInplace(&points2);
        REQUIRE_EQ(3, points2.size());
        REQUIRE_EQ(point_xy<float>(0.0f, 0.0f), points2[0]);
        REQUIRE_EQ(point_xy<float>(0.10f, 0.10f), points2[1]);
        REQUIRE_EQ(point_xy<float>(0.0f, 1.0f), points2[2]);
    };

    SUBCASE("Above threshold") {
        LineSimplifier<float> ls;
        // Test with a more complex shape and larger threshold
        ls.setMinimumDistance(0.101f);
        fl::vector<point_xy<float>> points3;
        points3.push_back({0.0f, 0.0f}); // Start point
        points3.push_back({0.1f, 0.1f}); // Filtered out
        points3.push_back({0.0f, 0.3f}); // Filtered out
        points3.push_back({0.0f, 1.0f}); // Should be kept (distance > 0.5)
        ls.simplifyInplace(&points3);
        REQUIRE_EQ(2, points3.size());
        REQUIRE_EQ(point_xy<float>(0.0f, 0.0f), points3[0]);
        REQUIRE_EQ(point_xy<float>(0.0f, 1.0f), points3[1]);
    };
}

TEST_CASE("Iteratively find the closest point") {
    LineSimplifier<float> ls;
    fl::vector<point_xy<float>> points;
    points.push_back({0.0f, 0.0f}); // First point of triangle
    points.push_back({0.5f, 0.5f});
    points.push_back({0.0f, 1.0f});

    float thresh = 0.0;
    while (true) {
        ls.setMinimumDistance(thresh);
        fl::vector<point_xy<float>> output;
        ls.simplify(points, &output);
        if (output.size() == 2) {
            break;
        }
        thresh += 0.01f;
    }
    REQUIRE(ALMOST_EQUAL(thresh, 0.5f, 0.01f));
}

class LineSimplifierExact {
  public:
    LineSimplifierExact() = default;
    using Point = point_xy<float>;
    LineSimplifierExact(int count) : mCount(count) {}

    void simplify(const fl::Slice<const point_xy<float>> &polyLine,
                  fl::vector<point_xy<float>> *out) {
        if (mCount > polyLine.size()) {
            safeCopy(polyLine, out);
            return;
        } else if (mCount == polyLine.size()) {
            safeCopy(polyLine, out);
            return;
        } else if (mCount < 2) {
            fl::vector_fixed<point_xy<float>, 2> temp;
            if (polyLine.size() > 0) {
                temp.push_back(polyLine[0]);
            }
            if (polyLine.size() > 1) {
                temp.push_back(polyLine[polyLine.size() - 1]);
            }
            out->assign(temp.begin(), temp.end());
            return;
        }
        float est_max_dist = estimateMaxDistance(polyLine);
        float min = 0;
        float max = est_max_dist;
        float mid = (min + max) / 2.0f;
        while (min < max) {
            out->clear();
            mLineSimplifier.setMinimumDistance(mid);
            mLineSimplifier.simplify(polyLine, out);
            if (out->size() == mCount) {
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

    void setCount(uint32_t count) { mCount = count; }



  private:

    float estimateMaxDistance(const fl::Slice<const Point> &polyLine) {
        // Rough guess: max distance between endpoints
        if (polyLine.size() < 2)
            return 0;

        const Point &first = polyLine[0];
        const Point &last = polyLine[polyLine.size() - 1];
        float dx = last.x - first.x;
        float dy = last.y - first.y;
        return fl::sqrt(dx * dx + dy * dy);
    }

    void safeCopy(const fl::Slice<const point_xy<float>> &polyLine,
                   fl::vector<point_xy<float>> *out) {
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
        fl::vector<point_xy<float>> temp;
        temp.assign(polyLine.begin(), polyLine.end());
        out->assign(temp.begin(), temp.end());
        return;
    }

    uint32_t mCount = 10;
    LineSimplifier<float> mLineSimplifier;
};

TEST_CASE("Binary search the the threshold that gives 3 points") {
    LineSimplifierExact ls;
    fl::vector<point_xy<float>> points;
    points.push_back({0.0f, 0.0f}); // First point of triangle
    points.push_back({0.5f, 0.5f});
    points.push_back({0.0f, 1.0f});
    points.push_back({0.6f, 2.0f});
    points.push_back({0.0f, 6.0f});

    ls.setCount(3);

    fl::vector<point_xy<float>> out;

    ls.simplify(points, &out);

    // REQUIRE(ALMOST_EQUAL(mid, 0.5f, 0.01f));
    // MESSAGE("mid: " << mid);
    // MESSAGE("done");

    REQUIRE_EQ(3, out.size());
    MESSAGE("Done");
}