
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/xypaths.h"
#include "fl/vector.h"
#include <string>

using namespace fl;

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
        circle->buildLut(4);

        uint16_t alpha0 = 0;
        uint16_t alpha1 = 16384;
        uint16_t alpha2 = 32768;
        uint16_t alpha3 = 49152;
        uint16_t alpha4 = 65535;

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

        // xy = circle->at16(alpha4, tx);
        // REQUIRE(xy.x == 0xffff);
        // REQUIRE(xy.y == 0xffff >> 1);
    }


    // SUBCASE("Check uint16 point range with LUT and transform to 0,255") {
    //     Transform16 tx;
    //     tx.scale = 255;
    //     tx.x_offset = 0;
    //     tx.y_offset = 0;

    //     for (auto &path : paths) {
    //         // will build on next at16() call.
    //         path->clearLut(255);
    //     }

    //     for (auto &path : paths) {
    //         for (uint16_t alpha = 0; true; alpha += 1) {
    //             INFO("path=" << std::string(path->name()) << ", alpha=" << alpha);
    //             alpha = MIN(65535, alpha);
    //             pair_xy<uint16_t> xy = path->at16(alpha, tx);
    //             REQUIRE_GE(xy.x, 0);
    //             REQUIRE_LE(xy.x, 255);
    //             REQUIRE_GE(xy.y, 0);
    //             REQUIRE_LE(xy.y, 255);
    //             if (alpha == 65535) {
    //                 break;
    //             }
    //         }
    //     }

    //     // clear the LUTs and reset the steps.
    //     for (auto &path : paths) {
    //         path->clearLut(0);
    //     }
    // }
}
