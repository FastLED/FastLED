#pragma once

/// @file platforms/arm/renesas/pin_renesas.hpp
/// Renesas (Arduino UNO R4, etc.) native pin implementation using FSP IOPORT API
///
/// Provides native pin control using Renesas Flexible Software Package (FSP) IOPORT
/// HAL driver for direct hardware access. Requires Arduino framework for type
/// definitions and pin mapping (g_pin_cfg[] from variant.cpp).
///
/// Architecture:
/// - Uses R_IOPORT_PinCfg() for pin mode configuration
/// - Uses R_IOPORT_PinWrite() for atomic digital output
/// - Uses R_IOPORT_PinRead() for digital input
/// - Pin mapping via g_pin_cfg[] array (PinMuxCfg_t from Arduino variant)

#if defined(ARDUINO_ARCH_RENESAS) || defined(FL_IS_RENESAS)

#include "fl/pin.h"

// Include Arduino.h to get all Renesas FSP headers and type definitions
// This provides:
// - bsp_api.h: Core BSP types (bsp_io_port_pin_t, bsp_io_level_t, etc.)
// - r_ioport_api.h: IOPORT HAL interface (IOPORT_CFG_*, R_IOPORT_*, etc.)
// - r_ioport.h: IOPORT driver implementation
// - variant.h: PinMuxCfg_t type and g_pin_cfg[] array declaration
#include <Arduino.h>

