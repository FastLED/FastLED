// g++ --std=c++11 test.cpp

#include "test.h"

#include <random>

#include "fl/line_simplification.h"
#include "fl/warn.h"

using namespace fl;

TEST_CASE("Test Line Simplification") {
    // default‐constructed bitset is empty
    LineSimplifier<float> ls;
    ls.setMinimumDistance(0.1f);
    fl::vector<vec2<float>> points;
    points.push_back({0.0f, 0.0f});
    points.push_back({1.0f, 1.0f});
    points.push_back({2.0f, 2.0f});
    points.push_back({3.0f, 3.0f});
    points.push_back({4.0f, 4.0f});
    ls.simplifyInplace(&points);
    REQUIRE_EQ(2,
               points.size()); // Only 2 points on co-linear line should remain.
    REQUIRE_EQ(vec2<float>(0.0f, 0.0f), points[0]);
    REQUIRE_EQ(vec2<float>(4.0f, 4.0f), points[1]);
}

TEST_CASE("Test simple triangle") {
    LineSimplifier<float> ls;

    fl::vector<vec2<float>> points;
    points.push_back({0.0f, 0.0f}); // First point of triangle
    points.push_back({0.5f, 0.5f});
    points.push_back({0.0f, 1.0f});
    float exceeds_thresh = 0.49f;
    float under_thresh = 0.51f;
    ls.setMinimumDistance(exceeds_thresh);
    fl::vector<vec2<float>> output;
    ls.simplify(points, &output);
    REQUIRE_EQ(3, output.size());
    REQUIRE_EQ(vec2<float>(0.0f, 0.0f), output[0]);
    REQUIRE_EQ(vec2<float>(0.5f, 0.5f), output[1]);
    REQUIRE_EQ(vec2<float>(0.0f, 1.0f), output[2]);

    ls.setMinimumDistance(under_thresh);
    ls.simplify(points, &output);
    REQUIRE_EQ(2, output.size());
    REQUIRE_EQ(vec2<float>(0.0f, 0.0f), output[0]);
    REQUIRE_EQ(vec2<float>(0.0f, 1.0f), output[1]);
}

TEST_CASE("Test Line Simplification with Different Distance Thresholds") {
    LineSimplifier<float> ls;

    // Test with a triangle shape - non-collinear points
    ls.setMinimumDistance(0.5f);
    fl::vector<vec2<float>> points1;
    points1.push_back({0.0f, 0.0f}); // First point of triangle
    points1.push_back({0.3f, 0.3f}); // Should be filtered out (distance < 0.5)
    points1.push_back({1.0f, 1.0f}); // Second point of triangle
    points1.push_back({0.8f, 1.2f}); // Should be filtered out (distance < 0.5)
    points1.push_back({0.0f, 2.0f}); // Third point of triangle
    ls.simplifyInplace(&points1);
    REQUIRE_EQ(3, points1.size()); // Triangle vertices should remain
    REQUIRE_EQ(vec2<float>(0.0f, 0.0f), points1[0]);
    REQUIRE_EQ(vec2<float>(1.0f, 1.0f), points1[1]);
    REQUIRE_EQ(vec2<float>(0.0f, 2.0f), points1[2]);
}

TEST_CASE("Test Line Simplification with Complex Shape") {
    // SUBCASE("at threshold") {
    //     LineSimplifier<float> ls;
    //     // Test with a more complex shape and smaller threshold
    //     ls.setMinimumDistance(0.1f);
    //     fl::vector<vec2<float>> points2;
    //     points2.push_back({0.0f, 0.0f}); // Start point
    //     points2.push_back({0.1f, 0.20f}); // Filtered out
    //     points2.push_back({0.0f, 0.29f}); // Filtered out
    //     points2.push_back({0.0f, 1.0f}); // Should be kept (distance > 0.2)
    //     ls.simplifyInplace(&points2);
    //     REQUIRE_EQ(3, points2.size());
    //     REQUIRE_EQ(vec2<float>(0.0f, 0.0f), points2[0]);
    //     REQUIRE_EQ(vec2<float>(0.10f, 0.10f), points2[1]);
    //     REQUIRE_EQ(vec2<float>(0.0f, 1.0f), points2[2]);
    // };

    SUBCASE("Above threshold") {
        LineSimplifier<float> ls;
        // Test with a more complex shape and larger threshold
        ls.setMinimumDistance(0.101f);
        fl::vector<vec2<float>> points3;
        points3.push_back({0.0f, 0.0f}); // Start point
        points3.push_back({0.1f, 0.1f}); // Filtered out
        points3.push_back({0.0f, 0.3f}); // Filtered out
        points3.push_back({0.0f, 1.0f}); // Should be kept (distance > 0.5)
        ls.simplifyInplace(&points3);
        REQUIRE_EQ(2, points3.size());
        REQUIRE_EQ(vec2<float>(0.0f, 0.0f), points3[0]);
        REQUIRE_EQ(vec2<float>(0.0f, 1.0f), points3[1]);
    };
}

