#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

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
namespace platforms {

inline void pinMode(int pin, fl::PinMode mode) {
    // Translate PinMode to Teensy core constants
    // PinMode::Input=0, Output=1, InputPullup=2, InputPulldown=3
    // Teensy: INPUT=0, OUTPUT=1, INPUT_PULLUP=2, INPUT_PULLDOWN (Teensy-specific)
    int teensy_mode = INPUT;  // Initialize to safe default
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

inline u16 analogRead(int pin) {
    return static_cast<u16>(::analogRead(pin));
}

inline void analogWrite(int pin, u16 val) {
    ::analogWrite(pin, val);
}

inline void setPwm16(int pin, u16 val) {
    // Teensy has excellent 16-bit PWM support via analogWriteResolution
    // Set 16-bit resolution for full dynamic range (0-65535)
    // Note: This may affect PWM frequency depending on timer configuration
    analogWriteResolution(16);
    ::analogWrite(pin, val);
}

inline void setAdcRange(fl::AdcRange range) {
#if defined(FL_IS_TEENSY_4X)
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

// ============================================================================
// PWM Frequency Control
// ============================================================================

namespace {
    struct TeensyPwmFreq {
        int pin;      // -1 = unused
        u32 freq_hz;
    };
    constexpr int MAX_TEENSY_PWM_PINS = 40;
    inline TeensyPwmFreq* getTeensyPwmTable() {
        static TeensyPwmFreq table[MAX_TEENSY_PWM_PINS] = {
            {-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},
            {-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},
            {-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},
            {-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},
            {-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0}
        };
        return table;
    }
}  // anonymous namespace

inline bool needsPwmIsrFallback(int /*pin*/, u32 frequency_hz) {
    // Teensy FlexPWM/QuadTimer supports 1 Hz - 200 kHz natively
    if (frequency_hz >= 1 && frequency_hz <= 200000) {
        return false;
    }
    return true;
}

inline int setPwmFrequencyNative(int pin, u32 frequency_hz) {
    ::analogWriteFrequency(pin, frequency_hz);
    // Track the frequency per pin
    TeensyPwmFreq* table = getTeensyPwmTable();
    // Look for existing entry for this pin
    for (int i = 0; i < MAX_TEENSY_PWM_PINS; ++i) {
        if (table[i].pin == pin) {
            table[i].freq_hz = frequency_hz;
            return 0;
        }
    }
    // Find an unused slot
    for (int i = 0; i < MAX_TEENSY_PWM_PINS; ++i) {
        if (table[i].pin == -1) {
            table[i].pin = pin;
            table[i].freq_hz = frequency_hz;
            return 0;
        }
    }
    // No free slot available (should not happen with 40 slots on Teensy 4.x)
    return -4;
}

inline u32 getPwmFrequencyNative(int pin) {
    TeensyPwmFreq* table = getTeensyPwmTable();
    for (int i = 0; i < MAX_TEENSY_PWM_PINS; ++i) {
        if (table[i].pin == pin) {
            return table[i].freq_hz;
        }
    }
    return 0;
}

}  // namespace platforms
}  // namespace fl

#endif  // ARDUINO