namespace fl {
namespace platform {

// ============================================================================
// Pin Mapping Helper
// ============================================================================

/// Get BSP pin identifier from Arduino pin number
/// @param pin Arduino pin number
/// @return BSP pin identifier (bsp_io_port_pin_t)
/// @note Uses g_pin_cfg[] declared in Arduino.h (from variant.h)
inline bsp_io_port_pin_t getBspPin(int pin) {
    // g_pin_cfg[] is declared in Arduino.h as extern const PinMuxCfg_t g_pin_cfg[];
    // PinMuxCfg_t contains: bsp_io_port_pin_t pin; const uint16_t* list;
    return g_pin_cfg[pin].pin;
}

// ============================================================================
// GPIO Functions - Native Renesas BSP Implementation
// ============================================================================

/// Set pin mode (input, output, input_pullup, input_pulldown)
/// @param pin Arduino pin number
/// @param mode Pin mode (PinMode enum)
inline void pinMode(int pin, PinMode mode) {
    bsp_io_port_pin_t bsp_pin = getBspPin(pin);
    uint32_t cfg = 0;

    // Map fl::PinMode to Renesas IOPORT configuration flags
    // PinMode: Input=0, Output=1, InputPullup=2, InputPulldown=3
    switch (mode) {
        case PinMode::Input:
            // Standard input: direction=input, no pull resistor
            cfg = IOPORT_CFG_PORT_DIRECTION_INPUT;
            break;

        case PinMode::Output:
            // Standard output: direction=output, push-pull
            cfg = IOPORT_CFG_PORT_DIRECTION_OUTPUT;
            break;

        case PinMode::InputPullup:
            // Input with pull-up resistor enabled
            cfg = IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE;
            break;

        case PinMode::InputPulldown:
            // Input with pull-down resistor
            // Note: Not all Renesas RA pins support pull-down (check datasheet)
            // This uses a workaround: configure as input with pull-up disabled
            // True pull-down would require PMOS control on supported pins
            cfg = IOPORT_CFG_PORT_DIRECTION_INPUT;
            break;

        default:
            // Unknown mode, default to input for safety
            cfg = IOPORT_CFG_PORT_DIRECTION_INPUT;
            break;
    }

    // Configure pin using BSP IOPORT driver
    // R_IOPORT_PinCfg(ctrl, pin, cfg) atomically updates pin configuration
    R_IOPORT_PinCfg(NULL, bsp_pin, cfg);
}

/// Write digital output value
/// @param pin Arduino pin number
/// @param val Output value (PinValue enum)
inline void digitalWrite(int pin, PinValue val) {
    bsp_io_port_pin_t bsp_pin = getBspPin(pin);

    // Convert fl::PinValue to BSP level
    // PinValue: Low=0, High=1
    bsp_io_level_t level = (val == PinValue::High) ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW;

    // Write pin state using BSP IOPORT driver
    // R_IOPORT_PinWrite() uses PCNTR3 register for atomic operation
    R_IOPORT_PinWrite(NULL, bsp_pin, level);
}

/// Read digital input value
/// @param pin Arduino pin number
/// @return Pin value (PinValue enum: Low or High)
inline PinValue digitalRead(int pin) {
    bsp_io_port_pin_t bsp_pin = getBspPin(pin);
    bsp_io_level_t level;

    // Read pin state using BSP IOPORT driver
    R_IOPORT_PinRead(NULL, bsp_pin, &level);

    // Convert BSP level to fl::PinValue
    return (level == BSP_IO_LEVEL_HIGH) ? PinValue::High : PinValue::Low;
}

/// Read analog input value
/// @param pin Arduino pin number
/// @return ADC value (0-1023 for 10-bit, 0-4095 for 12-bit)
/// @note STUB: Real implementation requires ADC peripheral configuration
inline uint16_t analogRead(int /*pin*/) {
    // STUB: ADC implementation requires:
    // 1. Map Arduino pin to ADC channel (via g_pin_cfg or pinmap)
    // 2. Initialize ADC peripheral (R_ADC_Open, R_ADC_ScanCfg)
    // 3. Start conversion (R_ADC_ScanStart)
    // 4. Wait for completion (R_ADC_StatusGet)
    // 5. Read result (R_ADC_Read)
    //
    // This is complex enough to warrant a separate ADC driver module.
    // For now, return 0 as stub.
    return 0;
}

/// Write analog output value (PWM)
/// @param pin Arduino pin number
/// @param val PWM duty cycle (0-255)
/// @note STUB: Real implementation requires GPT timer configuration
inline void analogWrite(int /*pin*/, uint16_t /*val*/) {
    // STUB: PWM implementation requires:
    // 1. Map Arduino pin to GPT channel (via g_pin_cfg or pinmap)
    // 2. Initialize GPT peripheral (R_GPT_Open, R_GPT_PeriodSet)
    // 3. Set duty cycle (R_GPT_DutyCycleSet)
    // 4. Start PWM output (R_GPT_Start)
    //
    // This is complex enough to warrant a separate PWM driver module.
    // For now, no-op as stub.
}

/// Set PWM duty cycle with 16-bit resolution
/// @param pin Arduino pin number
/// @param val PWM duty cycle (0-65535)
/// @note STUB: Real implementation requires GPT timer configuration
inline void setPwm16(int pin, uint16_t val) {
    // STUB: 16-bit PWM would use same GPT configuration as analogWrite
    // but with 16-bit period and compare registers
    // For now, scale to 8-bit and use analogWrite stub
    analogWrite(pin, val >> 8);
}

/// Set ADC voltage range
/// @param range ADC voltage range (AdcRange enum)
/// @note STUB: Real implementation requires ADC VREFCTL register configuration
inline void setAdcRange(AdcRange /*range*/) {
    // STUB: Analog reference configuration requires:
    // 1. Access to ADC peripheral
    // 2. Configure ADC VREFCTL register (if supported)
    // 3. Reference options on RA4M1:
    //    - AVCC0 (default, typically 3.3V or 5V)
    //    - Internal reference (1.0V typical)
    //    - External VREFH0/VREFL0 pins
    //
    // Most RA4M1 variants use AVCC0 as fixed reference.
    // For now, no-op as stub.
}

}  // namespace platform
}  // namespace fl

#endif  // ARDUINO_ARCH_RENESAS || FL_IS_RENESAS
