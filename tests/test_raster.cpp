
// g++ --std=c++11 test.cpp

#include "test.h"

#include "FastLED.h"
#include "fl/raster.h"
#include "fl/subpixel.h"
#include "fl/xypath.h"

#include "fl/namespace.h"

using namespace fl;


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
