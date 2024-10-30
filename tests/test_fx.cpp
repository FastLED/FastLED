
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <stdint.h>

#include "doctest.h"
#include "fx/1d/cylon.hpp"
#include "fx/1d/demoreel100.hpp"
#include "fx/1d/noisewave.hpp"
#include "fx/1d/pacifica.hpp"
#include "fx/1d/pride2015.hpp" // needs XY defined or linker error.
#include "fx/1d/twinklefox.hpp"
#include "fx/2d/animartrix.hpp"
#include "fx/2d/noisepalette.hpp"
#include "fx/2d/scale_up.hpp"
#include "fx/2d/redsquare.hpp"
#include "fx/2d/video.hpp"

#include "namespace.h"
FASTLED_USING_NAMESPACE

// To satisfy the linker, we must also define uint16_t XY( uint8_t, uint8_t);
// This should go away someday and only use functions supplied by the user.
uint16_t XY(uint8_t, uint8_t) { return 0; }

TEST_CASE("Compile Test") {
    // Suceessful compilation test
}
