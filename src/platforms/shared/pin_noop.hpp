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
/// All functions are constexpr inline for zero overhead.

namespace fl {

// ============================================================================
// Pin mode control
// ============================================================================

inline void pinMode(int /*pin*/, int /*mode*/) {
    // No-op: Host builds don't have physical pins
}

// ============================================================================
// Digital I/O
// ============================================================================

inline void digitalWrite(int /*pin*/, int /*val*/) {
    // No-op: Host builds don't have physical pins
}

inline int digitalRead(int /*pin*/) {
    // No-op: Host builds don't have physical pins
    return 0;  // Always return LOW
}

// ============================================================================
// Analog I/O
// ============================================================================

inline int analogRead(int /*pin*/) {
    // No-op: Host builds don't have physical pins
    return 0;  // Always return 0
}

inline void analogWrite(int /*pin*/, int /*val*/) {
    // No-op: Host builds don't have physical pins
}

inline void analogReference(int /*mode*/) {
    // No-op: Host builds don't have physical pins
}

}  // namespace fl
