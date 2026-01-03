#pragma once

/// @file platforms/arm/sam/pin_sam.hpp
/// SAM (Arduino Due, SAM3X8E) pin implementation (header-only)
///
/// Provides native SAM register manipulation without Arduino.h dependency.
///
/// Two paths:
/// 1. Arduino path (#ifdef ARDUINO): Direct PIO register access using SAM3X8E device headers
/// 2. Native SAM path (#else): Uses pin_sam_native.hpp for stub implementation
///
/// IMPORTANT: All functions use enum class types (PinMode, PinValue, AdcRange).

#ifndef ARDUINO
#include "pin_sam_native.hpp"
#endif

#ifdef ARDUINO

// ============================================================================
// Arduino/SAM Path: Native register manipulation (no Arduino.h dependency)
// ============================================================================
//
// This implementation uses direct SAM3X8E register access instead of Arduino
// wrapper functions. The register definitions (REG_PIOx_*, PIOA, etc.) come
// from the Arduino SAM core's device headers (sam3xa.h), which are automatically
// included when compiling for SAM platforms.
//
// Note: We still use the ARDUINO guard because:
// 1. The Arduino SAM core provides the necessary device header includes
// 2. This ensures compatibility with Arduino Due builds
// 3. Non-Arduino builds use pin_sam_native.hpp instead

// Include SAM device headers for Pio type and PIOA/PIOB/PIOC/PIOD register definitions
// chip.h includes sam.h which in turn includes the device-specific headers (sam3x8e.h, etc.)
#include <chip.h>

