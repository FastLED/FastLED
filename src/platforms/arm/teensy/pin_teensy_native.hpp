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

// ============================================================================
// Pin mode control
// ============================================================================

inline void pinMode(int pin, int mode) {
#if defined(CORE_TEENSY)
    ::pinMode(pin, mode);
#else
    // No-op: Teensy core not available
    (void)pin;
    (void)mode;
#endif
}

// ============================================================================
// Digital I/O
// ============================================================================

inline void digitalWrite(int pin, int val) {
#if defined(CORE_TEENSY)
    ::digitalWrite(pin, val);
#else
    // No-op: Teensy core not available
    (void)pin;
    (void)val;
#endif
}

inline int digitalRead(int pin) {
#if defined(CORE_TEENSY)
    return ::digitalRead(pin);
#else
    // No-op: Teensy core not available
    (void)pin;
    return 0;  // Always return LOW
#endif
}

// ============================================================================
// Analog I/O
// ============================================================================

inline int analogRead(int pin) {
#if defined(CORE_TEENSY)
    return ::analogRead(pin);
#else
    // No-op: Teensy core not available
    (void)pin;
    return 0;  // Always return 0
#endif
}

inline void analogWrite(int pin, int val) {
#if defined(CORE_TEENSY)
    ::analogWrite(pin, val);
#else
    // No-op: Teensy core not available
    (void)pin;
    (void)val;
#endif
}

inline void analogReference(int mode) {
#if defined(CORE_TEENSY)
    ::analogReference(mode);
#else
    // No-op: Teensy core not available
    (void)mode;
#endif
}

}  // namespace fl
