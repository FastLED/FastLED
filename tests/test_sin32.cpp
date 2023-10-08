
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/sin32.h"

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE

// 16777216 is 1 cycle
const uint32_t _360 = 16777216;
const uint32_t _ONE = 2147418112;
const uint32_t _NEG_ONE = -2147418112;

TEST_CASE("compile test") {
    int32_t result = sin32(0);
    REQUIRE(result == 0);

    result = sin32(_360);
    REQUIRE(result == 0);

    result = sin32(_360 / 4);
    REQUIRE(result == _ONE);

    result = sin32(_360 / 2);
    REQUIRE(result == 0);

    result = sin32(_360 / 4 * 3);
    REQUIRE(result == _NEG_ONE);
    
}
