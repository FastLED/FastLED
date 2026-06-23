#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

/// Raw platform delay without async task pumping
/// @param ms Milliseconds to delay
void delay(fl::u32 ms) FL_NOEXCEPT;

/// Raw platform microsecond delay
/// @param us Microseconds to delay
void delayMicroseconds(fl::u32 us) FL_NOEXCEPT;

/// Raw platform millisecond timer
/// @return Milliseconds since system startup
fl::u32 millis() FL_NOEXCEPT;

/// Raw platform microsecond timer
/// @return Microseconds since system startup
fl::u32 micros() FL_NOEXCEPT;

}  // namespace platforms
}  // namespace fl
