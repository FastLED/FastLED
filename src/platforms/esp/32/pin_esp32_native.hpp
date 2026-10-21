#pragma once

// IWYU pragma: private

/// @file platforms/esp/32/pin_esp32_native.hpp
/// ESP32 ESP-IDF native GPIO driver - trampoline header
///
/// This file tests the environment and either includes the real ESP-IDF
/// implementation or provides stub functions with a warning.
///
/// Define FL_ESP32_IO_DISABLE_WARN to silence the warning when using stubs.

#include "fl/stl/has_include.h"

// Test if we have ESP-IDF GPIO driver available
#if FL_HAS_INCLUDE("driver/gpio.h") && FL_HAS_INCLUDE("driver/ledc.h")
    // Real ESP-IDF environment detected - include full implementation
    #include "platforms/esp/32/pin_esp32_native_impl.hpp"
#else
    // ESP-IDF headers not available - provide stub implementation
    #ifndef FL_ESP32_IO_DISABLE_WARN
        #warning "ESP32 native GPIO/LEDC drivers not available. Pin I/O functions will be no-ops. Define FL_ESP32_IO_DISABLE_WARN to silence this warning."
    #endif

    #include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

    namespace fl {

    // Forward declarations of types from fl/pin.h
    enum class PinMode;
    enum class PinValue;
    enum class AdcRange;

    namespace platforms {

    // Stub implementations - all operations are no-ops
    inline void pinMode(int, PinMode) FL_NO_EXCEPT {}
    inline void digitalWrite(int, PinValue) FL_NO_EXCEPT {}
    inline PinValue digitalRead(int) FL_NO_EXCEPT { return static_cast<PinValue>(0); }
    inline u16 analogRead(int) FL_NO_EXCEPT { return 0; }
    inline void analogWrite(int, u16) FL_NO_EXCEPT {}
    inline void setPwm16(int, u16) FL_NO_EXCEPT {}
    inline void setAdcRange(AdcRange) FL_NO_EXCEPT {}
    inline bool needsPwmIsrFallback(int, u32) FL_NO_EXCEPT { return true; }
    inline int setPwmFrequencyNative(int, u32) FL_NO_EXCEPT { return -1; }
    inline u32 getPwmFrequencyNative(int) FL_NO_EXCEPT { return 0; }

    }  // namespace platforms
    }  // namespace fl

#endif  // FL_HAS_INCLUDE("driver/gpio.h")
