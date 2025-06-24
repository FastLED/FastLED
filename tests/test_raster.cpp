
// g++ --std=c++11 test.cpp

#include "test.h"

#include "FastLED.h"
#include "fl/raster.h"
#include "fl/tile2x2.h"
#include "fl/xypath.h"

#include "fl/namespace.h"

using namespace fl;


TEST_CASE("XYRasterU8SparseTest should match bounds of pixels draw area") {
    XYPathPtr path = XYPath::NewLinePath(-1, -1, 1, 1);
    path->setDrawBounds(4,4);
    Tile2x2_u8 sp0 = path->at_subpixel(0);
    Tile2x2_u8 sp1 = path->at_subpixel(1);
    Tile2x2_u8 subpixels[2] = {sp0, sp1};

    MESSAGE("subpixels[0] = " << subpixels[0]);
    MESSAGE("subpixels[1] = " << subpixels[1]);
    XYRasterU8Sparse raster(4, 4);
    raster.rasterize(subpixels);
    auto obligatory_bounds = raster.bounds();
    REQUIRE_EQ(rect<uint16_t>(0, 0, 4, 4), obligatory_bounds);

    auto pixel_bounds = raster.bounds_pixels();
    REQUIRE_EQ(rect<uint16_t>(0, 0, 4, 4), pixel_bounds);
}
