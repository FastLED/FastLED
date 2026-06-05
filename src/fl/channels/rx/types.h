#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

enum class RxBackend : u8 {
    PLATFORM_DEFAULT = 0,  ///< Use the recommended backend for the active platform (RMT on ESP32; FlexPWM on Teensy 4.x with FLEXIO as an opt-in alternative — see FastLED#2764; native RX on stub/host builds).
    RMT = 1,               ///< ESP32-only RMT capture backend.
    ISR = 2,               ///< Platform-neutral interrupt-driven edge capture backend when available.
    FLEXPWM = 3,           ///< Teensy 4.x-only FlexPWM capture backend.
    FLEXIO = 4             ///< Teensy 4.x-only FlexIO capture backend (FLEXIO1 — FLEXIO2 is owned by the WS2812 TX driver). Hardware-verified by `flexioRxBenchmark` (1/10/100 kHz square wave) and `flexioObjectFledTest` (5-pattern ObjectFLED loopback). See FastLED#2764.
};

inline const char* toString(RxBackend backend) FL_NOEXCEPT {
    switch (backend) {
    case RxBackend::PLATFORM_DEFAULT: return "PLATFORM_DEFAULT";
    case RxBackend::RMT: return "RMT";
    case RxBackend::ISR: return "ISR";
    case RxBackend::FLEXPWM: return "FLEXPWM";
    case RxBackend::FLEXIO: return "FLEXIO";
    }
    return "UNKNOWN";
}

}  // namespace fl
