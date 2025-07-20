
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "lib8tion/intmap.h"
#include "fl/xypath.h"
#include "fl/vector.h"
#include "fl/unused.h"
#include <string>

using namespace fl;

#define MESSAGE_TILE(TILE) \
    MESSAGE("\nTile:\n" \
            << "  " << TILE.at(0, 0) << " " << TILE.at(1, 0) << "\n" \
            << "  " << TILE.at(0, 1) << " " << TILE.at(1, 1) << "\n");

#define MESSAGE_TILE_ROW(TILE, ROW) \
    MESSAGE("\nTile Row " << ROW << ":\n" \
            << "  " << TILE.at(0, ROW) << " " << TILE.at(1, ROW) << "\n");


TEST_CASE("LinePath") {
    LinePath path(0.0f, 0.0f, 1.0f, 1.0f);
    vec2f xy = path.compute(0.5f);
    REQUIRE(xy.x == 0.5f);
    REQUIRE(xy.y == 0.5f);

    xy = path.compute(1.0f);
    REQUIRE(xy.x == 1.0f);
    REQUIRE(xy.y == 1.0f);

    xy = path.compute(0.0f);
    REQUIRE(xy.x == 0.0f);
    REQUIRE(xy.y == 0.0f);
}

TEST_CASE("LinePath at_subpixel") {
    // Tests that we can get the correct subpixel values at center point 0,0
    auto line = fl::make_shared<LinePath>(-1.0f, -1.0f, 1.0f, -1.0f);
    XYPath path(line);
    path.setDrawBounds(2,2);
    Tile2x2_u8 tile = path.at_subpixel(0);
    REQUIRE_EQ(vec2<uint16_t>(0, 0), tile.origin());
    MESSAGE_TILE(tile);
    REQUIRE_EQ(255, tile.at(0, 0));
}

TEST_CASE("LinePath simple float sweep") {
    // Tests that we can get the correct gaussian values at center point 0,0
    auto point = fl::make_shared<LinePath>(0, 1.f, 1.f, 1.f);
    XYPath path(point);
    auto xy = path.at(0);
    //MESSAGE_TILE(tile);
    REQUIRE_EQ(xy, vec2f(0.0f, 1.f));
    xy = path.at(1);
    REQUIRE_EQ(xy, vec2f(1.f, 1.f));
}

TEST_CASE("Point at exactly the middle") {
    // Tests that we can get the correct gaussian values at center point 0,0
    auto point = fl::make_shared<PointPath>(0.0, 0.0);  // Right in middle.
    XYPath path(point);
    path.setDrawBounds(2,2);
    // auto xy = path.at(0);
    fl::Tile2x2_u8 sp = path.at_subpixel(0);
    //MESSAGE_TILE(tile);
    // REQUIRE_EQ(vec2f(0.0f, 0.f), sp);
    // print out
    auto origin = sp.origin();
    MESSAGE("Origin: " << origin.x << ", " << origin.y);

    MESSAGE(sp.at(0, 0));
    MESSAGE(sp.at(0, 1));
    MESSAGE(sp.at(1, 0));
    MESSAGE(sp.at(1, 1));

    // require that all alpha be the same
    REQUIRE_EQ(sp.at(0, 0), sp.at(0, 1));
    REQUIRE_EQ(sp.at(0, 0), sp.at(1, 0));
    REQUIRE_EQ(sp.at(0, 0), sp.at(1, 1));
    REQUIRE_EQ(sp.at(0, 0), 64);
}

TEST_CASE("LinePath simple sweep in draw bounds") {
    // Tests that we can get the correct gaussian values at center point 0,0
    auto point = fl::make_shared<LinePath>(-1.f, -1.f, 1.f, -1.f);
    XYPath path(point);
    int width = 2;
    path.setDrawBounds(width, width);
    auto begin = path.at(0);
    auto end = path.at(1);
    REQUIRE_EQ(vec2f(0.5f, 0.5f), begin);
    REQUIRE_EQ(vec2f(1.5f, 0.5f), end);
}

