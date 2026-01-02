#pragma once

/// @file platforms/arm/teensy/pin_teensy.hpp
/// Teensy (3.x, 4.x, etc.) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for Teensy pin functions.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Wraps Teensy core pin functions
/// 2. Native Teensy path (#else): Uses pin_teensy_native.hpp (Teensy core functions)
///
/// IMPORTANT: All functions use enum class types for type safety.

#ifndef ARDUINO
#include "pin_teensy_native.hpp"
#endif

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Teensy core pin functions
// ============================================================================

// Use Teensy's native core_pins.h instead of Arduino.h
// core_pins.h provides the same pin functions (pinMode, digitalWrite, etc.)
// and constants (INPUT, OUTPUT, etc.) but works in both Arduino and non-Arduino builds
#include <core_pins.h>

namespace fl {
namespace platform {

inline void pinMode(int pin, fl::PinMode mode) {
    // Translate PinMode to Teensy core constants
    // PinMode::Input=0, Output=1, InputPullup=2, InputPulldown=3
    // Teensy: INPUT=0, OUTPUT=1, INPUT_PULLUP=2, INPUT_PULLDOWN (Teensy-specific)
    int teensy_mode;
    switch (mode) {
        case fl::PinMode::Input:
            teensy_mode = INPUT;  // 0
            break;
        case fl::PinMode::Output:
            teensy_mode = OUTPUT;  // 1
            break;
        case fl::PinMode::InputPullup:
            teensy_mode = INPUT_PULLUP;  // 2
            break;
        case fl::PinMode::InputPulldown:
#ifdef INPUT_PULLDOWN
            teensy_mode = INPUT_PULLDOWN;  // Teensy-specific
#else
            teensy_mode = INPUT_PULLUP;  // Fallback to INPUT_PULLUP
#endif
            break;
    }
    ::pinMode(pin, teensy_mode);
}

inline void digitalWrite(int pin, fl::PinValue val) {
    ::digitalWrite(pin, static_cast<int>(val));  // PinValue::Low=0, High=1
}

inline fl::PinValue digitalRead(int pin) {
    return ::digitalRead(pin) ? fl::PinValue::High : fl::PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    return static_cast<uint16_t>(::analogRead(pin));
}

inline void analogWrite(int pin, uint16_t val) {
    ::analogWrite(pin, val);
}

inline void setPwm16(int pin, uint16_t val) {
    // Teensy has excellent 16-bit PWM support via analogWriteResolution
    // Set 16-bit resolution for full dynamic range (0-65535)
    // Note: This may affect PWM frequency depending on timer configuration
    analogWriteResolution(16);
    ::analogWrite(pin, val);
}

inline void setAdcRange(fl::AdcRange range) {
#if defined(__IMXRT1062__) || defined(__IMXRT1052__)
    // Teensy 4.x: ADC range is fixed at 3.3V, analogReference() not supported
    // No-op: These processors have a fixed 3.3V reference
    (void)range;  // Suppress unused parameter warning
#else
    // Teensy 3.x: Translate AdcRange to Teensy core analogReference() constants
    // Supports: DEFAULT (3.3V), INTERNAL (1.2V), EXTERNAL (AREF pin)
    int ref_mode;
    switch (range) {
        case fl::AdcRange::Default:
            ref_mode = DEFAULT;
            break;
        case fl::AdcRange::Range0_1V1:
            ref_mode = INTERNAL;  // 1.2V internal reference on Teensy 3.x
            break;
        case fl::AdcRange::External:
            ref_mode = EXTERNAL;  // External AREF pin
            break;
        default:
            // Unsupported ranges - use default
            ref_mode = DEFAULT;
            break;
    }
    ::analogReference(ref_mode);
#endif
}

}  // namespace platform
}  // namespace fl

#endif  // ARDUINO
