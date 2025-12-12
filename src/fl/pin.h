#pragma once

/// @file fl/pin.h
/// Platform-independent pin API
///
/// CRITICAL DESIGN PRINCIPLE: This header MUST remain minimal and self-contained.
/// NO platform headers (Arduino.h, driver headers, etc.) should EVER be included here.
///
/// Architecture:
/// - This file: Minimal public interface (enum declarations + function signatures)
///   → Users include this and get ONLY the interface
///   → No Arduino.h, no platform headers, no dependencies beyond stdlib
///
/// - fl/pin.cpp: Compilation boundary
///   → Includes platforms/pin.h which pulls in platform-specific implementations
///   → Prevents platform headers from leaking into user code
///
/// - platforms/pin.h: Trampoline dispatcher
///   → Includes the appropriate platform .hpp file based on compile-time detection
///
/// - platforms/*/pin.hpp: Platform implementations (header-only)
///   → Provides inline implementations for zero-overhead
///   → Split into Arduino path (#ifdef ARDUINO) and non-Arduino path
///
/// Why this matters:
/// - Users can safely `#include "fl/pin.h"` without pulling in Arduino.h
/// - Platform detection and implementation selection happens at the compilation boundary
/// - Clean separation between interface and implementation
/// - Better compile times and reduced header pollution

namespace fl {

// ============================================================================
// Function declarations (implementations in platform-specific .hpp files)
// ============================================================================

/// Set pin mode (input, output, pull-up, pull-down)
/// @param pin Pin number (platform-specific numbering)
/// @param mode Pin mode (kInput, kOutput, kInputPullup, kInputPulldown)
inline void pinMode(int pin, int mode);

/// Write digital value to pin
/// @param pin Pin number (platform-specific numbering)
/// @param val Pin value (kLow or kHigh)
inline void digitalWrite(int pin, int val);

/// Read digital value from pin
/// @param pin Pin number (platform-specific numbering)
/// @return Pin value (kLow or kHigh)
inline int digitalRead(int pin);

/// Read analog value from pin
/// @param pin Pin number (platform-specific numbering)
/// @return Analog value (platform-specific range, typically 0-1023 or 0-4095)
inline int analogRead(int pin);

/// Write analog value to pin (PWM)
/// @param pin Pin number (platform-specific numbering)
/// @param val Analog value (platform-specific range, typically 0-255)
inline void analogWrite(int pin, int val);

/// Set analog reference voltage mode
/// @param mode Reference mode (platform-specific values)
inline void analogReference(int mode);

}  // namespace fl
