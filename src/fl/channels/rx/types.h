#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

enum class RxBackend : u8 {
    PLATFORM_DEFAULT = 0,  ///< Use the recommended backend for the active platform (currently RMT on ESP32, FlexPWM on Teensy 4.x, native RX on stub/host builds).
    RMT = 1,               ///< ESP32-only RMT capture backend.
    ISR = 2,               ///< Platform-neutral interrupt-driven edge capture backend when available.
    FLEXPWM = 3            ///< Teensy 4.x-only FlexPWM capture backend.
};

inline const char* toString(RxBackend backend) FL_NOEXCEPT {
    switch (backend) {
    case RxBackend::PLATFORM_DEFAULT: return "PLATFORM_DEFAULT";
    case RxBackend::RMT: return "RMT";
    case RxBackend::ISR: return "ISR";
    case RxBackend::FLEXPWM: return "FLEXPWM";
    }
    return "UNKNOWN";
}

}  // namespace fl