TEST_CASE("LinePath at_subpixel moves x") {
    // Tests that we can get the correct subpixel.
    auto point = fl::make_shared<LinePath>(-1.f, -1.f, 1.f, -1.f);
    XYPath path(point);
    path.setDrawBounds(3, 3);
    Tile2x2_u8 tile = path.at_subpixel(0.0f);
    // MESSAGE_TILE(tile);
    REQUIRE_EQ(tile.origin(), vec2<uint16_t>(0, 0));
    REQUIRE_EQ(tile.at(0, 0), 255);
    tile = path.at_subpixel(1.0f);
    REQUIRE_EQ(tile.origin(), vec2<uint16_t>(2, 0));
    REQUIRE_EQ(tile.at(0, 0), 255);
}


TEST_CASE("Test HeartPath") {
    HeartPathPtr heart = fl::make_shared<HeartPath>();
    
    // Track min and max values to help with scaling
    float min_x = 1.0f;
    float max_x = -1.0f;
    float min_y = 1.0f;
    float max_y = -1.0f;
    
    // Sample points along the heart curve
    const int num_samples = 100;
    for (int i = 0; i < num_samples; i++) {
        float alpha = static_cast<float>(i) / (num_samples - 1);
        vec2f point = heart->compute(alpha);
        
        // Update min/max values
        min_x = MIN(min_x, point.x);
        max_x = MAX(max_x, point.x);
        min_y = MIN(min_y, point.y);
        max_y = MAX(max_y, point.y);
        
        // Print every 10th point for visual inspection
        if (i % 10 == 0) {
            MESSAGE("Heart point at alpha=" << alpha << ": (" << point.x << ", " << point.y << ")");
        }
    }
    
    // Print the min/max values
    MESSAGE("\nHeart shape bounds:");
    MESSAGE("X range: [" << min_x << ", " << max_x << "]");
    MESSAGE("Y range: [" << min_y << ", " << max_y << "]");
    
    // Verify the heart is within the expected bounds
    REQUIRE(min_x >= -1.0f);
    REQUIRE(max_x <= 1.0f);
    REQUIRE(min_y >= -1.0f);
    REQUIRE(max_y <= 1.0f);
}

TEST_CASE("Test ArchimedeanSpiralPath") {
    ArchimedeanSpiralPathPtr spiral = fl::make_shared<ArchimedeanSpiralPath>(3, 1.0f);
    
    // Track min and max values to help with scaling
    float min_x = 1.0f;
    float max_x = -1.0f;
    float min_y = 1.0f;
    float max_y = -1.0f;
    
    // Sample points along the spiral curve
    const int num_samples = 100;
    for (int i = 0; i < num_samples; i++) {
        float alpha = static_cast<float>(i) / (num_samples - 1);
        vec2f point = spiral->compute(alpha);
        
        // Update min/max values
        min_x = MIN(min_x, point.x);
        max_x = MAX(max_x, point.x);
        min_y = MIN(min_y, point.y);
        max_y = MAX(max_y, point.y);
        
        // Print every 10th point for visual inspection
        if (i % 10 == 0) {
            MESSAGE("Spiral point at alpha=" << alpha << ": (" << point.x << ", " << point.y << ")");
        }
    }
    
    // Print the min/max values
    MESSAGE("\nSpiral shape bounds:");
    MESSAGE("X range: [" << min_x << ", " << max_x << "]");
    MESSAGE("Y range: [" << min_y << ", " << max_y << "]");
    
    // Verify the spiral is within the expected bounds
    REQUIRE(min_x >= -1.0f);
    REQUIRE(max_x <= 1.0f);
    REQUIRE(min_y >= -1.0f);
    REQUIRE(max_y <= 1.0f);
}

