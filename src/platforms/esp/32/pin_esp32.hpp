#pragma once

/// @file platforms/esp/32/pin_esp32.hpp
/// ESP32 pin implementation (header-only)
///
/// Provides zero-overhead wrappers for ESP32 pin functions.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Wraps Arduino pin functions
/// 2. ESP-IDF path (#else): Uses pin_esp32_native.hpp (native ESP-IDF GPIO drivers)
///
/// IMPORTANT: All functions return/accept int types only (no enums).

#ifndef ARDUINO
#include "pin_esp32_native.hpp"
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

inline void analogReference(int /*mode*/) {
    // ESP32 doesn't support analogReference in Arduino
    // ADC reference is fixed (different per ESP32 variant)
}

}  // namespace fl

#endif  // ARDUINO
