#pragma once

/// @file platforms/arm/samd/pin_samd_native.hpp
/// Native SAMD pin system implementation using direct PORT register access
///
/// Implements Arduino-compatible pin functions using SAMD21/SAMD51 PORT registers.
/// This provides GPIO control without requiring the Arduino framework.
///
/// SAMD PORT Architecture:
/// - Ports organized into groups (PORT->Group[0] = PORTA, Group[1] = PORTB, etc.)
/// - Each group has 32 pins maximum
/// - Key registers per group:
///   - DIR: Data Direction (0=input, 1=output)
///   - OUT: Output Value
///   - IN: Input Value
///   - OUTSET: Set output bits (write 1 to set)
///   - OUTCLR: Clear output bits (write 1 to clear)
///   - OUTTGL: Toggle output bits (write 1 to toggle)
///   - PINCFG[n]: Pin configuration (input enable, pull, etc.)
///
/// Pin Mapping:
/// - Arduino pin numbers must be converted to (port_group, pin_bit) pairs
/// - This implementation uses a simple lookup table approach
/// - Board-specific mappings can be extended as needed

#if defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || \
    defined(__SAMD21E17A__) || defined(__SAMD21E18A__) || \
    defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || \
    defined(__SAME51J19A__) || defined(__SAMD51P19A__) || \
    defined(__SAMD51P20A__)

#include "fl/pin.h"
#include <sam.h>  // SAMD register definitions

