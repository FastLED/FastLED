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

// Template trampoline: automatically infers second parameter type from function pointer
template<typename T>
inline void pinModeTrampoline(int pin, int mode, void(*func)(int, T)) {
    func(pin, static_cast<T>(mode));
}

template<typename T>
inline void digitalWriteTrampoline(int pin, int val, void(*func)(int, T)) {
    func(pin, static_cast<T>(val));
}

inline void pinMode(int pin, int mode) {
    pinModeTrampoline(pin, mode, ::pinMode);
}

inline void digitalWrite(int pin, int val) {
    digitalWriteTrampoline(pin, val, ::digitalWrite);
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
