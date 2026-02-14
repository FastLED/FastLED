#pragma once

// IWYU pragma: public

#include "fl/stl/stdint.h"

namespace fl {
namespace platforms {

/// Raw platform delay without async task pumping
/// @param ms Milliseconds to delay
void delay(fl::u32 ms);

/// Raw platform microsecond delay
/// @param us Microseconds to delay
void delayMicroseconds(fl::u32 us);

/// Raw platform millisecond timer
/// @return Milliseconds since system startup
fl::u32 millis();

/// Raw platform microsecond timer
/// @return Microseconds since system startup
fl::u32 micros();

}  // namespace platforms
}  // namespace fl
