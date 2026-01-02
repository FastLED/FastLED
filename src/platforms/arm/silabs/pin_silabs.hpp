#pragma once

/// @file platforms/arm/silabs/pin_silabs.hpp
/// Silicon Labs (MGM240/EFR32) pin implementation (header-only)
///
/// Provides native pin functions using Silicon Labs EMLIB GPIO API.
/// This implementation uses direct EMLIB calls without Arduino framework dependency.
///
/// IMPORTANT: Uses fl::PinMode, fl::PinValue, fl::AdcRange enum classes.

#if defined(ARDUINO_ARCH_SILABS)

// ============================================================================
// Native Silicon Labs EMLIB GPIO Implementation
// ============================================================================

#include "fl/pin.h"
#include "em_gpio.h"
#include "em_cmu.h"

namespace fl {
namespace platform {

// GPIO clock initialization - required for Silicon Labs devices
inline void _silabs_gpio_init() {
    static bool initialized = false;  // okay static in header
    if (!initialized) {
        CMU_ClockEnable(cmuClock_GPIO, true);
        initialized = true;
    }
}

// Pin mapping structure - converts Arduino pin number to (port, pin)
struct SilabsPinMapping {
    GPIO_Port_TypeDef port;
    uint8_t pin;
};

// Get GPIO port and pin for Arduino pin number
// This is a simplified implementation - real board definitions should provide accurate mappings
inline SilabsPinMapping getSilabsPinMapping(int pin) {
    // Simple heuristic mapping for common Silicon Labs boards
    // Most Arduino-compatible Silicon Labs boards follow a sequential port mapping
    // Port A: pins 0-15, Port B: pins 16-31, Port C: pins 32-47, Port D: pins 48-63

    if (pin < 0 || pin > 63) {
        // Invalid pin - return Port A, Pin 0 as safe default
        return {gpioPortA, 0};
    }

    if (pin < 16) {
        return {gpioPortA, static_cast<uint8_t>(pin)};
    } else if (pin < 32) {
        return {gpioPortB, static_cast<uint8_t>(pin - 16)};
    } else if (pin < 48) {
        return {gpioPortC, static_cast<uint8_t>(pin - 32)};
    } else {
        return {gpioPortD, static_cast<uint8_t>(pin - 48)};
    }
}

inline void pinMode(int pin, PinMode mode) {
    _silabs_gpio_init();

    SilabsPinMapping mapping = getSilabsPinMapping(pin);

    switch (mode) {
        case PinMode::Output:
            GPIO_PinModeSet(mapping.port, mapping.pin, gpioModePushPull, 0);
            break;

        case PinMode::Input:
            GPIO_PinModeSet(mapping.port, mapping.pin, gpioModeInput, 0);
            break;

        case PinMode::InputPullup:
            GPIO_PinModeSet(mapping.port, mapping.pin, gpioModeInputPull, 1);
            break;

        case PinMode::InputPulldown:
            GPIO_PinModeSet(mapping.port, mapping.pin, gpioModeInputPull, 0);
            break;

        default:
            // Unknown mode, default to input
            GPIO_PinModeSet(mapping.port, mapping.pin, gpioModeInput, 0);
            break;
    }
}

inline void digitalWrite(int pin, PinValue val) {
    SilabsPinMapping mapping = getSilabsPinMapping(pin);

    if (val == PinValue::High) {
        GPIO_PinOutSet(mapping.port, mapping.pin);
    } else {
        GPIO_PinOutClear(mapping.port, mapping.pin);
    }
}

inline PinValue digitalRead(int pin) {
    SilabsPinMapping mapping = getSilabsPinMapping(pin);

    unsigned int result = GPIO_PinInGet(mapping.port, mapping.pin);
    return result ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    // Silicon Labs ADC implementation would go here
    // For now, return 0 as placeholder - ADC requires more complex EMLIB setup
    // (ADC_Init, ADC_Start, ADC_DataSingleGet, etc.)
    (void)pin;  // Suppress unused parameter warning
    return 0;
}

inline void analogWrite(int pin, uint16_t val) {
    // Silicon Labs PWM/Timer implementation would go here
    // For now, no-op - PWM requires TIMER peripheral setup via EMLIB
    (void)pin;
    (void)val;
}

inline void setPwm16(int pin, uint16_t val) {
    // Silicon Labs 16-bit PWM implementation would go here
    // For now, no-op - PWM requires TIMER peripheral setup via EMLIB
    (void)pin;
    (void)val;
}

inline void setAdcRange(AdcRange range) {
    // Silicon Labs ADC reference configuration would go here
    // Requires ADC_Init with proper reference settings
    (void)range;
}

}  // namespace platform
}  // namespace fl

#endif  // ARDUINO_ARCH_SILABS
