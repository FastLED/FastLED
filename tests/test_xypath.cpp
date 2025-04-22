
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
    point_xy_float xy = path.compute(0.5f);
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
    LinePath line(-1.0f, -1.0f, 1.0f, -1.0f);
    XYPath path(NewPtrNoTracking(line));
    path.setDrawBounds(2,2);
    SubPixel2x2 tile = path.at_subpixel(0);
    REQUIRE_EQ(point_xy<uint16_t>(0, 0), tile.origin());
    MESSAGE_TILE(tile);
    REQUIRE_EQ(255, tile.at(0, 0));
}

TEST_CASE("LinePath simple float sweep") {
    // Tests that we can get the correct gaussian values at center point 0,0
    LinePath point(0, 1.f, 1.f, 1.f);
    XYPath path(NewPtrNoTracking(point));
    auto xy = path.at(0);
    //MESSAGE_TILE(tile);
    REQUIRE_EQ(xy, point_xy_float(0.0f, 1.f));
    xy = path.at(1);
    REQUIRE_EQ(xy, point_xy_float(1.f, 1.f));
}

TEST_CASE("Point at exactly the middle") {
    // Tests that we can get the correct gaussian values at center point 0,0
    PointPath point(0.0, 0.0);  // Right in middle.
    XYPath path(NewPtrNoTracking(point));
    path.setDrawBounds(2,2);
    // auto xy = path.at(0);
    fl::SubPixel2x2 sp = path.at_subpixel(0);
    //MESSAGE_TILE(tile);
    // REQUIRE_EQ(point_xy_float(0.0f, 0.f), sp);
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
    LinePath point(-1.f, -1.f, 1.f, -1.f);
    XYPath path(NewPtrNoTracking(point));
    int width = 2;
    path.setDrawBounds(width, width);
    auto begin = path.at(0);
    auto end = path.at(1);
    REQUIRE_EQ(point_xy_float(0.5f, 0.5f), begin);
    REQUIRE_EQ(point_xy_float(1.5f, 0.5f), end);
}

TEST_CASE("LinePath at_subpixel moves x") {
    // Tests that we can get the correct subpixel.
    LinePath point(-1.f, -1.f, 1.f, -1.f);
    XYPath path(NewPtrNoTracking(point));
    path.setDrawBounds(3, 3);
    SubPixel2x2 tile = path.at_subpixel(0.0f);
    // MESSAGE_TILE(tile);
    REQUIRE_EQ(tile.origin(), point_xy<uint16_t>(0, 0));
    REQUIRE_EQ(tile.at(0, 0), 255);
    tile = path.at_subpixel(1.0f);
    REQUIRE_EQ(tile.origin(), point_xy<uint16_t>(2, 0));
    REQUIRE_EQ(tile.at(0, 0), 255);
}


