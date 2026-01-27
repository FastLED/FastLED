#pragma once

// Only compile for WASM platform
#ifdef __EMSCRIPTEN__

#include "platforms/time_platform.h"
#include "fl/stl/stdint.h"
#include <emscripten.h>

// Forward declare WASM timer functions (defined in timer.cpp.hpp)
extern "C" {
    uint32_t millis();
    uint32_t micros();
}

namespace fl {
namespace platform {

void delay(fl::u32 ms) {
    if (ms == 0) return;
    uint32_t end = ::millis() + ms;
    while (::millis() < end) {
        // Busy-wait (no async pumping at platform layer)
    }
}

void delayMicroseconds(fl::u32 us) {
    if (us == 0) return;
    double start = emscripten_get_now();
    double target = start + (us / 1000.0);  // Convert to milliseconds
    while (emscripten_get_now() < target) {
        // Busy-wait for microsecond precision
    }
}

fl::u32 millis() {
    return ::millis();
}

fl::u32 micros() {
    return ::micros();
}

}  // namespace platform
}  // namespace fl

#endif  // __EMSCRIPTEN__