namespace fl {
namespace platform {

// Make enum types from fl namespace visible
using ::fl::PinMode;
using ::fl::PinValue;
using ::fl::AdcRange;

// Forward declarations for helper functions
namespace detail {
    // Pin mapping helper - converts Arduino pin number to (PIO controller, bit mask)
    // Returns nullptr for invalid pins
    inline Pio* getPioController(int pin, uint32_t& mask);
}

inline void pinMode(int pin, PinMode mode) {
    uint32_t mask;
    Pio* pio = detail::getPioController(pin, mask);
    if (!pio) return;

    // Enable PIO control of the pin
    pio->PIO_PER = mask;

    switch (mode) {
        case PinMode::Output:
            pio->PIO_OER = mask;   // Enable output
            pio->PIO_PUDR = mask;  // Disable pull-up
            break;
        case PinMode::Input:
            pio->PIO_ODR = mask;   // Disable output (input mode)
            pio->PIO_PUDR = mask;  // Disable pull-up
            break;
        case PinMode::InputPullup:
            pio->PIO_ODR = mask;   // Disable output (input mode)
            pio->PIO_PUER = mask;  // Enable pull-up
            break;
        case PinMode::InputPulldown:
            // SAM3X8E does not support internal pull-down resistors
            // Fall back to standard input mode
            pio->PIO_ODR = mask;   // Disable output (input mode)
            pio->PIO_PUDR = mask;  // Disable pull-up
            break;
    }
}

inline void digitalWrite(int pin, PinValue val) {
    uint32_t mask;
    Pio* pio = detail::getPioController(pin, mask);
    if (!pio) return;

    if (val == PinValue::High) {
        pio->PIO_SODR = mask;  // Set output data register
    } else {
        pio->PIO_CODR = mask;  // Clear output data register
    }
}

inline PinValue digitalRead(int pin) {
    uint32_t mask;
    Pio* pio = detail::getPioController(pin, mask);
    if (!pio) return PinValue::Low;

    // Read from pin data status register
    return (pio->PIO_PDSR & mask) ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    // ADC implementation requires complex peripheral configuration
    // For now, use stub - full implementation would require:
    // 1. Enable ADC peripheral clock via PMC
    // 2. Configure ADC resolution, timing, reference
    // 3. Map pin to ADC channel
    // 4. Start conversion and wait for completion
    // 5. Read ADC result register
    (void)pin;
    return 0;
}

inline void analogWrite(int pin, uint16_t val) {
    // PWM implementation requires complex peripheral configuration
    // For now, use stub - full implementation would require:
    // 1. Enable PWM peripheral clock via PMC
    // 2. Configure PWM channel mode, period, prescaler
    // 3. Map pin to PWM output
    // 4. Set duty cycle register
    (void)pin;
    (void)val;
}

inline void setPwm16(int pin, uint16_t val) {
    // 16-bit PWM would configure SAM3X8E PWM controller with CPRD=65535
    // For now, delegate to analogWrite stub
    analogWrite(pin, val);
}

inline void setAdcRange(AdcRange range) {
    // Arduino Due doesn't support analogReference - analog reference is fixed at 3.3V
    // No-op for all range values
    (void)range;
}

// ============================================================================
// Pin Mapping Implementation
// ============================================================================

namespace detail {

// Pin mapping table: converts Arduino pin numbers to (PIO controller, bit position)
// Based on Arduino Due pin mapping from fastpin_arm_sam.h
inline Pio* getPioController(int pin, uint32_t& mask) {
    // Arduino Due pin mapping (from fastpin_arm_sam.h)
    // Format: pin -> (controller, bit)
    switch (pin) {
        case 0:  mask = 1U << 8;  return PIOA;  // PA8
        case 1:  mask = 1U << 9;  return PIOA;  // PA9
        case 2:  mask = 1U << 25; return PIOB;  // PB25
        case 3:  mask = 1U << 28; return PIOC;  // PC28
        case 4:  mask = 1U << 26; return PIOC;  // PC26
        case 5:  mask = 1U << 25; return PIOC;  // PC25
        case 6:  mask = 1U << 24; return PIOC;  // PC24
        case 7:  mask = 1U << 23; return PIOC;  // PC23
        case 8:  mask = 1U << 22; return PIOC;  // PC22
        case 9:  mask = 1U << 21; return PIOC;  // PC21
        case 10: mask = 1U << 29; return PIOC;  // PC29
        case 11: mask = 1U << 7;  return PIOD;  // PD7
        case 12: mask = 1U << 8;  return PIOD;  // PD8
        case 13: mask = 1U << 27; return PIOB;  // PB27
        case 14: mask = 1U << 4;  return PIOD;  // PD4
        case 15: mask = 1U << 5;  return PIOD;  // PD5
        case 16: mask = 1U << 13; return PIOA;  // PA13
        case 17: mask = 1U << 12; return PIOA;  // PA12
        case 18: mask = 1U << 11; return PIOA;  // PA11
        case 19: mask = 1U << 10; return PIOA;  // PA10
        case 20: mask = 1U << 12; return PIOB;  // PB12
        case 21: mask = 1U << 13; return PIOB;  // PB13
        case 22: mask = 1U << 26; return PIOB;  // PB26
        case 23: mask = 1U << 14; return PIOA;  // PA14
        case 24: mask = 1U << 15; return PIOA;  // PA15
        case 25: mask = 1U << 0;  return PIOD;  // PD0
        case 26: mask = 1U << 1;  return PIOD;  // PD1
        case 27: mask = 1U << 2;  return PIOD;  // PD2
        case 28: mask = 1U << 3;  return PIOD;  // PD3
        case 29: mask = 1U << 6;  return PIOD;  // PD6
        case 30: mask = 1U << 9;  return PIOD;  // PD9
        case 31: mask = 1U << 7;  return PIOA;  // PA7
        case 32: mask = 1U << 10; return PIOD;  // PD10
        case 33: mask = 1U << 1;  return PIOC;  // PC1
        case 34: mask = 1U << 2;  return PIOC;  // PC2
        case 35: mask = 1U << 3;  return PIOC;  // PC3
        case 36: mask = 1U << 4;  return PIOC;  // PC4
        case 37: mask = 1U << 5;  return PIOC;  // PC5
        case 38: mask = 1U << 6;  return PIOC;  // PC6
        case 39: mask = 1U << 7;  return PIOC;  // PC7
        case 40: mask = 1U << 8;  return PIOC;  // PC8
        case 41: mask = 1U << 9;  return PIOC;  // PC9
        case 42: mask = 1U << 19; return PIOA;  // PA19
        case 43: mask = 1U << 20; return PIOA;  // PA20
        case 44: mask = 1U << 19; return PIOC;  // PC19
        case 45: mask = 1U << 18; return PIOC;  // PC18
        case 46: mask = 1U << 17; return PIOC;  // PC17
        case 47: mask = 1U << 16; return PIOC;  // PC16
        case 48: mask = 1U << 15; return PIOC;  // PC15
        case 49: mask = 1U << 14; return PIOC;  // PC14
        case 50: mask = 1U << 13; return PIOC;  // PC13
        case 51: mask = 1U << 12; return PIOC;  // PC12
        case 52: mask = 1U << 21; return PIOB;  // PB21
        case 53: mask = 1U << 14; return PIOB;  // PB14
        case 54: mask = 1U << 16; return PIOA;  // PA16
        case 55: mask = 1U << 24; return PIOA;  // PA24
        case 56: mask = 1U << 23; return PIOA;  // PA23
        case 57: mask = 1U << 22; return PIOA;  // PA22
        case 58: mask = 1U << 6;  return PIOA;  // PA6
        case 59: mask = 1U << 4;  return PIOA;  // PA4
        case 60: mask = 1U << 3;  return PIOA;  // PA3
        case 61: mask = 1U << 2;  return PIOA;  // PA2
        case 62: mask = 1U << 17; return PIOB;  // PB17
        case 63: mask = 1U << 18; return PIOB;  // PB18
        case 64: mask = 1U << 19; return PIOB;  // PB19
        case 65: mask = 1U << 20; return PIOB;  // PB20
        case 66: mask = 1U << 15; return PIOB;  // PB15
        case 67: mask = 1U << 16; return PIOB;  // PB16
        case 68: mask = 1U << 1;  return PIOA;  // PA1
        case 69: mask = 1U << 0;  return PIOA;  // PA0
        case 70: mask = 1U << 17; return PIOA;  // PA17
        case 71: mask = 1U << 18; return PIOA;  // PA18
        case 72: mask = 1U << 30; return PIOC;  // PC30
        case 73: mask = 1U << 21; return PIOA;  // PA21
        case 74: mask = 1U << 25; return PIOA;  // PA25 (SPI MISO)
        case 75: mask = 1U << 26; return PIOA;  // PA26 (SPI MOSI)
        case 76: mask = 1U << 27; return PIOA;  // PA27 (SPI SCK)
        case 77: mask = 1U << 28; return PIOA;  // PA28
        case 78: mask = 1U << 23; return PIOB;  // PB23

        // Digix extended pins (90-113)
        case 90:  mask = 1U << 0;  return PIOB;  // PB0
        case 91:  mask = 1U << 1;  return PIOB;  // PB1
        case 92:  mask = 1U << 2;  return PIOB;  // PB2
        case 93:  mask = 1U << 3;  return PIOB;  // PB3
        case 94:  mask = 1U << 4;  return PIOB;  // PB4
        case 95:  mask = 1U << 5;  return PIOB;  // PB5
        case 96:  mask = 1U << 6;  return PIOB;  // PB6
        case 97:  mask = 1U << 7;  return PIOB;  // PB7
        case 98:  mask = 1U << 8;  return PIOB;  // PB8
        case 99:  mask = 1U << 9;  return PIOB;  // PB9
        case 100: mask = 1U << 5;  return PIOA;  // PA5
        case 101: mask = 1U << 22; return PIOB;  // PB22
        case 102: mask = 1U << 23; return PIOB;  // PB23
        case 103: mask = 1U << 24; return PIOB;  // PB24
        case 104: mask = 1U << 27; return PIOC;  // PC27
        case 105: mask = 1U << 20; return PIOC;  // PC20
        case 106: mask = 1U << 11; return PIOC;  // PC11
        case 107: mask = 1U << 10; return PIOC;  // PC10
        case 108: mask = 1U << 21; return PIOA;  // PA21
        case 109: mask = 1U << 30; return PIOC;  // PC30
        case 110: mask = 1U << 29; return PIOB;  // PB29
        case 111: mask = 1U << 30; return PIOB;  // PB30
        case 112: mask = 1U << 31; return PIOB;  // PB31
        case 113: mask = 1U << 28; return PIOB;  // PB28

        default:
            mask = 0;
            return nullptr;  // Invalid pin
    }
}

} // namespace detail

}  // namespace platform
}  // namespace fl

#endif  // ARDUINO
