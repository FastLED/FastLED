
// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/stdint.h"

#include "test.h"
#include "fx/1d/cylon.h"
#include "fx/1d/demoreel100.h"
#include "fx/1d/noisewave.h"
#include "fx/1d/pacifica.h"
#include "fx/1d/pride2015.h" // needs XY defined or linker error.
#include "fx/1d/twinklefox.h"
#include "fx/2d/animartrix.hpp"
#include "fx/2d/noisepalette.h"
#include "fx/2d/scale_up.h"
#include "fx/2d/redsquare.h"
#include "fx/video.h"

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE

uint16_t XY(uint8_t, uint8_t);  // declaration to fix compiler warning.

// To satisfy the linker, we must also define uint16_t XY( uint8_t, uint8_t);
// This should go away someday and only use functions supplied by the user.
uint16_t XY(uint8_t, uint8_t) { return 0; }

TEST_CASE("Compile Test") {
    // Suceessful compilation test
}
