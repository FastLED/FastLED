#pragma once

/// @file platforms/shared/pin_arduino.hpp
/// Generic Arduino pin implementation (fallback for any Arduino-compatible platform)
///
/// This is a simple pass-through wrapper for platforms that support the standard
/// Arduino pin API (::pinMode, ::digitalWrite, etc.) but don't have a specific
/// optimized implementation.
///
/// IMPORTANT: This file should ONLY be included on Arduino builds.

#ifndef ARDUINO
    #error "pin_arduino.hpp requires ARDUINO to be defined"
#endif

#include <Arduino.h>

namespace fl {

// ============================================================================
// Pin mode control
// ============================================================================

inline void pinMode(int pin, int mode) {
#if defined(ARDUINO_ARCH_SILABS)
    // Silabs Arduino framework requires PinMode enum instead of int
    ::pinMode(pin, static_cast<PinMode>(mode));
#else
    ::pinMode(pin, mode);
#endif
}

// ============================================================================
// Digital I/O
// ============================================================================

inline void digitalWrite(int pin, int val) {
#if defined(ARDUINO_ARCH_SILABS)
    // Silabs Arduino framework requires PinStatus enum instead of int
    ::digitalWrite(pin, static_cast<PinStatus>(val));
#else
    ::digitalWrite(pin, val);
#endif
}

inline int digitalRead(int pin) {
    return ::digitalRead(pin);
}

// ============================================================================
// Analog I/O
// ============================================================================

inline int analogRead(int pin) {
    return ::analogRead(pin);
}

inline void analogWrite(int pin, int val) {
    ::analogWrite(pin, val);
}

inline void analogReference(int mode) {
#if defined(ARDUINO_SAM_DUE) || defined(__SAM3X8E__) || defined(NRF52_SERIES)
    // Arduino SAM (Due) and nRF52 require eAnalogReference enum instead of int
    ::analogReference(static_cast<eAnalogReference>(mode));
#else
    ::analogReference(mode);
#endif
}

}  // namespace fl
