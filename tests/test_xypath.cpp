
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
    Tile2x2<float> tile;
    path.at_subpixel(0, &tile);
    REQUIRE_EQ(point_xy<int>(0, 0), tile.origin);
    MESSAGE_TILE(tile);
    REQUIRE_EQ(1.0f, tile.at(0, 0));
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
    // Tests that we can get the correct gaussian values at center point 0,0
    LinePath point(-1.f, -1.f, 1.f, -1.f);
    XYPath path(NewPtrNoTracking(point));
    path.setDrawBounds(3, 3);
    //path.setDrawBounds(3, 3);
    Tile2x2<float> tile;
    path.at_subpixel(0.0f, &tile);
    // MESSAGE_TILE(tile);
    REQUIRE_EQ(tile.origin, point_xy<int>(0, 0));
    REQUIRE_EQ(tile.at(0, 0), 1.0f);


    path.at_subpixel(1.0f, &tile);
    REQUIRE_EQ(tile.origin, point_xy<int>(2, 0));
    REQUIRE_EQ(tile.at(0, 0), 1.0f);

    // just check the x value
    




    // // sweep left to right and get the values.
    // const int y = 1;
    // const int num_steps = 20;
    // const float step = 3.0f / num_steps;

    // for (float x = 0; true; x += step) {
    //     x = MIN(3.f, x);
    //     point_xy_float xy = path.at_subpixel(x, &tile);
    //     //MESSAGE("x: " << x << " y: " << y);
    //     //MESSAGE_TILE_ROW(tile, y);

    //     float row[3] = {};

    //     // now draw.
    //     for (int i = MAX(0, xy.x - 1); i <= MIN(2, xy.x + 1); ++i) {
    //         row[i] = tile.at(i, y);
    //         //MESSAGE("row[" << i << "] = " << row[i]);
    //     }

    //     // now print out the entire row.

    //     MESSAGE("Row at " << x <<  " => " << row[0] << "," << row[1] << "," << row[2]);


    //     if (x >= 3.0f) {
    //         break;
    //     }


    //     // REQUIRE_EQ(xy.x, x);
    //     // REQUIRE_EQ(xy.y, y);
    // }
    // MESSAGE("Done!");
}

// TEST_CASE("LinePath at_subpixel slightly off center") {
//     // Tests that we can get the correct gaussian values at center point 0,0
//     PointPath point(0.1f, 0.0f);
//     XYPath path(NewPtrNoTracking(point));
//     Tile2x2<float> tile;
//     point_xy_float xy = path.at_subpixel(0.5f, &tile);
//     MESSAGE_TILE(tile);
//     REQUIRE_EQ(xy, point_xy_float(0.f, 0.f));
// }


TEST_CASE("Check complex types") {
    HeapVector<XYPathPtr> paths;
    XYPathPtr circle = XYPath::NewCirclePath();
    paths.push_back(circle);

    
    // paths.push_back(HeartPathPtr::New());
    // paths.push_back(LissajousPathPtr::New());
    // paths.push_back(ArchimedeanSpiralPathPtr::New());
    // paths.push_back(RosePathPtr::New());
    // paths.push_back(PhyllotaxisPathPtr::New());
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
        // float nearly_one = 1.0f - 0.0001f;
        tx.scale_x = 4.0f;
        tx.scale_y = 4.0f;


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
