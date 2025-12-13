#pragma once

/// @file platforms/arm/silabs/pin_silabs.hpp
/// Silicon Labs (MGM240/EFR32) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for Silicon Labs pin functions.
///
/// Arduino path: Wraps Arduino pin functions (pinMode, digitalWrite, etc.)
///
/// IMPORTANT: All functions return/accept int types only (no enums).
/// The Silicon Labs Arduino API uses strict enum types (PinMode, PinStatus),
/// so we cast from int to the appropriate enum types.

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
