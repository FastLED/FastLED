
// g++ --std=c++11 test.cpp

#include "fl/stl/stdint.h"
#include "doctest.h"
#include "fl/int.h"


using namespace fl;
uint16_t XY(uint8_t, uint8_t);  // declaration to fix compiler warning.

// To satisfy the linker, we must also define uint16_t XY( uint8_t, uint8_t);
// This should go away someday and only use functions supplied by the user.
uint16_t XY(uint8_t, uint8_t) { return 0; }

TEST_CASE("Compile Test") {
    // Suceessful compilation test
}
