#pragma once

/// @file platforms/arm/nrf52/pin_nrf52.hpp
/// Nordic nRF52 (Adafruit Feather nRF52, etc.) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for nRF52 pin functions.
///
/// Arduino path: Wraps Arduino pin functions (nRF52 uses standard Arduino API)
///
/// IMPORTANT: All functions return/accept int types only (no enums).

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>

namespace fl {

inline void pinMode(int pin, int mode) {
    ::pinMode(pin, mode);
}

inline void digitalWrite(int pin, int val) {
    ::digitalWrite(pin, val);
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
    ::analogReference(static_cast<eAnalogReference>(mode));
}

}  // namespace fl

#endif  // ARDUINO
