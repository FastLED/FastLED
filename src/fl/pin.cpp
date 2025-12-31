/// @file fl/pin.cpp
/// Compilation boundary for platform-independent pin API
///
/// IMPORTANT: This file acts as a compilation boundary to prevent header pollution.
///
/// Architecture:
/// - fl/pin.h: Minimal public interface (enum declarations + function signatures)
///   → Users include this and get ONLY the interface (no Arduino.h, no platform headers)
///
/// - fl/pin.cpp (this file): Compilation boundary
///   → Includes platforms/pin.h which pulls in platform-specific implementations
///   → Prevents platform headers from leaking into user code
///   → Provides non-inline wrapper functions that forward to platform implementations
///
/// - platforms/pin.h: Trampoline dispatcher
///   → Includes the appropriate platform .hpp file based on compile-time detection
///
/// - platforms/*/pin.hpp: Platform implementations (header-only)
///   → Provides inline/constexpr implementations for zero-overhead
///   → Split into Arduino path (#ifdef ARDUINO) and non-Arduino path
///
/// Why this matters:
/// - Users can safely `#include "fl/pin.h"` without pulling in Arduino.h
/// - Platform detection and implementation selection happens at this boundary
/// - Clean separation between interface and implementation

#include "fl/pin.h"

// Include platform-specific implementations (must come after fl/pin.h for proper type resolution)
#include "platforms/pin.h"

namespace fl {

// ============================================================================
// Non-inline wrapper functions
// ============================================================================
// These wrappers are necessary because the platform-specific implementations
// are inline functions. The linker needs actual symbols to link against when
// other translation units call these functions.

void pinMode(int pin, PinMode mode) {
    // Forward to platform-specific inline implementation
    platform::pinMode(pin, mode);
}

void digitalWrite(int pin, PinValue val) {
    // Forward to platform-specific inline implementation
    platform::digitalWrite(pin, val);
}

PinValue digitalRead(int pin) {
    // Forward to platform-specific inline implementation
    return platform::digitalRead(pin);
}

uint16_t analogRead(int pin) {
    // Forward to platform-specific inline implementation
    return platform::analogRead(pin);
}

void analogWrite(int pin, uint16_t val) {
    // Forward to platform-specific inline implementation
    platform::analogWrite(pin, val);
}

void setPwm16(int pin, uint16_t val) {
    // Forward to platform-specific inline implementation
    platform::setPwm16(pin, val);
}

void setAdcRange(AdcRange range) {
    // Forward to platform-specific inline implementation
    platform::setAdcRange(range);
}

}  // namespace fl
