
// g++ --std=c++11 test.cpp


#include "fl/tile2x2.h"
#include "fl/xypath.h"
#include "fl/stl/stdint.h"
#include "doctest.h"
#include "fl/geometry.h"
#include "fl/raster_sparse.h"
#include "fl/slice.h"

TEST_CASE("XYRasterU8SparseTest should match bounds of pixels draw area") {
    fl::XYPathPtr path = fl::XYPath::NewLinePath(-1, -1, 1, 1);
    path->setDrawBounds(4,4);
    fl::Tile2x2_u8 sp0 = path->at_subpixel(0);
    fl::Tile2x2_u8 sp1 = path->at_subpixel(1);
    fl::Tile2x2_u8 subpixels[2] = {sp0, sp1};

    MESSAGE("subpixels[0] = " << subpixels[0]);
    MESSAGE("subpixels[1] = " << subpixels[1]);
    fl::XYRasterU8Sparse raster(4, 4);
    raster.rasterize(subpixels);
    auto obligatory_bounds = raster.bounds();
    REQUIRE_EQ(fl::rect<uint16_t>(0, 0, 4, 4), obligatory_bounds);

    auto pixel_bounds = raster.bounds_pixels();
    REQUIRE_EQ(fl::rect<uint16_t>(0, 0, 4, 4), pixel_bounds);
}
