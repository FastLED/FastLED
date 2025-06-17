// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "lib8tion/intmap.h"
#include "fl/transform.h"
#include "fl/vector.h"
#include "fl/tile2x2.h"
#include <string>

using namespace fl;


TEST_CASE("Tile2x2_u8") {
    Tile2x2_u8 tile;
    tile.setOrigin(1, 1);
    tile.at(0, 0) = 1;
    tile.at(0, 1) = 2;
    tile.at(1, 0) = 3;
    tile.at(1, 1) = 4;

    REQUIRE_EQ(tile.at(0, 0), 1);
    REQUIRE_EQ(tile.at(0, 1), 2);
    REQUIRE_EQ(tile.at(1, 0), 3);
    REQUIRE_EQ(tile.at(1, 1), 4);
}

TEST_CASE("Tile2x2_u8_wrap") {
    Tile2x2_u8 tile;
    tile.setOrigin(1, 1);
    tile.at(0, 0) = 1;
    tile.at(0, 1) = 2;
    tile.at(1, 0) = 3;
    tile.at(1, 1) = 4;

    Tile2x2_u8_wrap wrap(tile, 2, 2);
    REQUIRE_EQ(wrap.at(0, 0).first.x, 1);
    REQUIRE_EQ(wrap.at(0, 0).first.y, 1);
    REQUIRE_EQ(wrap.at(0, 1).first.x, 1);
    REQUIRE_EQ(wrap.at(0, 1).first.y, 0);
    REQUIRE_EQ(wrap.at(1, 0).first.x, 0);
    REQUIRE_EQ(wrap.at(1, 0).first.y, 1);
    REQUIRE_EQ(wrap.at(1, 1).first.x, 0);
    REQUIRE_EQ(wrap.at(1, 1).first.y, 0);
}