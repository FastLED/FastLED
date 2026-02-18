#pragma once

// IWYU pragma: private

// Only compile for real Arduino platforms (not stub, not WASM)
#include "platforms/wasm/is_wasm.h"

#if defined(ARDUINO) && !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM)

#include "platforms/time_platform.h"
#include "fl/arduino.h"

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

#endif  // ARDUINO && !FASTLED_STUB_IMPL && !FL_IS_WASM
