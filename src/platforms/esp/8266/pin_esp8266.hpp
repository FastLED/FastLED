#pragma once

/// @file platforms/esp/8266/pin_esp8266.hpp
/// ESP8266 pin implementation (header-only)
///
/// Provides zero-overhead wrappers for ESP8266 pin functions.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Wraps Arduino pin functions
/// 2. ESP8266 SDK path (#else): Uses pin_esp8266_native.hpp
///
/// IMPORTANT: All functions return/accept int types only (no enums).

#ifndef ARDUINO
#include "pin_esp8266_native.hpp"
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
