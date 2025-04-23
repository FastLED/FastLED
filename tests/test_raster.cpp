
// g++ --std=c++11 test.cpp

#include "test.h"

#include "FastLED.h"
#include "fl/raster.h"
#include "fl/subpixel.h"
#include "fl/xypath.h"

#include "fl/namespace.h"

using namespace fl;

TEST_CASE("XYRasterDense simple test") {
    XYPathPtr path = XYPath::NewPointPath(0, 0);
    path->setDrawBounds(2, 2);
    SubPixel2x2 subpixel = path->at_subpixel(0);
    XYRasterDense raster(3, 3);
    SubPixel2x2::Rasterize(Slice<SubPixel2x2>(&subpixel, 1), &raster);

    point_xy<int> origin = subpixel.origin();
    point_xy<int> global_min = raster.global_min();
    point_xy<int> global_max = raster.global_max();
    REQUIRE_EQ(global_min, origin);
    REQUIRE_EQ(global_max, origin + point_xy<int>(2, 2));

    // now test that each point matches.
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            uint8_t v1 = subpixel.at(x, y);
            uint8_t v2 = raster.at(x, y);
            REQUIRE_EQ(v1, v2);
        }
    }
}

TEST_CASE("XYRasterDense two unit test") {
    XYPathPtr path = XYPath::NewLinePath(-1, -1, 1, 1);
    path->setDrawBounds(4, 4);
    CHECK_EQ(point_xy_float(0.5f, 0.5f), path->at(0.f));
    CHECK_EQ(point_xy_float(2.0f, 2.0f), path->at(0.5f));
    CHECK_EQ(point_xy_float(3.5f, 3.5f), path->at(1.f));
    SubPixel2x2 sp0 = path->at_subpixel(0);
    SubPixel2x2 sp1 = path->at_subpixel(1);
    SubPixel2x2 subpixels[2] = {sp0, sp1};
    XYRasterDense raster(4, 4);
    raster.rasterize(subpixels);
    auto b = raster.bounds();
    REQUIRE_EQ(rect_xy<uint16_t>(0, 0, 4, 4), b);
    auto w = raster.width();
    auto h = raster.height();
    REQUIRE_EQ(4, w);
    REQUIRE_EQ(4, h);
}



TEST_CASE("XYRasterDense with empty draw bounds should prevent any elements") {
    XYPathPtr path = XYPath::NewLinePath(-1, -1, 1, 1);
    path->setDrawBounds(-4,4);
    SubPixel2x2 sp0 = path->at_subpixel(0);
    SubPixel2x2 sp1 = path->at_subpixel(1);
    SubPixel2x2 subpixels[2] = {sp0, sp1};
    XYRasterDense raster(4, 4);
    auto bstart = raster.bounds();
    REQUIRE_EQ(rect_xy<uint16_t>(0, 0, 0, 0), bstart);
    rect_xy<int> empty;
    SubPixel2x2::Rasterize(subpixels, &raster, &empty);
    auto b = raster.bounds();
    REQUIRE_EQ(rect_xy<uint16_t>(0, 0, 0, 0), b);
}


TEST_CASE("XYRasterSparseTest with empty draw bounds should prevent any elements") {
    XYPathPtr path = XYPath::NewLinePath(-1, -1, 1, 1);
    path->setDrawBounds(4,4);
    SubPixel2x2 sp0 = path->at_subpixel(0);
    SubPixel2x2 sp1 = path->at_subpixel(1);
    SubPixel2x2 subpixels[2] = {sp0, sp1};

    MESSAGE("subpixels[0] = " << subpixels[0]);
    MESSAGE("subpixels[1] = " << subpixels[1]);
    XYRasterSparse raster(4, 4);
    auto bstart = raster.bounds();
    REQUIRE_EQ(rect_xy<uint16_t>(0, 0, 0, 0), bstart);
    raster.rasterize(subpixels);
    auto b = raster.bounds();
    REQUIRE_EQ(rect_xy<uint16_t>(0, 0, 3, 3), b);
}