#pragma once

/// @file platforms/arm/nrf52/pin_nrf52.hpp
/// Nordic nRF52 (Adafruit Feather nRF52, etc.) pin implementation (header-only)
///
/// Provides zero-overhead wrappers for nRF52 pin functions.
///
/// Native path: Direct Nordic SDK register manipulation (no Arduino.h dependency)
///
/// IMPORTANT: Uses enum class types from fl/pin.h for type safety.

// ============================================================================
// Native nRF52 Path: Direct Nordic SDK register manipulation
// ============================================================================

#include "fl/compiler_control.h"

// Include Nordic SDK headers for native GPIO and peripheral access
FL_EXTERN_C_BEGIN
#include "nrf.h"
#include "nrf_gpio.h"
#ifdef NRF_SAADC
#include "nrf_saadc.h"
#endif
#ifdef NRF_PWM0
#include "nrf_pwm.h"
#endif
FL_EXTERN_C_END

namespace fl {
namespace platform {

inline void pinMode(int pin, PinMode mode) {
    switch (mode) {
        case PinMode::Input:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
            break;
        case PinMode::Output:
            nrf_gpio_cfg_output(pin);
            break;
        case PinMode::InputPullup:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLUP);
            break;
        case PinMode::InputPulldown:
            nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLDOWN);
            break;
    }
}

inline void digitalWrite(int pin, PinValue val) {
    if (val == PinValue::High) {
        nrf_gpio_pin_set(pin);
    } else {
        nrf_gpio_pin_clear(pin);
    }
}

inline PinValue digitalRead(int pin) {
    return nrf_gpio_pin_read(pin) ? PinValue::High : PinValue::Low;
}

inline uint16_t analogRead(int pin) {
    // SAADC (Successive Approximation ADC) requires complex initialization
    // For now, return 0 as a safe default - full implementation would require:
    // 1. SAADC channel configuration
    // 2. Buffer allocation
    // 3. Conversion trigger and wait
    // 4. Result reading
    // This is typically handled by Arduino core's analogRead() wrapper
    (void)pin;  // Suppress unused parameter warning
    return 0;
}

inline void analogWrite(int pin, uint16_t val) {
    // PWM on nRF52 requires complex PWM peripheral configuration
    // For now, this is a no-op - full implementation would require:
    // 1. PWM instance allocation
    // 2. Pin assignment to PWM channel
    // 3. Duty cycle configuration
    // 4. PWM start
    // This is typically handled by Arduino core's analogWrite() wrapper
    (void)pin;
    (void)val;
}

inline void setPwm16(int pin, uint16_t val) {
    // nRF52 PWM peripheral natively supports 15-bit resolution (0-32767)
    // For 16-bit input, scale down by 1 bit
    // However, without Arduino core support, we'd need direct PWM peripheral access
    (void)pin;
    (void)val;
}

inline void setAdcRange(AdcRange range) {
    // SAADC reference voltage configuration
    // nRF52 SAADC supports: 0.6V internal, VDD/4 (default)
    // Full implementation requires SAADC peripheral configuration
#ifdef NRF_SAADC
    switch (range) {
        case AdcRange::Default:
            // VDD/4 (default) - 0.825V with VDD=3.3V
            // SAADC->CONFIG.REFSEL = SAADC_CONFIG_REFSEL_VDD1_4;
            break;
        case AdcRange::Range0_1V1:
            // 0.6V internal reference (closest to 1.1V)
            // SAADC->CONFIG.REFSEL = SAADC_CONFIG_REFSEL_Internal;
            break;
        case AdcRange::Range0_3V3:
            // For 3.0V range, would need external reference or VDD
            // SAADC doesn't have exact 3.0V internal reference
            break;
        case AdcRange::Range0_1V5:
        case AdcRange::Range0_2V2:
        case AdcRange::Range0_5V:
        case AdcRange::External:
            // Not supported on nRF52 SAADC, use default
            break;
    }
#else
    (void)range;  // Suppress unused parameter warning if SAADC not available
#endif
}

}  // namespace platform
}  // namespace fl
