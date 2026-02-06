#pragma once

// Only compile for real Arduino platforms (not stub, not WASM)
#if defined(ARDUINO) && !defined(FASTLED_STUB_IMPL) && !defined(__EMSCRIPTEN__)

#include "platforms/time_platform.h"
#include "Arduino.h"  // okay banned header

namespace fl {
namespace platforms {

void delay(fl::u32 ms) {
    ::delay((unsigned long)ms);
}

void delayMicroseconds(fl::u32 us) {
    ::delayMicroseconds((unsigned int)us);
}

fl::u32 millis() {
    return static_cast<fl::u32>(::millis());
}

fl::u32 micros() {
    return static_cast<fl::u32>(::micros());
}

}  // namespace platforms
}  // namespace fl

#endif  // ARDUINO && !FASTLED_STUB_IMPL && !__EMSCRIPTEN__
