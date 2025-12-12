#pragma once

/// @file platforms/arm/samd/pin_samd.hpp
/// SAMD (Arduino Zero, M0, M4, etc.) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for SAMD pin functions.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Wraps Arduino pin functions
/// 2. Native SAMD path (#else): Uses pin_samd_native.hpp for direct PORT register access
///
/// IMPORTANT: All functions return/accept int types only (no enums).

#ifndef ARDUINO
#include "pin_samd_native.hpp"
#endif

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
    ::analogReference(mode);
}

}  // namespace fl

#endif  // ARDUINO
