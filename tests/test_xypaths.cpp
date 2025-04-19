
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/xypaths.h"
#include "fl/vector.h"

using namespace fl;

TEST_CASE("LinePath") {
    LinePath path(0.0f, 0.0f, 1.0f, 1.0f);
    pair_xy_float xy = path.at(0.5f);
    CHECK(xy.x == 0.5f);
    CHECK(xy.y == 0.5f);

    xy = path.at(1.0f);
    CHECK(xy.x == 1.0f);
    CHECK(xy.y == 1.0f);

    xy = path.at(0.0f);
    CHECK(xy.x == 0.0f);
    CHECK(xy.y == 0.0f);
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
                CHECK(xy.x >= 0.0f);
                CHECK(xy.x <= 1.0f);
                CHECK(xy.y >= 0.0f);
                CHECK(xy.y <= 1.0f);
                if (ALMOST_EQUAL(alpha, 1.0f, 0.001f)) {
                    break;
                }
            }
        }
    }
}