TEST_CASE("Iteratively find the closest point") {
    LineSimplifier<float> ls;
    fl::vector<vec2<float>> points;
    points.push_back({0.0f, 0.0f}); // First point of triangle
    points.push_back({0.5f, 0.5f});
    points.push_back({0.0f, 1.0f});

    float thresh = 0.0;
    while (true) {
        ls.setMinimumDistance(thresh);
        fl::vector<vec2<float>> output;
        ls.simplify(points, &output);
        if (output.size() == 2) {
            break;
        }
        thresh += 0.01f;
    }
    REQUIRE(ALMOST_EQUAL(thresh, 0.5f, 0.01f));
}



TEST_CASE("Binary search the the threshold that gives 3 points") {
    LineSimplifierExact<float> ls;
    fl::vector<vec2<float>> points;
    points.push_back({0.0f, 0.0f}); // First point of triangle
    points.push_back({0.5f, 0.5f});
    points.push_back({0.0f, 1.0f});
    points.push_back({0.6f, 2.0f});
    points.push_back({0.0f, 6.0f});

    ls.setCount(3);

    fl::vector<vec2<float>> out;

    ls.simplify(points, &out);
    REQUIRE_EQ(3, out.size());
    MESSAGE("Done");
}


TEST_CASE("Known bad") {
    fl::vector<vec2<float>> points;
    points.push_back({-3136.439941f, 2546.339844f});
    points.push_back({4580.994141f, -3516.982422f});
    points.push_back({-1228.554688f, -5104.814453f});
    points.push_back({-8806.442383f, 3895.103516f});
    points.push_back({-2039.114746f, 1878.047852f});

    LineSimplifierExact<float> ls;
    ls.setCount(3);
    fl::vector<vec2<float>> out;
    ls.simplify(points, &out);

    MESSAGE("Output points: " << out.size());
    MESSAGE("Output points: " << out);

    REQUIRE_EQ(3, out.size());
}


// TEST_CASE("Binary search reduction to 3 points from 5 random points (1000 runs)") {
//     constexpr int kTrials = 1000;
//     constexpr int kInputPoints = 5;
//     constexpr int kTargetPoints = 3;

//     std::mt19937 rng(123); // fixed seed for reproducibility
//     std::uniform_real_distribution<float> dist(-10000.0f, 10000.0f);

//     for (int trial = 0; trial < kTrials; ++trial) {
//         LineSimplifierExact<float> ls;
//         fl::vector<vec2<float>> points;

//         for (int i = 0; i < kInputPoints; ++i) {
//             points.push_back({dist(rng), dist(rng)});
//         }

//         ls.setCount(kTargetPoints);

//         fl::vector<vec2<float>> out;
//         ls.simplify(points, &out);


//         const bool bad_value = (out.size() != kTargetPoints);

//         if (bad_value) {
//             INFO("Trial " << trial << ": Input points: " << points.size()
//                 << ", Output points: " << out.size() << ", " << out);

//             for (size_t i = 0; i < points.size(); ++i) {
//                 auto p = points[i];
//                 FASTLED_WARN("Input point " << i << ": " << p);
//             }

//             // Assert
//             REQUIRE_EQ(kTargetPoints, out.size());
//         }



//     }

//     MESSAGE("Completed 1000 trials of random 5→3 simplification");
// }