namespace fl {
namespace platform {

// ============================================================================
// Pin Mapping Structures
// ============================================================================

/// Pin mapping: converts Arduino pin number to (port_group, pin_bit)
struct PinMapping {
    uint8_t group;  ///< Port group (0=PORTA, 1=PORTB, etc.)
    uint8_t bit;    ///< Bit position within group (0-31)
};

/// Get pin mapping for Arduino pin number
/// @note This is a simplified implementation. Board-specific variants
///       should provide accurate mappings via g_APinDescription table.
/// @param pin Arduino pin number
/// @return PinMapping with group and bit, or {0xFF, 0xFF} if invalid
inline PinMapping getPinMapping(int pin) {
    // Simple default mapping for common SAMD boards
    // In a real implementation, this would use the variant's g_APinDescription table
    // or compile-time board-specific lookup tables.

    // For now, assume a simple linear mapping as fallback
    // Most SAMD boards have pins mapped sequentially within port groups
    if (pin < 0 || pin > 127) {
        return {0xFF, 0xFF};  // Invalid pin
    }

    // Simple heuristic: first 32 pins on PORTA, next 32 on PORTB, etc.
    // Real board definitions should override this
    uint8_t group = static_cast<uint8_t>(pin / 32);
    uint8_t bit = static_cast<uint8_t>(pin % 32);

    return {group, bit};
}

// ============================================================================
// GPIO Functions - Native SAMD Implementation
// ============================================================================

/// Set pin mode (input, output, input_pullup, input_pulldown)
/// @param pin Arduino pin number
/// @param mode Pin mode (PinMode enum)
inline void pinMode(int pin, PinMode mode) {
    PinMapping pm = getPinMapping(pin);
    if (pm.group == 0xFF) {
        return;  // Invalid pin
    }

    PortGroup* port = &PORT->Group[pm.group];
    uint32_t mask = 1ul << pm.bit;

    switch (mode) {
        case PinMode::Output:
            // Set as output: DIR bit = 1
            port->DIRSET.reg = mask;
            // Disable input buffer and pull resistor
            port->PINCFG[pm.bit].reg = 0;
            break;

        case PinMode::Input:
            // Set as input: DIR bit = 0
            port->DIRCLR.reg = mask;
            // Enable input buffer, disable pull resistor
            port->PINCFG[pm.bit].reg = PORT_PINCFG_INEN;
            break;

        case PinMode::InputPullup:
            // Set as input with pull-up
            port->DIRCLR.reg = mask;
            // Enable input buffer and pull resistor, set output high for pull-up
            port->OUTSET.reg = mask;
            port->PINCFG[pm.bit].reg = PORT_PINCFG_INEN | PORT_PINCFG_PULLEN;
            break;

        case PinMode::InputPulldown:
            // Set as input with pull-down
            port->DIRCLR.reg = mask;
            // Enable input buffer and pull resistor, set output low for pull-down
            port->OUTCLR.reg = mask;
            port->PINCFG[pm.bit].reg = PORT_PINCFG_INEN | PORT_PINCFG_PULLEN;
            break;

        default:
            // Unknown mode, default to input
            port->DIRCLR.reg = mask;
            port->PINCFG[pm.bit].reg = PORT_PINCFG_INEN;
            break;
    }
}

/// Write digital output value
/// @param pin Arduino pin number
/// @param val Output value (PinValue enum)
inline void digitalWrite(int pin, PinValue val) {
    PinMapping pm = getPinMapping(pin);
    if (pm.group == 0xFF) {
        return;  // Invalid pin
    }

    PortGroup* port = &PORT->Group[pm.group];
    uint32_t mask = 1ul << pm.bit;

    if (val == PinValue::High) {
        // Set pin high using OUTSET register (atomic set)
        port->OUTSET.reg = mask;
    } else {
        // Set pin low using OUTCLR register (atomic clear)
        port->OUTCLR.reg = mask;
    }
}

/// Read digital input value
/// @param pin Arduino pin number
/// @return Pin value (PinValue enum: Low or High)
inline PinValue digitalRead(int pin) {
    PinMapping pm = getPinMapping(pin);
    if (pm.group == 0xFF) {
        return PinValue::Low;  // Invalid pin
    }

    PortGroup* port = &PORT->Group[pm.group];
    uint32_t mask = 1ul << pm.bit;

    // Read from IN register
    return (port->IN.reg & mask) ? PinValue::High : PinValue::Low;
}

/// Read analog input value
/// @param pin Arduino pin number
/// @return ADC value (0-1023 for 10-bit, 0-4095 for 12-bit)
/// @note STUB: Real implementation requires ADC peripheral configuration
inline uint16_t analogRead(int /*pin*/) {
    // STUB: ADC implementation requires:
    // 1. Enable ADC peripheral clock (GCLK + MCLK/PM)
    // 2. Configure ADC resolution, reference, prescaler
    // 3. Map pin to ADC channel (AIN[0..15])
    // 4. Start conversion and wait for completion
    // 5. Read ADC result register
    //
    // This is complex enough to warrant a separate ADC driver module.
    // For now, return 0 as stub.
    return 0;
}

/// Write analog output value (PWM)
/// @param pin Arduino pin number
/// @param val PWM duty cycle (0-255)
/// @note STUB: Real implementation requires TCC/TC peripheral configuration
inline void analogWrite(int /*pin*/, uint16_t /*val*/) {
    // STUB: PWM implementation requires:
    // 1. Enable TCC/TC peripheral clock (GCLK + MCLK/PM)
    // 2. Configure timer mode, period, prescaler
    // 3. Map pin to timer output (TCC/TC peripheral pinmux)
    // 4. Set compare/capture register for duty cycle
    // 5. Enable timer
    //
    // This is complex enough to warrant a separate PWM driver module.
    // For now, no-op as stub.
}

/// Set PWM duty cycle with 16-bit resolution
/// @param pin Arduino pin number
/// @param val PWM duty cycle (0-65535)
/// @note STUB: Real implementation requires TCC/TC peripheral configuration
inline void setPwm16(int /*pin*/, uint16_t /*val*/) {
    // STUB: 16-bit PWM would use same TCC/TC configuration as analogWrite
    // but with 16-bit period and compare registers
    // For now, no-op as stub.
}

/// Set ADC voltage range
/// @param range ADC voltage range (AdcRange enum)
/// @note STUB: Real implementation requires ADC REFCTRL register configuration
inline void setAdcRange(AdcRange /*range*/) {
    // STUB: Analog reference configuration requires:
    // 1. Access to ADC peripheral
    // 2. Configure ADC REFCTRL register
    // 3. Reference options:
    //    - INTREF (1.0V internal reference)
    //    - INTVCC0 (1/1.6 VDDANA)
    //    - INTVCC1 (1/2 VDDANA)
    //    - VREFA (External VREFA pin)
    //    - VREFB (External VREFB pin)
    //
    // For now, no-op as stub.
}

}  // namespace platform
}  // namespace fl

#endif  // SAMD21/SAMD51 platform guards
