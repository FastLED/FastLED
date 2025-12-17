#pragma once

/// @file platforms/arm/teensy/pin_teensy.hpp
/// Teensy (3.x, 4.x, etc.) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for Teensy pin functions.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Wraps Arduino pin functions
/// 2. Native Teensy path (#else): Uses pin_teensy_native.hpp (Teensy core functions)
///
/// IMPORTANT: All functions use enum class types for type safety.

#ifndef ARDUINO
#include "pin_teensy_native.hpp"
#endif

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>

namespace fl {

inline void pinMode(int pin, PinMode mode) {
    // Translate PinMode to Arduino constants
    // PinMode::Input=0, Output=1, InputPullup=2, InputPulldown=3
    // Arduino: INPUT=0, OUTPUT=1, INPUT_PULLUP=2
    // Teensy also supports INPUT_PULLDOWN (not standard Arduino)
    int arduino_mode;
    switch (mode) {
        case PinMode::Input:
            arduino_mode = INPUT;  // 0
            break;
        case PinMode::Output:
            arduino_mode = OUTPUT;  // 1
            break;
        case PinMode::InputPullup:
            arduino_mode = INPUT_PULLUP;  // 2
            break;
        case PinMode::InputPulldown:
#ifdef INPUT_PULLDOWN
            arduino_mode = INPUT_PULLDOWN;  // Teensy-specific
#else
            arduino_mode = INPUT_PULLUP;  // Fallback to INPUT_PULLUP
#endif
            break;
    }
    ::pinMode(pin, arduino_mode);
}

inline void digitalWrite(int pin, PinValue val) {
    ::digitalWrite(pin, static_cast<int>(val));  // PinValue::Low=0, High=1
}

inline PinValue digitalRead(int pin) {
    return ::digitalRead(pin) ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    return static_cast<uint16_t>(::analogRead(pin));
}

inline void analogWrite(int pin, uint16_t val) {
    ::analogWrite(pin, val);
}

inline void setAdcRange(AdcRange range) {
    // Translate AdcRange to Arduino analogReference() constants
    // Teensy supports: DEFAULT, INTERNAL, EXTERNAL
    int ref_mode;
    switch (range) {
        case AdcRange::Default:
            ref_mode = ::DEFAULT;
            break;
        case AdcRange::Range0_1V1:
            ref_mode = ::INTERNAL;  // 1.2V internal reference on Teensy
            break;
        case AdcRange::External:
            ref_mode = ::EXTERNAL;  // External AREF pin
            break;
        default:
            // Unsupported ranges - use default
            ref_mode = ::DEFAULT;
            break;
    }
    ::analogReference(ref_mode);
}

}  // namespace fl

#endif  // ARDUINO
