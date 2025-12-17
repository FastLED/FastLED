#pragma once

/// @file platforms/esp/8266/pin_esp8266.hpp
/// ESP8266 pin implementation (header-only)
///
/// Provides zero-overhead wrappers for ESP8266 pin functions.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Wraps Arduino pin functions
/// 2. ESP8266 SDK path (#else): Uses pin_esp8266_native.hpp

#ifndef ARDUINO
#include "pin_esp8266_native.hpp"
#endif

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Zero-overhead wrappers around Arduino pin functions
// ============================================================================

#include <Arduino.h>
#include "fl/pin.h"

namespace fl {

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
    ::analogWrite(pin, static_cast<int>(val));
}

inline void setAdcRange(AdcRange range) {
    // ESP8266 ADC reference voltage is fixed at 1.0V (internal reference)
    // This function has no effect on ESP8266 hardware - reference cannot be changed
    (void)range;  // Parameter unused - no-op
}

}  // namespace fl

#endif  // ARDUINO
