#pragma once

/// @file fl/math/squarewave.h
/// Square wave generator function.

#include "fl/stl/int.h"
#include "fl/math/lib8static.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @ingroup WaveformGenerators
/// @{

/// Square wave generator.
///   if in < pulsewidth then output is 255
///   if in >= pulsewidth then output is 0
///
/// @param in input value
/// @param pulsewidth width of the output pulse
/// @returns square wave output
LIB8STATIC u8 squarewave8(u8 in, u8 pulsewidth = 128) FL_NOEXCEPT {
    if (in < pulsewidth || (pulsewidth == 255)) {
        return 255;
    } else {
        return 0;
    }
}

/// @}

} // namespace fl
