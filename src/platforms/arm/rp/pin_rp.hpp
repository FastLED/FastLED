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
/// IMPORTANT: All functions use enum class types from fl/pin.h for type safety.

#ifndef ARDUINO
#include "pin_rp_native.hpp"
#endif

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>
#include "fl/pin.h"

namespace fl {

inline void pinMode(int pin, PinMode mode) {
    // Translate fl::PinMode to Arduino PinMode constants
    // PinMode::Input = 0 -> INPUT
    // PinMode::Output = 1 -> OUTPUT
    // PinMode::InputPullup = 2 -> INPUT_PULLUP
    // PinMode::InputPulldown = 3 -> INPUT_PULLDOWN
    ::pinMode(pin, static_cast<::PinMode>(static_cast<int>(mode)));
}

inline void digitalWrite(int pin, PinValue val) {
    // Translate fl::PinValue to Arduino PinStatus
    // PinValue::Low = 0 -> LOW
    // PinValue::High = 1 -> HIGH
    ::digitalWrite(pin, static_cast<PinStatus>(static_cast<int>(val)));
}

inline PinValue digitalRead(int pin) {
    int result = ::digitalRead(pin);
    return result ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    return static_cast<uint16_t>(::analogRead(pin));
}

inline void analogWrite(int pin, uint16_t val) {
    ::analogWrite(pin, static_cast<int>(val));
}

inline void setAdcRange(AdcRange range) {
    // RP2040/RP2350 uses fixed 3.3V reference, so this is a no-op
    // analogReference() exists in Arduino API but has no effect on RP2040
    (void)range;  // Suppress unused parameter warning
}

}  // namespace fl

#endif  // ARDUINO
