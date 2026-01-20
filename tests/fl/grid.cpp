
// g++ --std=c++11 test.cpp


#include "fl/grid.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/geometry.h"




TEST_CASE("Grid_int16_t") {
    fl::Grid<int16_t> grid(2, 2);
    REQUIRE_EQ(grid.width(), 2);
    REQUIRE_EQ(grid.height(), 2);
    auto min_max = grid.minMax();

    REQUIRE_EQ(min_max.x, 0);
    REQUIRE_EQ(min_max.y, 0);

    grid.at(0, 0) = 32767;
    REQUIRE_EQ(32767, grid.at(0, 0));
}