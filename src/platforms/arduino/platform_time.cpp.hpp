#pragma once

// IWYU pragma: private

// Only compile for non-ESP32 Arduino platforms (not stub, not WASM)
// ESP32 has its own platform_time in platforms/esp/32/platform_time_esp32.cpp.hpp
#include "platforms/wasm/is_wasm.h"
#include "platforms/esp/is_esp.h"

#if defined(ARDUINO) && !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM) && !defined(FL_IS_ESP32)

#include "platforms/time_platform.h"
#include "fl/arduino.h"

namespace fl {
namespace platforms {

void delay(fl::u32 ms) {
    ::delay((unsigned long)ms);
}

void delayMicroseconds(fl::u32 us) {
    ::delayMicroseconds((u32)us);
}

fl::u32 millis() {
    return static_cast<fl::u32>(::millis());
}

fl::u32 micros() {
    return static_cast<fl::u32>(::micros());
}

}  // namespace platforms
}  // namespace fl

#endif  // ARDUINO && !FASTLED_STUB_IMPL && !FL_IS_WASM && !FL_IS_ESP32
