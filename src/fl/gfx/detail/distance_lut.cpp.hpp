/// @file distance_lut.cpp.hpp
/// @brief Single definition of distance antialiasing LUT
/// This is included in the unity build via fl/gfx/_build.cpp.hpp

#pragma once

#include "fl/gfx/detail/distance_lut.h"
#include "fastled_progmem.h"

namespace fl {
namespace gfx {
namespace detail {

/// @brief 256-byte antialiasing lookup table
/// Maps normalized squared distance d²/r_max² (scaled 0..255) to brightness byte
/// Entry i = round(255 * max(0, 1 - sqrt(i/255)))
/// Provides smooth antialiasing fringe for circles and discs
const fl::u8 distanceAA_LUT[256] FL_ALIGN_PROGMEM(256) FL_PROGMEM = {
    255,239,232,227,223,219,216,213,210,207,205,202,200,197,195,193,
    191,189,187,185,184,182,180,178,177,175,174,172,171,169,168,166,
    165,163,162,161,159,158,157,155,154,153,152,150,149,148,147,146,
    144,143,142,141,140,139,138,137,136,134,133,132,131,130,129,128,
    127,126,125,124,123,122,121,120,120,119,118,117,116,115,114,113,
    112,111,110,110,109,108,107,106,105,104,104,103,102,101,100, 99,
     99, 98, 97, 96, 95, 95, 94, 93, 92, 91, 91, 90, 89, 88, 88, 87,
     86, 85, 85, 84, 83, 82, 82, 81, 80, 79, 79, 78, 77, 76, 76, 75,
     74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67, 67, 66, 65, 65, 64,
     63, 63, 62, 61, 61, 60, 59, 59, 58, 57, 57, 56, 56, 55, 54, 54,
     53, 52, 52, 51, 51, 50, 49, 49, 48, 47, 47, 46, 46, 45, 44, 44,
     43, 43, 42, 41, 41, 40, 40, 39, 38, 38, 37, 37, 36, 35, 35, 34,
     34, 33, 33, 32, 31, 31, 30, 30, 29, 29, 28, 27, 27, 26, 26, 25,
     25, 24, 24, 23, 22, 22, 21, 21, 20, 20, 19, 19, 18, 18, 17, 17,
     16, 15, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10,  9,  9,  8,
      8,  7,  7,  6,  6,  5,  5,  4,  4,  3,  3,  2,  2,  1,  1,  0
};

}  // namespace detail
}  // namespace gfx
}  // namespace fl
