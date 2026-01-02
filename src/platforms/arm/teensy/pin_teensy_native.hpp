#pragma once

/// @file platforms/arm/teensy/pin_teensy_native.hpp
/// Native Teensy pin implementation (non-Arduino builds)
///
/// This provides pin access for Teensy boards without Arduino framework.
/// Uses Teensy's native core_pins.h functions which are available even
/// in non-Arduino builds (bare-metal Teensy development).
///
/// Supported Teensy boards:
/// - Teensy 3.0, 3.1, 3.2 (ARM Cortex-M4)
/// - Teensy 3.5, 3.6 (ARM Cortex-M4F)
/// - Teensy LC (ARM Cortex-M0+)
/// - Teensy 4.0, 4.1 (ARM Cortex-M7)
///
/// Note: Teensy core provides these functions in both Arduino and non-Arduino builds.
/// The Teensy core headers (core_pins.h) define the standard Arduino-like functions.

#if defined(CORE_TEENSY)
    // Teensy core is available - use native Teensy functions
    // These are defined in the Teensy core's core_pins.h
    #include <core_pins.h>
#else
    // Fallback: Define basic pin constants if Teensy core not available
    #ifndef OUTPUT
        #define OUTPUT 1
    #endif
    #ifndef INPUT
        #define INPUT 0
    #endif
    #ifndef INPUT_PULLUP
        #define INPUT_PULLUP 2
    #endif
    #ifndef HIGH
        #define HIGH 1
    #endif
    #ifndef LOW
        #define LOW 0
    #endif
    #ifndef DEFAULT
        #define DEFAULT 0
    #endif
    #ifndef EXTERNAL
        #define EXTERNAL 1
    #endif
    #ifndef INTERNAL
        #define INTERNAL 2
    #endif
#endif

namespace fl {
namespace platform {

// ============================================================================
// Pin mode control
// ============================================================================

inline void pinMode(int pin, fl::PinMode mode) {
#if defined(CORE_TEENSY)
    // Translate PinMode to Teensy core constants
    // PinMode::Input=0, Output=1, InputPullup=2, InputPulldown=3
    // Teensy: INPUT=0, OUTPUT=1, INPUT_PULLUP=2, INPUT_PULLDOWN (Teensy-specific)
    int teensy_mode;
    switch (mode) {
        case fl::PinMode::Input:
            teensy_mode = INPUT;  // 0
            break;
        case fl::PinMode::Output:
            teensy_mode = OUTPUT;  // 1
            break;
        case fl::PinMode::InputPullup:
            teensy_mode = INPUT_PULLUP;  // 2
            break;
        case fl::PinMode::InputPulldown:
#ifdef INPUT_PULLDOWN
            teensy_mode = INPUT_PULLDOWN;  // Teensy-specific
#else
            teensy_mode = INPUT_PULLUP;  // Fallback to INPUT_PULLUP
#endif
            break;
    }
    ::pinMode(pin, teensy_mode);
#else
    // No-op: Teensy core not available
    (void)pin;
    (void)mode;
#endif
}

// ============================================================================
// Digital I/O
// ============================================================================

inline void digitalWrite(int pin, fl::PinValue val) {
#if defined(CORE_TEENSY)
    ::digitalWrite(pin, static_cast<int>(val));  // PinValue::Low=0, High=1
#else
    // No-op: Teensy core not available
    (void)pin;
    (void)val;
#endif
}

inline fl::PinValue digitalRead(int pin) {
#if defined(CORE_TEENSY)
    return ::digitalRead(pin) ? fl::PinValue::High : fl::PinValue::Low;
#else
    // No-op: Teensy core not available
    (void)pin;
    return fl::PinValue::Low;  // Always return Low
#endif
}

// ============================================================================
// Analog I/O
// ============================================================================

inline uint16_t analogRead(int pin) {
#if defined(CORE_TEENSY)
    return static_cast<uint16_t>(::analogRead(pin));
#else
    // No-op: Teensy core not available
    (void)pin;
    return 0;  // Always return 0
#endif
}

inline void analogWrite(int pin, uint16_t val) {
#if defined(CORE_TEENSY)
    ::analogWrite(pin, val);
#else
    // No-op: Teensy core not available
    (void)pin;
    (void)val;
#endif
}

inline void setPwm16(int pin, uint16_t val) {
#if defined(CORE_TEENSY)
    // Teensy native path: Use analogWriteResolution for 16-bit PWM
    analogWriteResolution(16);
    ::analogWrite(pin, val);
#else
    // No-op: Teensy core not available
    (void)pin;
    (void)val;
#endif
}

inline void setAdcRange(fl::AdcRange range) {
#if defined(CORE_TEENSY)
    // Translate AdcRange to Teensy analogReference() constants
    // Teensy supports: DEFAULT, INTERNAL, EXTERNAL
    int ref_mode;
    switch (range) {
        case fl::AdcRange::Default:
            ref_mode = DEFAULT;
            break;
        case fl::AdcRange::Range0_1V1:
            ref_mode = INTERNAL;  // 1.2V internal reference on Teensy
            break;
        case fl::AdcRange::External:
            ref_mode = EXTERNAL;  // External AREF pin
            break;
        default:
            // Unsupported ranges - use default
            ref_mode = DEFAULT;
            break;
    }
    ::analogReference(ref_mode);
#else
    // No-op: Teensy core not available
    (void)range;
#endif
}

}  // namespace platform
}  // namespace fl
