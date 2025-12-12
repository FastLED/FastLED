#pragma once

/// @file platforms/arm/rp/pin_rp.hpp
/// RP2040/RP2350 (Raspberry Pi Pico) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for RP2040/RP2350 pin functions.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Wraps Arduino pin functions
/// 2. Pico SDK path (#else): Uses pin_rp_native.hpp with native Pico SDK GPIO functions
///
/// IMPORTANT: All functions return/accept int types only (no enums).

#ifndef ARDUINO
#include "pin_rp_native.hpp"
#endif

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
