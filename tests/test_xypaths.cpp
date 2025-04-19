
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "lib8tion/intmap.h"
#include "fl/xypaths.h"
#include "fl/vector.h"
#include "fl/unused.h"
#include <string>

using namespace fl;


TEST_CASE("Transform16::ToBounds") {
    Transform16 tx = Transform16::ToBounds(255);

    // SUBCASE("Check bounds at 128") {
    //     // known bad at i == 128
    //     uint16_t i16 = map8_to_16(128);
    //     pair_xy<uint16_t> xy_input = pair_xy<uint16_t>(i16, i16);
    //     pair_xy<uint16_t> xy = tx.transform(xy_input);
    //     INFO("i = " << 128);
    //     REQUIRE_EQ(128, xy.x);
    //     REQUIRE_EQ(128, xy.y);
    // }

    SUBCASE("Check all bounds are in 255") {
        for (uint16_t i = 0; i < 256; i++) {
            uint16_t i16 = map8_to_16(i);
            pair_xy<uint16_t> xy_input = pair_xy<uint16_t>(i16, i16);
            pair_xy<uint16_t> xy = tx.transform(xy_input);
            INFO("i = " << i);
            REQUIRE_LE(xy.x, 255);
            REQUIRE_LE(xy.y, 255);
        }
    }
}

TEST_CASE("LinePath") {
    LinePath path(0.0f, 0.0f, 1.0f, 1.0f);
    pair_xy_float xy = path.at(0.5f);
    REQUIRE(xy.x == 0.5f);
    REQUIRE(xy.y == 0.5f);

    xy = path.at(1.0f);
    REQUIRE(xy.x == 1.0f);
    REQUIRE(xy.y == 1.0f);

    xy = path.at(0.0f);
    REQUIRE(xy.x == 0.0f);
    REQUIRE(xy.y == 0.0f);
}


TEST_CASE("Check complex types") {
    HeapVector<XYPathPtr> paths;
    paths.push_back(CirclePathPtr::New());
    paths.push_back(HeartPathPtr::New());
    paths.push_back(LissajousPathPtr::New());
    paths.push_back(ArchimedeanSpiralPathPtr::New());
    paths.push_back(RosePathPtr::New());
    paths.push_back(PhyllotaxisPathPtr::New());
    paths.push_back(GielisCurvePathPtr::New());
    paths.push_back(CatmullRomPathPtr::New());

    SUBCASE("Check floating point range") {
        for (auto &path : paths) {
            for (float alpha = 0.0f; true; alpha += 0.01f) {
                alpha = MIN(1.f, alpha);
                pair_xy_float xy = path->at(alpha);
                REQUIRE(xy.x >= 0.0f);
                REQUIRE(xy.x <= 1.0f);
                REQUIRE(xy.y >= 0.0f);
                REQUIRE(xy.y <= 1.0f);
                if (ALMOST_EQUAL(alpha, 1.0f, 0.001f)) {
                    break;
                }
            }
        }
    }

    SUBCASE("Check float point range with transform to -8,8") {
        TransformFloat tx;
        tx.scale = 8.0f;
        tx.x_offset = -4.0f;
        tx.y_offset = -4.0f;

        for (auto &path : paths) {
            for (float alpha = 0.0f; true; alpha += 0.01f) {
                alpha = MIN(1.f, alpha);
                pair_xy_float xy = path->at(alpha, tx);
                REQUIRE(xy.x >= -4.0f);
                REQUIRE(xy.x <= 4.0f);
                REQUIRE(xy.y >= -4.0f);
                REQUIRE(xy.y <= 4.0f);
                if (ALMOST_EQUAL(alpha, 1.0f, 0.001f)) {
                    break;
                }
            }
        }
    }

    SUBCASE("Check uint16 point range") {
        for (auto &path : paths) {
            for (uint16_t alpha = 0; true; alpha += 1) {
                alpha = MIN(65535, alpha);
                pair_xy<uint16_t> xy = path->at16(alpha);
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
        tx.scale = 255;
        tx.x_offset = 0;
        tx.y_offset = 0;

        for (auto &path : paths) {
            for (uint16_t alpha = 0; true; alpha += 1) {
                alpha = MIN(65535, alpha);
                pair_xy<uint16_t> xy = path->at16(alpha, tx);
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

    SUBCASE("Circle works with LUT") {
        CirclePathPtr circle = CirclePathPtr::New();
        Transform16 tx;
        circle->buildLut(5);  // circle->at16(0) == circle->at16(65535)
        auto lut = circle->getLut();
        REQUIRE_EQ(lut->size(), 5);
        REQUIRE(lut != nullptr);
        for (uint16_t i = 0; i < 4; i++) {
            pair_xy<uint16_t> xy = lut->getData()[i];
            std::cout << "lut[" << i << "] = " << xy.x << ", " << xy.y
                      << std::endl;
        }

        pair_xy<uint16_t> expected_xy0(65535, 32767);
        pair_xy<uint16_t> expected_xy1(32767, 65535);
        pair_xy<uint16_t> expected_xy2(0, 32767);
        pair_xy<uint16_t> expected_xy3(32767, 0);

        
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

        pair_xy<uint16_t> xy0 = circle->at16(alpha0, tx);
        pair_xy<uint16_t> xy1 = circle->at16(alpha1, tx);
        pair_xy<uint16_t> xy2 = circle->at16(alpha2, tx);
        pair_xy<uint16_t> xy3 = circle->at16(alpha3, tx);

        FASTLED_UNUSED(xy0);
        FASTLED_UNUSED(xy1);
        FASTLED_UNUSED(xy2);
        FASTLED_UNUSED(xy3);

        pair_xy <uint16_t> xy = circle->at16(alpha0, tx);
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

    SUBCASE("Circle with LUT and transform") {
        Transform16 tx;
        tx.scale = 255;
        tx.x_offset = 0;
        tx.y_offset = 0;

        CirclePathPtr circle = CirclePathPtr::New();
        circle->buildLut(5);  // circle->at16(0) == circle->at16(65535)
        auto lut = circle->getLut();
        REQUIRE_EQ(lut->size(), 5);
        REQUIRE(lut != nullptr);

        pair_xy<uint16_t> expected_xy0(255, 127);
        pair_xy<uint16_t> expected_xy1(127, 255);
        pair_xy<uint16_t> expected_xy2(0, 127);
        pair_xy<uint16_t> expected_xy3(127, 0);
        pair_xy<uint16_t> expected_xy4(255, 127);

        pair_xy<uint16_t> xy0 = circle->at16(0, tx);
        pair_xy<uint16_t> xy1 = circle->at16(16384, tx);
        pair_xy<uint16_t> xy2 = circle->at16(32768, tx);
        pair_xy<uint16_t> xy3 = circle->at16(49152, tx);
        pair_xy<uint16_t> xy4 = circle->at16(65535, tx);
        REQUIRE_EQ(expected_xy1, xy1);
        REQUIRE_EQ(expected_xy2, xy2);
        REQUIRE_EQ(expected_xy3, xy3);
        REQUIRE_EQ(expected_xy4, xy4);
    }


    SUBCASE("Check uint16 point range with LUT and transform to 0,255") {
        Transform16 tx;
        tx.scale = 255;
        tx.x_offset = 0;
        tx.y_offset = 0;

        for (auto &path : paths) {
            path->clearLut(255);
        }

        for (auto &path : paths) {
            for (uint16_t alpha = 0; true; alpha += 1) {
                alpha = MIN(65535, alpha);
                pair_xy<uint16_t> xy = path->at16(alpha, tx);
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
}
