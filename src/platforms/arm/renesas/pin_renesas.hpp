#pragma once

/// @file platforms/arm/renesas/pin_renesas.hpp
/// Renesas (Arduino UNO R4, etc.) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for Renesas pin functions.
///
/// Arduino path: Wraps Arduino pin functions (Renesas uses standard Arduino API)

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>

namespace fl {

inline void pinMode(int pin, PinMode mode) {
    // Translate fl::PinMode to Arduino constants
    // Input=0, Output=1, InputPullup=2, InputPulldown=3
    ::pinMode(pin, static_cast<::PinMode>(static_cast<int>(mode)));
}

inline void digitalWrite(int pin, PinValue val) {
    // Translate fl::PinValue to Arduino constants
    // Low=0, High=1
    ::digitalWrite(pin, static_cast<::PinStatus>(static_cast<int>(val)));
}

inline PinValue digitalRead(int pin) {
    int result = ::digitalRead(pin);
    return (result == HIGH) ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    return static_cast<uint16_t>(::analogRead(pin));
}

inline void analogWrite(int pin, uint16_t val) {
    ::analogWrite(pin, static_cast<int>(val));
}

inline void setAdcRange(AdcRange range) {
    // Translate fl::AdcRange to Arduino analogReference constants
    // Note: Renesas uses AR_DEFAULT, AR_INTERNAL, AR_EXTERNAL
    switch (range) {
        case AdcRange::Default:
            ::analogReference(AR_DEFAULT);
            break;
        case AdcRange::Range0_1V1:
            ::analogReference(AR_INTERNAL);
            break;
        case AdcRange::External:
            ::analogReference(AR_EXTERNAL);
            break;
        default:
            // Unsupported range - no-op
            break;
    }
}

}  // namespace fl

#endif  // ARDUINO
