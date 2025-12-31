#pragma once

/// @file platforms/avr/pin_avr.hpp
/// AVR (Arduino Uno, Mega, etc.) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for AVR pin functions.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Wraps Arduino pin functions
/// 2. Non-Arduino path (#else): Native AVR path via pin_avr_native.hpp
///
/// IMPORTANT: Translates fl:: enum classes to Arduino int constants.

#ifndef ARDUINO
#include "pin_avr_native.hpp"
#endif

#ifdef ARDUINO

// ============================================================================
// Arduino Path: Translates fl:: enum classes to Arduino constants
// ============================================================================

#include <Arduino.h>

// Analog reference modes fallback definitions (for ATmega8 and other old AVR cores)
#ifndef DEFAULT
#define DEFAULT 1
#endif
#ifndef INTERNAL
#define INTERNAL 3
#endif
#ifndef EXTERNAL
#define EXTERNAL 0
#endif

namespace fl {
namespace platform {

// Forward declarations from fl/pin.h
enum class PinMode;
enum class PinValue;
enum class AdcRange;

inline void pinMode(int pin, PinMode mode) {
    // Translate PinMode to Arduino constants (0=INPUT, 1=OUTPUT, 2=INPUT_PULLUP)
    // PinMode: Input=0, Output=1, InputPullup=2, InputPulldown=3
    // AVR doesn't support InputPulldown (no hardware pull-down resistors)
    int arduino_mode = static_cast<int>(mode);
    if (arduino_mode == 3) {  // InputPulldown -> treat as Input
        arduino_mode = 0;  // INPUT
    }
    ::pinMode(pin, arduino_mode);
}

inline void digitalWrite(int pin, PinValue val) {
    // Translate PinValue to Arduino constants (0=LOW, 1=HIGH)
    ::digitalWrite(pin, static_cast<int>(val));
}

inline PinValue digitalRead(int pin) {
    // Translate Arduino return value (0=LOW, 1=HIGH) to PinValue
    return ::digitalRead(pin) ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    return static_cast<uint16_t>(::analogRead(pin));
}

inline void analogWrite(int pin, uint16_t val) {
    ::analogWrite(pin, static_cast<int>(val));
}

inline void setPwm16(int pin, uint16_t val) {
    // AVR: Only Timer1 (pins 9,10 on Uno, more on Mega) supports true 16-bit PWM
    // Arduino's analogWrite uses 8-bit resolution
    // Scale 16-bit value down to 8-bit for Arduino compatibility
    ::analogWrite(pin, static_cast<int>(val >> 8));
}

inline void setAdcRange(AdcRange range) {
    // Translate AdcRange to Arduino analogReference constants
    // AdcRange: Default=0, Range0_1V1=1, Range0_1V5=2, Range0_2V2=3, Range0_3V3=4, Range0_5V=5, External=6
    // AVR analogReference: DEFAULT=1, INTERNAL=3, EXTERNAL=0
    int arduino_ref;
    switch (range) {
        case AdcRange::Default:
            arduino_ref = DEFAULT;  // 1 (5V on 5V boards)
            break;
        case AdcRange::Range0_1V1:
            arduino_ref = INTERNAL;  // 3 (1.1V internal reference)
            break;
        case AdcRange::External:
            arduino_ref = EXTERNAL;  // 0 (AREF pin)
            break;
        case AdcRange::Range0_5V:
            arduino_ref = DEFAULT;  // 1 (same as Default for 5V AVR boards)
            break;
        default:
            // Other ranges not supported on AVR (no-op)
            return;
    }
    ::analogReference(arduino_ref);
}

}  // namespace platform
}  // namespace fl

#endif  // ARDUINO
