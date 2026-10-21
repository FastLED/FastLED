// IWYU pragma: private

/// @file fl/system/analog/analog_read.cpp.hpp
/// @brief Non-inline `fl::analogRead` wrapper, in its own unity object so the
/// platform ADC driver is linked only when a sketch reads an analog pin
/// (FastLED #4796). See `fl/system/pin.cpp.hpp` for the wrapper pattern.

#include "fl/system/analog/analog_read.h"

// Include platform-specific implementations (must come after fl/pin.h for proper type resolution)
#include "platforms/pin.h"

#include "fl/stl/noexcept.h"

namespace fl {

u16 analogRead(int pin) FL_NO_EXCEPT {
    return platforms::analogRead(pin);
}

}  // namespace fl
