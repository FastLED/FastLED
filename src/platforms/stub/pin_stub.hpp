#pragma once

// IWYU pragma: private

/// @file platforms/stub/pin_stub.hpp
/// Pin implementation for the stub (native/host) platform.
///
/// Tracks pin state and records edges via fl::stub::setPinState /
/// fl::stub::getPinState, enabling NativeRxDevice and ClocklessController
/// stub to observe simulated GPIO transitions.

#include "platforms/stub/stub_gpio.h"

namespace fl {
namespace platforms {

// ============================================================================
// Pin mode control
// ============================================================================

inline void pinMode(int /*pin*/, PinMode /*mode*/) {
    // No physical pins on host builds
}

// ============================================================================
// Digital I/O
// ============================================================================

inline void digitalWrite(int pin, PinValue val) {
    fl::stub::setPinState(pin, val == PinValue::High);
}

inline PinValue digitalRead(int pin) {
    return fl::stub::getPinState(pin) ? PinValue::High : PinValue::Low;
}

// ============================================================================
// Analog I/O
// ============================================================================

inline u16 analogRead(int /*pin*/) {
    return 0;
}

inline void analogWrite(int /*pin*/, u16 /*val*/) {}

inline void setPwm16(int /*pin*/, u16 /*val*/) {}

inline void setAdcRange(AdcRange /*range*/) {}

// ============================================================================
// PWM Frequency Control
// ============================================================================

inline bool needsPwmIsrFallback(int /*pin*/, u32 /*frequency_hz*/) {
    return true;
}

inline int setPwmFrequencyNative(int /*pin*/, u32 /*frequency_hz*/) {
    return -4;
}

inline u32 getPwmFrequencyNative(int /*pin*/) {
    return 0;
}

}  // namespace platforms
}  // namespace fl
