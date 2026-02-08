#pragma once

// Embedded Covenant5x5 font (9.9 KB, 5x5 pixel font)
// License: CC BY 4.0 - https://heraldod.itch.io/bitmap-fonts

#include "fl/stl/span.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace ttf {

// Get the embedded Covenant5x5 TTF font data as a span
fl::span<const u8> covenant5x5();

} // namespace ttf
} // namespace fl
