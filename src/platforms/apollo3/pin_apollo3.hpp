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
/// IMPORTANT: All functions use fl::PinMode, fl::PinValue, fl::AdcRange enum classes.

#ifndef ARDUINO
#include "pin_apollo3_native.hpp"
#endif

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>
#include "fl/pin.h"

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
// Public API: fl::PinMode/PinValue/AdcRange interface
// ============================================================================

namespace platform {

inline void pinMode(int pin, PinMode mode) {
    // Translate fl::PinMode to Arduino_PinMode
    // PinMode: Input=0, Output=1, InputPullup=2, InputPulldown=3
    // Arduino_PinMode: INPUT=0, OUTPUT=1, INPUT_PULLUP=2, INPUT_PULLDOWN=3
    detail::pinMode(pin, static_cast<Arduino_PinMode>(static_cast<int>(mode)));
}

inline void digitalWrite(int pin, PinValue val) {
    // Translate fl::PinValue to PinStatus
    // PinValue: Low=0, High=1
    // PinStatus: LOW=0, HIGH=1
    detail::digitalWrite(pin, static_cast<PinStatus>(static_cast<int>(val)));
}

inline PinValue digitalRead(int pin) {
    int result = ::digitalRead(pin);
    return result ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    return static_cast<uint16_t>(::analogRead(pin));
}

inline void analogWrite(int pin, uint16_t val) {
    ::analogWrite(pin, val);
}

inline void setPwm16(int pin, uint16_t val) {
    // Apollo3 CTIMER supports 32-bit counters, capable of 16-bit PWM
    // Arduino core's analogWrite uses 8-bit resolution by default
    // Scale 16-bit value down to 8-bit for Arduino compatibility
    // Full 16-bit would require direct CTIMER configuration (see pin_apollo3_native.hpp)
    ::analogWrite(pin, static_cast<uint8_t>(val >> 8));
}

inline void setAdcRange(AdcRange range) {
    // Translate fl::AdcRange to Arduino analogReference modes
    // AdcRange: Default=0, Range0_1V1=1, Range0_1V5=2, Range0_2V2=3, Range0_3V3=4, Range0_5V=5, External=6
    // Apollo3 supports: DEFAULT, INTERNAL (1.5V), INTERNAL1V5, INTERNAL2V0, EXTERNAL
    int mode = 0;  // DEFAULT

    switch (range) {
        case AdcRange::Default:
            mode = 0;  // DEFAULT (1.5V internal)
            break;
        case AdcRange::Range0_1V1:
            mode = 1;  // INTERNAL (1.5V - closest available)
            break;
        case AdcRange::Range0_1V5:
            mode = 3;  // INTERNAL1V5
            break;
        case AdcRange::Range0_2V2:
            mode = 4;  // INTERNAL2V0 (closest to 2.2V)
            break;
        case AdcRange::Range0_3V3:
        case AdcRange::Range0_5V:
            mode = 4;  // INTERNAL2V0 (max internal reference)
            break;
        case AdcRange::External:
            mode = 2;  // EXTERNAL
            break;
    }

    ::analogReference(mode);
}

}  // namespace platform
}  // namespace fl

#endif  // ARDUINO
