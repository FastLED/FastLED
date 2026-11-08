#pragma once

// IWYU pragma: private

/// @file platforms/arm/nrf51/pin_nrf51.hpp
/// Nordic nRF51 (BBC micro:bit v1, etc.) pin implementation (header-only)
///
/// Adapters from FastLED's type-safe pin API to the arduino-nRF5 core's
/// Arduino API. Pin numbers are Arduino pin numbers: the core maps them to
/// nRF51 GPIOs through the variant's g_ADigitalPinMap (on the micro:bit the
/// two numberings differ), which matches the Arduino-delegating backends of
/// Teensy, STM32, AVR and ClearCore. The core also owns the ADC and the
/// TIMER-based PWM, which the nRF51 has no dedicated peripheral for.

#include "fl/stl/noexcept.h"
#include "fl/system/arduino.h"

namespace fl {
namespace platforms {

inline void pinMode(int pin, PinMode mode) FL_NO_EXCEPT {
    u32 arduino_mode = INPUT;
    switch (mode) {
        case PinMode::Input:
            arduino_mode = INPUT;
            break;
        case PinMode::Output:
            arduino_mode = OUTPUT;
            break;
        case PinMode::InputPullup:
            arduino_mode = INPUT_PULLUP;
            break;
        case PinMode::InputPulldown:
            arduino_mode = INPUT_PULLDOWN;
            break;
    }
    ::pinMode(static_cast<u32>(pin), arduino_mode);
}

inline void digitalWrite(int pin, PinValue val) FL_NO_EXCEPT {
    ::digitalWrite(static_cast<u32>(pin), val == PinValue::High ? HIGH : LOW);
}

inline PinValue digitalRead(int pin) FL_NO_EXCEPT {
    return ::digitalRead(static_cast<u32>(pin)) ? PinValue::High : PinValue::Low;
}

inline u16 analogRead(int pin) FL_NO_EXCEPT {
    return static_cast<u16>(::analogRead(static_cast<u32>(pin)));
}

inline void analogWrite(int pin, u16 val) FL_NO_EXCEPT {
    ::analogWrite(static_cast<u32>(pin), val);
}

inline void setPwm16(int pin, u16 val) FL_NO_EXCEPT {
    // The core's software PWM is 8-bit; scale down rather than changing the
    // global analogWriteResolution() other callers rely on.
    ::analogWrite(static_cast<u32>(pin), static_cast<u32>(val >> 8));
}

inline void setAdcRange(AdcRange /*range*/) FL_NO_EXCEPT {
    // Reference selection is left to the core's analogReference().
}

// ============================================================================
// PWM Frequency Control
// ============================================================================

inline bool needsPwmIsrFallback(int /*pin*/, u32 /*frequency_hz*/) {
    return true;
}

inline int setPwmFrequencyNative(int /*pin*/, u32 /*frequency_hz*/) {
    return -4;
}

inline u32 getPwmFrequencyNative(int /*pin*/) {
    return 0;
}

}  // namespace platforms
}  // namespace fl