TEST_CASE("Test RosePath") {
    // Test with different petal configurations
    SUBCASE("3-petal rose") {
        RosePathPtr rose = fl::make_shared<RosePath>(3, 1);
        
        // Track min and max values to help with scaling
        float min_x = 1.0f;
        float max_x = -1.0f;
        float min_y = 1.0f;
        float max_y = -1.0f;
        
        // Sample points along the rose curve
        const int num_samples = 100;
        for (int i = 0; i < num_samples; i++) {
            float alpha = static_cast<float>(i) / (num_samples - 1);
            vec2f point = rose->compute(alpha);
            
            // Update min/max values
            min_x = MIN(min_x, point.x);
            max_x = MAX(max_x, point.x);
            min_y = MIN(min_y, point.y);
            max_y = MAX(max_y, point.y);
            
            // Print every 10th point for visual inspection
            if (i % 10 == 0) {
                MESSAGE("3-petal rose point at alpha=" << alpha << ": (" << point.x << ", " << point.y << ")");
            }
        }
        
        // Print the min/max values
        MESSAGE("\n3-petal rose shape bounds:");
        MESSAGE("X range: [" << min_x << ", " << max_x << "]");
        MESSAGE("Y range: [" << min_y << ", " << max_y << "]");
        
        // Verify the rose is within the expected bounds
        REQUIRE(min_x >= -1.0f);
        REQUIRE(max_x <= 1.0f);
        REQUIRE(min_y >= -1.0f);
        REQUIRE(max_y <= 1.0f);
    }
    
    SUBCASE("4-petal rose") {
        RosePathPtr rose = fl::make_shared<RosePath>(2, 1);  // n=2 gives 4 petals
        
        // Track min and max values to help with scaling
        float min_x = 1.0f;
        float max_x = -1.0f;
        float min_y = 1.0f;
        float max_y = -1.0f;
        
        // Sample points along the rose curve
        const int num_samples = 100;
        for (int i = 0; i < num_samples; i++) {
            float alpha = static_cast<float>(i) / (num_samples - 1);
            vec2f point = rose->compute(alpha);
            
            // Update min/max values
            min_x = MIN(min_x, point.x);
            max_x = MAX(max_x, point.x);
            min_y = MIN(min_y, point.y);
            max_y = MAX(max_y, point.y);
        }
        
        // Verify the rose is within the expected bounds
        REQUIRE(min_x >= -1.0f);
        REQUIRE(max_x <= 1.0f);
        REQUIRE(min_y >= -1.0f);
        REQUIRE(max_y <= 1.0f);
    }
}

TEST_CASE("Check complex types") {
    HeapVector<XYPathPtr> paths;
    XYPathPtr circle = XYPath::NewCirclePath();
    paths.push_back(circle);
    
    // Add heart path to the tests
    XYPathPtr heart = XYPath::NewHeartPath();
    paths.push_back(heart);
    
    // Add spiral path to the tests
    XYPathPtr spiral = XYPath::NewArchimedeanSpiralPath();
    paths.push_back(spiral);
    
    // Add rose path to the tests
    XYPathPtr rose = XYPath::NewRosePath();
    paths.push_back(rose);
    
    // Add phyllotaxis path to the tests
    XYPathPtr phyllotaxis = XYPath::NewPhyllotaxisPath();
    paths.push_back(phyllotaxis);
    
    // paths.push_back(fl::make_intrusive<LissajousPath>());
    // paths.push_back(fl::make_intrusive<GielisCurvePath>());
    // paths.push_back(fl::make_intrusive<CatmullRomPath>());

    SUBCASE("Check floating point range") {
        for (auto &path : paths) {
            for (float alpha = 0.0f; true; alpha += 0.01f) {
                alpha = MIN(1.f, alpha);
                vec2f xy = path->at(alpha);
                REQUIRE(xy.x >= -1.0f);
                REQUIRE(xy.x <= 1.0f);
                REQUIRE(xy.y >= -1.0f);
                REQUIRE(xy.y <= 1.0f);
                if (ALMOST_EQUAL(alpha, 1.0f, 0.001f)) {
                    break;
                }
            }
        }
    }

    SUBCASE("Check float point range with transform to -8,8") {
        TransformFloat tx;
        tx.set_scale(4.0f);

        for (auto &path : paths) {
            for (float alpha = 0.0f; true; alpha += 0.01f) {
                alpha = MIN(1.f, alpha);
                vec2f xy = path->at(alpha, tx);
                REQUIRE_GE(xy.x, -4.0f);
                REQUIRE_LE(xy.x, 4.0f);
                REQUIRE_GE(xy.y, -4.0f);
                REQUIRE_LE(xy.y, 4.0f);
                if (ALMOST_EQUAL(alpha, 1.0f, 0.001f)) {
                    break;
                }
            }
        }
    }

}
