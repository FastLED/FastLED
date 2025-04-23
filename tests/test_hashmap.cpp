
// g++ --std=c++11 test.cpp

#include "test.h"


#include "fl/hashmap.h"


using namespace fl;


TEST_CASE("Test hashmap simple") {
    HashMap<int,int> map;
    REQUIRE_EQ(map.size(), 0u);

    map.insert(10, 20);
    REQUIRE_EQ(map.size(), 1u);

    auto *val = map.find(10);
    REQUIRE(val);
    REQUIRE_EQ(*val, 20);
}