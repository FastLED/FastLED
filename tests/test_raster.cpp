
// g++ --std=c++11 test.cpp

#include "test.h"


#include "FastLED.h"
#include "fl/raster.h"
#include "fl/subpixel.h"
#include "fl/xypath.h"

#include "fl/namespace.h"

using namespace fl;


TEST_CASE("Raster simple test") {
    XYPathPtr path = XYPath::NewPointPath(0, 0);
    path->setDrawBounds(2, 2);
    SubPixel2x2 subpixel = path->at_subpixel(0);
    Raster raster;
    SubPixel2x2::Rasterize(Slice<SubPixel2x2>(&subpixel, 1), &raster);

    point_xy<uint16_t> origin = subpixel.origin();
    point_xy<uint16_t> global_min = raster.global_min();
    point_xy<uint16_t> global_max = raster.global_max();
    REQUIRE_EQ(global_min, origin);
}