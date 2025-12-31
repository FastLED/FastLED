#pragma once

/// @file fl/pin.h
/// Platform-independent pin API
///
/// CRITICAL DESIGN PRINCIPLE: This header MUST remain minimal and self-contained.
/// NO platform headers (Arduino.h, driver headers, etc.) should EVER be included here.
///
/// Architecture:
/// - This file: Minimal public interface (enum class declarations + function signatures)
///   → Users include this and get ONLY the interface
///   → No Arduino.h, no platform headers, no dependencies beyond stdlib
///
/// - fl/pin.cpp: Compilation boundary
///   → Includes platforms/pin.h which pulls in platform-specific implementations
///   → Prevents platform headers from leaking into user code
///
/// - platforms/pin.h: Trampoline dispatcher
///   → Includes the appropriate platform-specific pin.hpp file based on compile-time detection
///
/// - platforms/*/pin.hpp: Platform implementations (header-only)
///   → Provides inline implementations for zero-overhead
///   → Translates fl::PinMode/fl::PinValue/fl::AdcRange to platform-specific constants
///
/// Why this matters:
/// - Users can safely `#include "fl/pin.h"` without pulling in Arduino.h
/// - Type-safe enum classes prevent accidental int misuse
/// - Platform detection and implementation selection happens at the compilation boundary
/// - Clean separation between interface and implementation
/// - Better compile times and reduced header pollution

#include "fl/stl/stdint.h"

namespace fl {

// ============================================================================
// Pin Configuration Enums
// ============================================================================

/// Pin mode configuration
enum class PinMode {
    Input = 0,         ///< Digital input (high impedance)
    Output,            ///< Digital output (push-pull)
    InputPullup,       ///< Digital input with internal pull-up resistor
    InputPulldown      ///< Digital input with internal pull-down resistor
};

/// Digital pin value
enum class PinValue {
    Low = 0,           ///< Logic low (0V / GND)
    High = 1           ///< Logic high (3.3V / 5V, platform-dependent)
};

/// ADC voltage range configuration
/// @note Different platforms implement this differently (reference voltage vs attenuation)
/// @note Not all ranges are available on all platforms (no-op for unsupported values)
enum class AdcRange {
    Default = 0,       ///< Platform default (5V on AVR Uno, 3.3V on ESP32 w/ 11dB, etc.)
    Range0_1V1,        ///< 0-1.1V range (INTERNAL on AVR, 0dB on ESP32)
    Range0_1V5,        ///< 0-1.5V range (2.5dB on ESP32)
    Range0_2V2,        ///< 0-2.2V range (6dB on ESP32)
    Range0_3V3,        ///< 0-3.3V range (11dB on ESP32, VDDANA on 3.3V SAMD)
    Range0_5V,         ///< 0-5V range (DEFAULT on 5V AVR boards)
    External           ///< External reference voltage on AREF pin (AVR/SAMD only)
};

// ============================================================================
// Function declarations (implementations in platform-specific .hpp files)
// ============================================================================

/// Set pin mode (input, output, pull-up, pull-down)
/// @param pin Pin number (platform-specific numbering)
/// @param mode Pin mode configuration
void pinMode(int pin, PinMode mode);

/// Write digital value to pin
/// @param pin Pin number (platform-specific numbering)
/// @param val Pin value (Low or High)
void digitalWrite(int pin, PinValue val);

/// Read digital value from pin
/// @param pin Pin number (platform-specific numbering)
/// @return Pin value (Low or High)
PinValue digitalRead(int pin);

/// Read analog value from pin
/// @param pin Pin number (platform-specific numbering)
/// @return Analog value (0-1023 for 10-bit ADC, 0-4095 for 12-bit ADC)
uint16_t analogRead(int pin);

/// Write analog value to pin (PWM)
/// @param pin Pin number (platform-specific numbering)
/// @param val PWM duty cycle (0-255 typical, platform-specific maximum)
void analogWrite(int pin, uint16_t val);

/// Set PWM duty cycle with 16-bit resolution
/// @param pin Pin number (platform-specific numbering)
/// @param val PWM duty cycle (0-65535, platform-specific maximum)
void setPwm16(int pin, uint16_t val);

/// Alias for setPwm16 - Set PWM duty cycle with 16-bit resolution
/// @param pin Pin number (platform-specific numbering)
/// @param val PWM duty cycle (0-65535, platform-specific maximum)
inline void analogWrite16(int pin, uint16_t val) {
    setPwm16(pin, val);
}

/// Set ADC voltage range
/// @param range Voltage range for analog readings
/// @note Implementation varies by platform (reference voltage vs attenuation)
/// @note Not all ranges supported on all platforms (no-op for unsupported values)
void setAdcRange(AdcRange range);

}  // namespace fl