TEST_CASE("Test HeartPath") {
    HeartPathPtr heart = HeartPathPtr::New();
    
    // Track min and max values to help with scaling
    float min_x = 1.0f;
    float max_x = -1.0f;
    float min_y = 1.0f;
    float max_y = -1.0f;
    
    // Sample points along the heart curve
    const int num_samples = 100;
    for (int i = 0; i < num_samples; i++) {
        float alpha = static_cast<float>(i) / (num_samples - 1);
        point_xy_float point = heart->compute(alpha);
        
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
    ArchimedeanSpiralPathPtr spiral = ArchimedeanSpiralPathPtr::New(3, 1.0f);
    
    // Track min and max values to help with scaling
    float min_x = 1.0f;
    float max_x = -1.0f;
    float min_y = 1.0f;
    float max_y = -1.0f;
    
    // Sample points along the spiral curve
    const int num_samples = 100;
    for (int i = 0; i < num_samples; i++) {
        float alpha = static_cast<float>(i) / (num_samples - 1);
        point_xy_float point = spiral->compute(alpha);
        
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
        RosePathPtr rose = RosePathPtr::New(3, 1);
        
        // Track min and max values to help with scaling
        float min_x = 1.0f;
        float max_x = -1.0f;
        float min_y = 1.0f;
        float max_y = -1.0f;
        
        // Sample points along the rose curve
        const int num_samples = 100;
        for (int i = 0; i < num_samples; i++) {
            float alpha = static_cast<float>(i) / (num_samples - 1);
            point_xy_float point = rose->compute(alpha);
            
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
        RosePathPtr rose = RosePathPtr::New(2, 1);  // n=2 gives 4 petals
        
        // Track min and max values to help with scaling
        float min_x = 1.0f;
        float max_x = -1.0f;
        float min_y = 1.0f;
        float max_y = -1.0f;
        
        // Sample points along the rose curve
        const int num_samples = 100;
        for (int i = 0; i < num_samples; i++) {
            float alpha = static_cast<float>(i) / (num_samples - 1);
            point_xy_float point = rose->compute(alpha);
            
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
    
    // paths.push_back(LissajousPathPtr::New());
    // paths.push_back(GielisCurvePathPtr::New());
    // paths.push_back(CatmullRomPathPtr::New());

    SUBCASE("Check floating point range") {
        for (auto &path : paths) {
            for (float alpha = 0.0f; true; alpha += 0.01f) {
                alpha = MIN(1.f, alpha);
                point_xy_float xy = path->at(alpha);
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
                point_xy_float xy = path->at(alpha, tx);
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

    SUBCASE("Check uint16 point range") {
        for (auto &path : paths) {
            for (uint32_t alpha = 0; true; alpha += 100) {
                alpha = MIN(65535, alpha);
                point_xy<uint16_t> xy = path->at16(static_cast<uint16_t>(alpha));
                REQUIRE(xy.x >= 0);
                REQUIRE(xy.x <= 65535);
                REQUIRE(xy.y >= 0);
                REQUIRE(xy.y <= 65535);
                if (alpha == 65535) {
                    break;
                }
            }
        }
    }

    SUBCASE("Check uint16 point range with transform to 0,255") {
        Transform16 tx;
        tx.scale_x = 255;
        tx.scale_y = 255;
        tx.x_offset = 0;
        tx.y_offset = 0;

        for (auto &path : paths) {
            for (uint32_t alpha = 0; true; alpha += 10) {
                alpha = MIN(65535, alpha);
                uint16_t alpha16 = static_cast<uint16_t>(alpha);
                point_xy<uint16_t> xy = path->at16(alpha16, tx);
                REQUIRE_GE(xy.x, 0);
                REQUIRE_LE(xy.x, 255);
                REQUIRE_GE(xy.y, 0);
                REQUIRE_LE(xy.y, 255);
                if (alpha == 65535) {
                    break;
                }
            }
        }
    }

    #if 0
    SUBCASE("Circle works with LUT") {
        XYPathPtr circle = XYPath::NewCirclePath();
        Transform16 tx;
        circle->buildLut(5);  // circle->at16(0) == circle->at16(65535)
        auto lut = circle->getLut();
        REQUIRE_EQ(lut->size(), 5);
        REQUIRE(lut != nullptr);
        for (uint16_t i = 0; i < 4; i++) {
            point_xy<uint16_t> xy = lut->getData()[i];
            std::cout << "lut[" << i << "] = " << xy.x << ", " << xy.y
                      << std::endl;
        }

        point_xy<uint16_t> expected_xy0(65535, 32767);
        point_xy<uint16_t> expected_xy1(32767, 65535);
        point_xy<uint16_t> expected_xy2(0, 32767);
        point_xy<uint16_t> expected_xy3(32767, 0);

        
        REQUIRE_EQ(expected_xy0, lut->getData()[0]);
        REQUIRE_EQ(expected_xy1, lut->getData()[1]);
        REQUIRE_EQ(expected_xy2, lut->getData()[2]);
        REQUIRE_EQ(expected_xy3, lut->getData()[3]);

        uint16_t alpha0 = 0;
        uint16_t alpha1 = 16384;
        uint16_t alpha2 = 32768;
        uint16_t alpha3 = 49152;
        uint16_t alpha4 = 65535;

        // test LUT interpolation

        REQUIRE_EQ(lut->interp16(alpha0), expected_xy0);
        REQUIRE_EQ(lut->interp16(alpha1), expected_xy1);
        REQUIRE_EQ(lut->interp16(alpha2), expected_xy2);
        REQUIRE_EQ(lut->interp16(alpha3), expected_xy3);

        point_xy<uint16_t> xy0 = circle->at16(alpha0, tx);
        point_xy<uint16_t> xy1 = circle->at16(alpha1, tx);
        point_xy<uint16_t> xy2 = circle->at16(alpha2, tx);
        point_xy<uint16_t> xy3 = circle->at16(alpha3, tx);

        FASTLED_UNUSED(xy0);
        FASTLED_UNUSED(xy1);
        FASTLED_UNUSED(xy2);
        FASTLED_UNUSED(xy3);

        point_xy <uint16_t> xy = circle->at16(alpha0, tx);
        REQUIRE(xy.x == 0xffff);
        REQUIRE(xy.y == 0xffff >> 1);

        xy = circle->at16(alpha1,tx);
        REQUIRE(xy.x == 0xffff >> 1);
        REQUIRE(xy.y == 0xffff);

        xy = circle->at16(alpha2, tx);
        REQUIRE(xy.x == 0);
        REQUIRE(xy.y == 0xffff >> 1);

        xy = circle->at16(alpha3, tx);
        REQUIRE(xy.x == 0xffff >> 1);
        REQUIRE(xy.y == 0);

        xy = circle->at16(alpha4, tx);
        REQUIRE(xy.x == 0xffff);
        REQUIRE(xy.y == 0xffff >> 1);
    }

    SUBCASE("Circle works with LUT and rotation") {
        Transform16 tx;
        tx.scale_x = 255;
        tx.scale_y = 255;
        tx.x_offset = 0;
        tx.y_offset = 0;
        tx.rotation = 32768; // rotate by 180 degrees

        XYPathPtr circle = XYPath::NewCirclePath();
        circle->buildLut(5);  // circle->at16(0) == circle->at16(65535)
        auto lut = circle->getLut();
        REQUIRE_EQ(lut->size(), 5);
        REQUIRE(lut != nullptr);

        point_xy<uint16_t> expected_xy0(0, 127);
        point_xy<uint16_t> expected_xy1(127, 0);
        point_xy<uint16_t> expected_xy2(255, 127);
        point_xy<uint16_t> expected_xy3(127, 255);
        point_xy<uint16_t> expected_xy4(0, 127);

        point_xy<uint16_t> xy0 = circle->at16(0, tx);
        point_xy<uint16_t> xy1 = circle->at16(16384, tx);
        point_xy<uint16_t> xy2 = circle->at16(32768, tx);
        point_xy<uint16_t> xy3 = circle->at16(49152, tx);
        point_xy<uint16_t> xy4 = circle->at16(65535, tx);
        REQUIRE_EQ(expected_xy0, xy0);
        REQUIRE_EQ(expected_xy1, xy1);
        REQUIRE_EQ(expected_xy2, xy2);
        REQUIRE_EQ(expected_xy3, xy3);
        REQUIRE_EQ(expected_xy4, xy4);
    }

    SUBCASE("Circle with LUT and transform") {
        Transform16 tx;
        tx.scale_x = 255;
        tx.scale_y = 255;
        tx.x_offset = 0;
        tx.y_offset = 0;

        XYPathPtr circle = XYPath::NewCirclePath();
        circle->buildLut(5);  // circle->at16(0) == circle->at16(65535)
        auto lut = circle->getLut();
        REQUIRE_EQ(lut->size(), 5);
        REQUIRE(lut != nullptr);

        point_xy<uint16_t> expected_xy0(255, 127);
        point_xy<uint16_t> expected_xy1(127, 255);
        point_xy<uint16_t> expected_xy2(0, 127);
        point_xy<uint16_t> expected_xy3(127, 0);
        point_xy<uint16_t> expected_xy4(255, 127);

        point_xy<uint16_t> xy0 = circle->at16(0, tx);
        point_xy<uint16_t> xy1 = circle->at16(16384, tx);
        point_xy<uint16_t> xy2 = circle->at16(32768, tx);
        point_xy<uint16_t> xy3 = circle->at16(49152, tx);
        point_xy<uint16_t> xy4 = circle->at16(65535, tx);
        REQUIRE_EQ(expected_xy0, xy0);
        REQUIRE_EQ(expected_xy1, xy1);
        REQUIRE_EQ(expected_xy2, xy2);
        REQUIRE_EQ(expected_xy3, xy3);
        REQUIRE_EQ(expected_xy4, xy4);
    }


    SUBCASE("Check uint16 point range with LUT and transform to 0,255") {
        Transform16 tx;
        tx.scale_x = 255;
        tx.scale_y = 255;
        tx.x_offset = 0;
        tx.y_offset = 0;

        for (auto &path : paths) {
            path->clearLut(255);
        }

        for (auto &path : paths) {
            for (uint32_t alpha = 0; true; alpha += 10) {
                alpha = MIN(65535, alpha);
                uint16_t alpha16 = static_cast<uint16_t>(alpha);
                point_xy<uint16_t> xy = path->at16(alpha16, tx);
                REQUIRE_GE(xy.x, 0);
                REQUIRE_LE(xy.x, 255);
                REQUIRE_GE(xy.y, 0);
                REQUIRE_LE(xy.y, 255);
                if (alpha == 65535) {
                    break;
                }
            }
        }

        // clear the LUTs and reset the steps.
        for (auto &path : paths) {
            path->clearLut(0);
        }
    }
    #endif
}
