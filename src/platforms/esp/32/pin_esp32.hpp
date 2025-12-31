#pragma once

/// @file platforms/esp/32/pin_esp32.hpp
/// ESP32 pin implementation (header-only)
///
/// Provides zero-overhead wrappers for ESP32 pin functions.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Wraps Arduino pin functions
/// 2. ESP-IDF path (#else): Uses pin_esp32_native.hpp (native ESP-IDF GPIO drivers)
///
/// IMPORTANT: Translates fl::PinMode/fl::PinValue/fl::AdcRange enum classes to platform-specific types.

#ifndef ARDUINO
#include "pin_esp32_native.hpp"
#endif

#ifdef ARDUINO
#include "platforms/esp/esp_version.h"

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>

namespace fl {
namespace platform {

inline void pinMode(int pin, PinMode mode) {
    // Translate PinMode enum to Arduino constants
    // PinMode::Input=0, Output=1, InputPullup=2, InputPulldown=3
    ::pinMode(pin, static_cast<int>(mode));
}

inline void digitalWrite(int pin, PinValue val) {
    // Translate PinValue enum to Arduino constants
    // PinValue::Low=0, High=1
    ::digitalWrite(pin, static_cast<int>(val));
}

inline PinValue digitalRead(int pin) {
    int raw = ::digitalRead(pin);
    return raw ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    return static_cast<uint16_t>(::analogRead(pin));
}

inline void analogWrite(int pin, uint16_t val) {
#if ESP_IDF_VERSION_4_OR_HIGHER
    // Arduino-ESP32 2.x+ (ESP-IDF 4.x+) provides analogWrite
    ::analogWrite(pin, static_cast<int>(val));
#else
    // ESP-IDF 3.x (Arduino-ESP32 1.x) does not provide analogWrite
    // No-op for compatibility - PWM would require LEDC setup
    (void)pin;
    (void)val;
#endif
}

inline void setPwm16(int pin, uint16_t val) {
#if ESP_IDF_VERSION_4_OR_HIGHER
    // ESP32: Accept 16-bit input, scale to Arduino's 8-bit analogWrite
    // Users apply gamma correction upstream, this function just scales
    // For higher resolution, use CLED class or direct LEDC configuration
    // Scale 16-bit (0-65535) to 8-bit (0-255)
    uint8_t val8 = val >> 8;
    ::analogWrite(pin, val8);
#else
    // ESP-IDF 3.x: No PWM support without manual LEDC configuration
    (void)pin;
    (void)val;
#endif
}

inline void setAdcRange(AdcRange range) {
    // ESP32 doesn't support analogReference in Arduino
    // ADC reference is fixed (different per ESP32 variant)
    // No-op for all ranges
    (void)range;
}

}  // namespace platform
}  // namespace fl

#endif  // ARDUINO
