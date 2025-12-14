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
/// IMPORTANT: All functions return/accept int types only (no enums).

#ifndef ARDUINO
#include "pin_stm32_native.hpp"
#endif

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>

namespace fl {

inline void pinMode(int pin, int mode) {
#ifdef _WIRISH_WIRISH_H_
    // Maple framework uses WiringPinMode enum instead of int
    ::pinMode(pin, static_cast<WiringPinMode>(mode));
#elif defined(ARDUINO_ARCH_MBED) || defined(ARDUINO_GIGA)
    // Arduino MBED framework (e.g., Giga R1) uses PinMode enum
    ::pinMode(pin, static_cast<PinMode>(mode));
#else
    ::pinMode(pin, mode);
#endif
}

inline void digitalWrite(int pin, int val) {
#if defined(ARDUINO_ARCH_MBED) || defined(ARDUINO_GIGA)
    // Arduino MBED framework uses PinStatus enum
    ::digitalWrite(pin, static_cast<PinStatus>(val));
#else
    ::digitalWrite(pin, val);
#endif
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
#if defined(_WIRISH_WIRISH_H_) || defined(ARDUINO_ARCH_MBED) || defined(ARDUINO_GIGA)
    // Maple framework and Arduino MBED don't support analogReference
    // Reference voltage is fixed by hardware (typically 3.3V)
    (void)mode;  // Suppress unused parameter warning
#else
    ::analogReference(static_cast<eAnalogReference>(mode));
#endif
}

}  // namespace fl

#endif  // ARDUINO
