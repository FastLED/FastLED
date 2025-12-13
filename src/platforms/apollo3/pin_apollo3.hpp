#pragma once

/// @file platforms/apollo3/pin_apollo3.hpp
/// Apollo3 (SparkFun RedBoard Artemis, etc.) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for Apollo3 pin functions.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Wraps Arduino pin functions
/// 2. Native Apollo3 path (#else): Uses pin_apollo3_native.hpp
///
/// IMPORTANT: All functions return/accept int types only (no enums).

#ifndef ARDUINO
#include "pin_apollo3_native.hpp"
#endif

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>

namespace fl {

// ============================================================================
// Detail namespace: Type-safe wrappers that accept Arduino enum types
// ============================================================================

namespace detail {

inline void pinMode(int pin, Arduino_PinMode mode) {
    ::pinMode(pin, mode);
}

inline void digitalWrite(int pin, PinStatus val) {
    ::digitalWrite(pin, val);
}

}  // namespace detail

// ============================================================================
// Public API: int-based interface that casts to enum types
// ============================================================================

inline void pinMode(int pin, int mode) {
    detail::pinMode(pin, static_cast<Arduino_PinMode>(mode));
}

inline void digitalWrite(int pin, int val) {
    detail::digitalWrite(pin, static_cast<PinStatus>(val));
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
