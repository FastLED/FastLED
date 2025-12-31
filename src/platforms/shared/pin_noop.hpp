#pragma once

/// @file platforms/shared/pin_noop.hpp
/// No-op pin implementation for non-Arduino builds (host tests, simulators, etc.)
///
/// This provides stub implementations of all pin functions that compile but do nothing.
/// Used for:
/// - Host test builds (Linux, macOS, Windows)
/// - Simulators and emulators
/// - Any non-Arduino build where physical pin access is not available
///
/// Functions are placed in the platform namespace and will be wrapped by non-inline
/// functions in fl/pin.cpp.

namespace fl {
namespace platform {

// ============================================================================
// Pin mode control
// ============================================================================

inline void pinMode(int /*pin*/, PinMode /*mode*/) {
    // No-op: Host builds don't have physical pins
}

// ============================================================================
// Digital I/O
// ============================================================================

inline void digitalWrite(int /*pin*/, PinValue /*val*/) {
    // No-op: Host builds don't have physical pins
}

inline PinValue digitalRead(int /*pin*/) {
    // No-op: Host builds don't have physical pins
    return PinValue::Low;  // Always return LOW
}

// ============================================================================
// Analog I/O
// ============================================================================

inline uint16_t analogRead(int /*pin*/) {
    // No-op: Host builds don't have physical pins
    return 0;  // Always return 0
}

inline void analogWrite(int /*pin*/, uint16_t /*val*/) {
    // No-op: Host builds don't have physical pins
}

inline void setPwm16(int /*pin*/, uint16_t /*val*/) {
    // No-op: Host builds don't have physical pins
}

inline void setAdcRange(AdcRange /*range*/) {
    // No-op: Host builds don't have physical pins
}

}  // namespace platform
}  // namespace fl
