#pragma once

// IWYU pragma: private

// Only compile for WASM platform
#include "is_wasm.h"

#ifdef FL_IS_WASM

#include "platforms/time_platform.h"
#include "fl/stl/stdint.h"
#include <emscripten.h>

// Forward declare WASM timer functions (defined in timer.cpp.hpp)
extern "C" {
    fl::u32 millis();
    fl::u32 micros();
}

namespace fl {
namespace platforms {

void delay(fl::u32 ms) {
    if (ms == 0) return;
    u32 end = ::millis() + ms;
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

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_WASM
