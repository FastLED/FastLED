#pragma once

/// @file platforms/arm/silabs/pin_silabs.hpp
/// Silicon Labs (MGM240/EFR32) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for Silicon Labs pin functions.
///
/// Arduino path: Wraps Arduino pin functions (pinMode, digitalWrite, etc.)
///
/// IMPORTANT: Uses fl::PinMode, fl::PinValue, fl::AdcRange enum classes.
/// The Silicon Labs Arduino API uses strict enum types (PinMode, PinStatus),
/// so we cast between fl:: and Arduino enum types.

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>
#include "fl/pin.h"

namespace fl {
namespace platform {

inline void pinMode(int pin, PinMode mode) {
    // Map fl::PinMode to Arduino PinMode
    // Arduino PinMode: INPUT=0, OUTPUT=1, INPUT_PULLUP=2
    // fl::PinMode: Input=0, Output=1, InputPullup=2, InputPulldown=3
    ::pinMode(pin, static_cast<::PinMode>(static_cast<int>(mode)));
}

inline void digitalWrite(int pin, PinValue val) {
    // Map fl::PinValue to Arduino PinStatus
    // Arduino PinStatus: LOW=0, HIGH=1
    // fl::PinValue: Low=0, High=1
    ::digitalWrite(pin, static_cast<::PinStatus>(static_cast<int>(val)));
}

inline PinValue digitalRead(int pin) {
    int result = ::digitalRead(pin);
    return result ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    return static_cast<uint16_t>(::analogRead(pin));
}

inline void analogWrite(int pin, uint16_t val) {
    ::analogWrite(pin, val);
}

inline void setPwm16(int pin, uint16_t val) {
    // Silicon Labs Arduino core PWM capabilities vary
    // Check if analogWriteResolution is available, otherwise scale to 8-bit
#if defined(analogWriteResolution)
    static bool resolution_set = false; // okay static in header
    if (!resolution_set) {
        analogWriteResolution(16);
        resolution_set = true;
    }
    ::analogWrite(pin, val);
#else
    // Scale 16-bit value down to 8-bit if no resolution control
    ::analogWrite(pin, static_cast<uint8_t>(val >> 8));
#endif
}

inline void setAdcRange(AdcRange range) {
    // Silicon Labs Arduino API uses analogReference(uint8_t mode)
    // Map fl::AdcRange to Arduino reference constants
    // Note: Silicon Labs may not support all ranges - platform-specific behavior
    ::analogReference(static_cast<uint8_t>(range));
}

}  // namespace platform
}  // namespace fl

#endif  // ARDUINO
