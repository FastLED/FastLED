#pragma once

#include "fl/stl/stdint.h"  // IWYU pragma: keep
#include "fastled_progmem.h"

namespace fl {
namespace gfx {
namespace detail {

/// @brief Distance antialiasing lookup table
/// Maps normalized squared distance d²/r_max² (scaled 0..255) to brightness byte
/// Entry i = round(255 * max(0, 1 - sqrt(i/255)))
/// PROGMEM on embedded platforms, regular array on host
extern const fl::u8 distanceAA_LUT[256] FL_PROGMEM;

}  // namespace detail
}  // namespace gfx
}  // namespace fl
