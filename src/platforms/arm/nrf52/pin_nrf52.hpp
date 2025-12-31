#pragma once

/// @file platforms/arm/nrf52/pin_nrf52.hpp
/// Nordic nRF52 (Adafruit Feather nRF52, etc.) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for nRF52 pin functions.
///
/// Arduino path: Wraps Arduino pin functions (nRF52 uses standard Arduino API)
///
/// IMPORTANT: Uses enum class types from fl/pin.h for type safety.

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>

namespace fl {
namespace platform {

// Forward declarations from fl/pin.h
enum class PinMode;
enum class PinValue;
enum class AdcRange;

inline void pinMode(int pin, PinMode mode) {
    ::pinMode(pin, static_cast<int>(mode));
}

inline void digitalWrite(int pin, PinValue val) {
    ::digitalWrite(pin, static_cast<int>(val));
}

inline PinValue digitalRead(int pin) {
    return ::digitalRead(pin) ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    return static_cast<uint16_t>(::analogRead(pin));
}

inline void analogWrite(int pin, uint16_t val) {
    ::analogWrite(pin, val);
}

inline void setPwm16(int pin, uint16_t val) {
    // NRF52 Arduino core typically provides 8-bit PWM via analogWrite
    // Scale 16-bit value down to 8-bit for Arduino compatibility
    // True 16-bit would require direct PWM peripheral access
    ::analogWrite(pin, static_cast<uint8_t>(val >> 8));
}

inline void setAdcRange(AdcRange range) {
    // Map AdcRange to nRF52 eAnalogReference values
    // nRF52 reference values: AR_DEFAULT, AR_INTERNAL (0.6V), AR_INTERNAL_3_0 (3.0V), AR_VDD4 (VDD/4)
    switch (range) {
        case AdcRange::Default:
            ::analogReference(AR_DEFAULT);  // VDD/4 (0.825V with VDD=3.3V)
            break;
        case AdcRange::Range0_1V1:
            ::analogReference(AR_INTERNAL);  // 0.6V internal reference (closest to 1.1V)
            break;
        case AdcRange::Range0_3V3:
            ::analogReference(AR_INTERNAL_3_0);  // 3.0V internal reference
            break;
        case AdcRange::Range0_1V5:
        case AdcRange::Range0_2V2:
        case AdcRange::Range0_5V:
        case AdcRange::External:
            // Not supported on nRF52, use default
            ::analogReference(AR_DEFAULT);
            break;
    }
}

}  // namespace platform
}  // namespace fl

#endif  // ARDUINO
