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
// Template trampolines: automatically infer parameter types from function pointers
// ============================================================================

template<typename T>
inline void pinModeTrampoline(int pin, int mode, void(*func)(int, T)) {
    func(pin, static_cast<T>(mode));
}

template<typename T>
inline void digitalWriteTrampoline(int pin, int val, void(*func)(int, T)) {
    func(pin, static_cast<T>(val));
}

template<typename T>
inline void analogReferenceTrampoline(int mode, void(*func)(T)) {
    func(static_cast<T>(mode));
}

// ============================================================================
// Pin mode control
// ============================================================================

inline void pinMode(int pin, int mode) {
    pinModeTrampoline(pin, mode, ::pinMode);
}

// ============================================================================
// Digital I/O
// ============================================================================

inline void digitalWrite(int pin, int val) {
    digitalWriteTrampoline(pin, val, ::digitalWrite);
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
    analogReferenceTrampoline(mode, ::analogReference);
}

}  // namespace fl
