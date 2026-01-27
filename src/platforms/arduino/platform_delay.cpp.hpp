#pragma once

// Only compile for real Arduino platforms (not stub, not WASM)
#if defined(ARDUINO) && !defined(FASTLED_STUB_IMPL) && !defined(__EMSCRIPTEN__)

#include "platforms/delay_platform.h"
#include "Arduino.h"  // okay banned header

namespace fl {
namespace platform {

void delay(fl::u32 ms) {
    // Call Arduino core's delay() function
    ::delay((unsigned long)ms);
}

}  // namespace platform
}  // namespace fl

#endif  // ARDUINO && !FASTLED_STUB_IMPL && !__EMSCRIPTEN__
