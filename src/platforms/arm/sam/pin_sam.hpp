#pragma once

/// @file platforms/arm/sam/pin_sam.hpp
/// SAM (Arduino Due, SAM3X8E) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for SAM pin functions.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Wraps Arduino pin functions
/// 2. Native SAM path (#else): Uses pin_sam_native.hpp for direct PORT register access
///
/// IMPORTANT: All functions use enum class types (PinMode, PinValue, AdcRange).

#ifndef ARDUINO
#include "pin_sam_native.hpp"
#endif

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>

namespace fl {
namespace platform {

inline void pinMode(int pin, PinMode mode) {
    // Translate PinMode to Arduino constants
    // Input = 0, Output = 1, InputPullup = 2, InputPulldown = 3
    ::pinMode(pin, static_cast<int>(mode));
}

inline void digitalWrite(int pin, PinValue val) {
    // Translate PinValue to Arduino constants (Low = 0, High = 1)
    ::digitalWrite(pin, static_cast<int>(val));
}

inline PinValue digitalRead(int pin) {
    return ::digitalRead(pin) ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    return static_cast<uint16_t>(::analogRead(pin));
}

inline void analogWrite(int pin, uint16_t val) {
    ::analogWrite(pin, static_cast<int>(val));
}

inline void setPwm16(int pin, uint16_t val) {
    // SAM (Arduino Due) supports 16-bit PWM natively via PWM controller
    // Use analogWriteResolution to set 16-bit mode, then write value
    static bool resolution_set = false;
    if (!resolution_set) {
        analogWriteResolution(16);
        resolution_set = true;
    }
    ::analogWrite(pin, static_cast<int>(val));
}

inline void setAdcRange(AdcRange range) {
    // Arduino Due doesn't support analogReference - analog reference is fixed at 3.3V
    // The function exists in the SAM core but does nothing
    // No-op for all range values
    (void)range;
}

}  // namespace platform
}  // namespace fl

#endif  // ARDUINO
