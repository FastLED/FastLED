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
/// Uses new fl/pin.h API with enum classes (PinMode, PinValue, AdcRange).

#ifndef ARDUINO
#include "pin_samd_native.hpp"
#endif

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>

namespace fl {
namespace platform {

inline void pinMode(int pin, PinMode mode) {
    // Translate PinMode enum to Arduino constants
    // PinMode: Input=0, Output=1, InputPullup=2, InputPulldown=3
    // Arduino: INPUT=0x0, OUTPUT=0x1, INPUT_PULLUP=0x2, INPUT_PULLDOWN=0x3
    ::pinMode(pin, static_cast<int>(mode));
}

inline void digitalWrite(int pin, PinValue val) {
    // Translate PinValue enum to Arduino constants
    // PinValue: Low=0, High=1
    // Arduino: LOW=0, HIGH=1
    ::digitalWrite(pin, static_cast<int>(val));
}

inline PinValue digitalRead(int pin) {
    // Arduino digitalRead returns int (0 or 1)
    // Convert to PinValue enum
    return ::digitalRead(pin) ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    // Arduino analogRead returns int, we return uint16_t
    return static_cast<uint16_t>(::analogRead(pin));
}

inline void analogWrite(int pin, uint16_t val) {
    // Arduino analogWrite expects int, we accept uint16_t
    ::analogWrite(pin, static_cast<int>(val));
}

inline void setPwm16(int pin, uint16_t val) {
    // SAMD (Arduino Zero, M4, etc.) supports 16-bit PWM via TCC/TC modules
    // Arduino core's analogWrite handles resolution internally via mapResolution
    // Just pass the value through - Arduino core will scale it appropriately
    ::analogWrite(pin, static_cast<int>(val));
}

inline void setAdcRange(AdcRange range) {
    // Translate AdcRange enum to Arduino analogReference constants
    switch (range) {
        case AdcRange::Default:
            ::analogReference(AR_DEFAULT);
            break;
        case AdcRange::Range0_1V1:
            #ifdef AR_INTERNAL1V0
            ::analogReference(AR_INTERNAL1V0);
            #elif defined(AR_INTERNAL)
            ::analogReference(AR_INTERNAL);
            #endif
            break;
        case AdcRange::External:
            #ifdef AR_EXTERNAL
            ::analogReference(AR_EXTERNAL);
            #endif
            break;
        default:
            // Other ranges not supported on SAMD, no-op
            break;
    }
}

}  // namespace platform
}  // namespace fl

#endif  // ARDUINO
