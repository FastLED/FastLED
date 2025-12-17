#pragma once

/// @file platforms/arm/stm32/pin_stm32.hpp
/// STM32 pin implementation (header-only)
///
/// Provides zero-overhead wrappers for STM32 pin functions.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Wraps Arduino pin functions
/// 2. Non-Arduino path (#else): STM32 HAL path: Uses pin_stm32_native.hpp
///
/// IMPORTANT: All functions use fl::PinMode/fl::PinValue/fl::AdcRange enum classes.

#ifndef ARDUINO
#include "pin_stm32_native.hpp"
#endif

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>

namespace fl {

inline void pinMode(int pin, PinMode mode) {
    // Translate fl::PinMode to Arduino pin mode integer
    // PinMode::Input=0, Output=1, InputPullup=2, InputPulldown=3
    int arduino_mode = static_cast<int>(mode);

#ifdef _WIRISH_WIRISH_H_
    // Maple framework uses WiringPinMode enum instead of int
    ::pinMode(pin, static_cast<WiringPinMode>(arduino_mode));
#elif defined(ARDUINO_ARCH_MBED) || defined(ARDUINO_GIGA)
    // Arduino MBED framework (e.g., Giga R1) uses PinMode enum
    ::pinMode(pin, static_cast<::PinMode>(arduino_mode));
#else
    ::pinMode(pin, arduino_mode);
#endif
}

inline void digitalWrite(int pin, PinValue val) {
    // Translate fl::PinValue to Arduino digital value integer
    // PinValue::Low=0, High=1
    int arduino_val = static_cast<int>(val);

#if defined(ARDUINO_ARCH_MBED) || defined(ARDUINO_GIGA)
    // Arduino MBED framework uses PinStatus enum
    ::digitalWrite(pin, static_cast<PinStatus>(arduino_val));
#else
    ::digitalWrite(pin, arduino_val);
#endif
}

inline PinValue digitalRead(int pin) {
    int val = ::digitalRead(pin);
    return val ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    return static_cast<uint16_t>(::analogRead(pin));
}

inline void analogWrite(int pin, uint16_t val) {
    ::analogWrite(pin, static_cast<int>(val));
}

inline void setAdcRange(AdcRange range) {
#if defined(_WIRISH_WIRISH_H_) || defined(ARDUINO_ARCH_MBED) || defined(ARDUINO_GIGA)
    // Maple framework and Arduino MBED don't support analogReference
    // Reference voltage is fixed by hardware (typically 3.3V)
    (void)range;  // Suppress unused parameter warning
#else
    // Translate fl::AdcRange to Arduino analogReference mode
    // This is platform-specific and may not map directly
    int mode = static_cast<int>(range);
    ::analogReference(mode);
#endif
}

}  // namespace fl

#endif  // ARDUINO
