#pragma once

/// @file platforms/arm/renesas/pin_renesas.hpp
/// Renesas (Arduino UNO R4, etc.) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for Renesas pin functions.
///
/// Arduino path: Wraps Arduino pin functions (Renesas uses standard Arduino API)
///
/// IMPORTANT: All functions return/accept int types only (no enums).

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>

namespace fl {

inline void pinMode(int pin, int mode) {
    ::pinMode(pin, static_cast<PinMode>(mode));
}

inline void digitalWrite(int pin, int val) {
    ::digitalWrite(pin, static_cast<PinStatus>(val));
}

inline int digitalRead(int pin) {
    return ::digitalRead(pin);
}

inline int analogRead(int pin) {
    return ::analogRead(pin);
}

inline void analogWrite(int pin, int val) {
    ::analogWrite(pin, val);
}

inline void analogReference(int mode) {
    ::analogReference(mode);
}

}  // namespace fl

#endif  // ARDUINO
