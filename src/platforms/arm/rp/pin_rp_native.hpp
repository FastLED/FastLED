#pragma once

/// @file platforms/arm/rp/pin_rp_native.hpp
/// RP2040/RP2350 Pico SDK GPIO implementation
///
/// Provides Arduino-compatible pin functions using native Pico SDK GPIO APIs.
/// This file is used in non-Arduino builds (Pico SDK only).

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

namespace fl {

// Pin mode constants matching Arduino API
constexpr int INPUT = 0;
constexpr int OUTPUT = 1;
constexpr int INPUT_PULLUP = 2;
constexpr int INPUT_PULLDOWN = 3;

// Digital level constants
constexpr int LOW = 0;
constexpr int HIGH = 1;

// ADC reference mode (stub - RP2040 uses fixed 3.3V reference)
constexpr int DEFAULT = 0;

/// @brief Configure a GPIO pin mode
/// @param pin GPIO pin number (0-29 for RP2040, 0-47 for RP2350)
/// @param mode Pin mode: INPUT, OUTPUT, INPUT_PULLUP, or INPUT_PULLDOWN
inline void pinMode(int pin, int mode) {
    gpio_init(pin);

    switch (mode) {
        case OUTPUT:
            gpio_set_dir(pin, GPIO_OUT);
            break;
        case INPUT:
            gpio_set_dir(pin, GPIO_IN);
            gpio_pull_up(pin);   // Disable pull-up
            gpio_pull_down(pin); // Disable pull-down
            break;
        case INPUT_PULLUP:
            gpio_set_dir(pin, GPIO_IN);
            gpio_pull_up(pin);
            break;
        case INPUT_PULLDOWN:
            gpio_set_dir(pin, GPIO_IN);
            gpio_pull_down(pin);
            break;
        default:
            gpio_set_dir(pin, GPIO_IN);
            break;
    }
}

/// @brief Write a digital value to a GPIO pin
/// @param pin GPIO pin number
/// @param val Digital level: LOW (0) or HIGH (1)
inline void digitalWrite(int pin, int val) {
    gpio_put(pin, val != 0);
}

/// @brief Read a digital value from a GPIO pin
/// @param pin GPIO pin number
/// @return Digital level: LOW (0) or HIGH (1)
inline int digitalRead(int pin) {
    return gpio_get(pin) ? HIGH : LOW;
}

/// @brief Read an analog value from an ADC pin
/// @param pin ADC pin number (26-29 for RP2040 ADC0-ADC3, or 4 for temperature sensor)
/// @return 12-bit ADC value (0-4095)
///
/// RP2040/RP2350 has 5 ADC inputs:
/// - GPIO26 = ADC0
/// - GPIO27 = ADC1
/// - GPIO28 = ADC2
/// - GPIO29 = ADC3 (also VSYS/3 on Pico)
/// - ADC4 = Internal temperature sensor (virtual pin 4)
inline int analogRead(int pin) {
    static bool adc_initialized = false;

    // Initialize ADC hardware on first use
    if (!adc_initialized) {
        adc_init();
        adc_initialized = true;
    }

    // Map GPIO pin to ADC input channel
    int adc_channel = -1;
    if (pin >= 26 && pin <= 29) {
        // GPIO26-29 map to ADC0-3
        adc_channel = pin - 26;
        adc_gpio_init(pin);
    } else if (pin == 4) {
        // Virtual pin 4 = temperature sensor (ADC4)
        adc_channel = 4;
        // Temperature sensor doesn't need gpio_init
    } else {
        // Invalid ADC pin
        return 0;
    }

    // Select ADC input and read
    adc_select_input(adc_channel);
    return adc_read();
}

/// @brief Write an analog (PWM) value to a GPIO pin
/// @param pin GPIO pin number
/// @param val PWM duty cycle (0-255, mapped to 0-100% duty cycle)
///
/// Uses hardware PWM. Each PWM slice controls 2 pins (A/B channels).
/// PWM frequency is approximately 244 Hz (system clock / 65536).
inline void analogWrite(int pin, int val) {
    // Check if pin supports PWM
    if (pin >= NUM_BANK0_GPIOS) {
        return;  // Invalid pin for PWM
    }

    // Get PWM slice and channel for this GPIO
    uint slice = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);

    // Configure GPIO for PWM function
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Set PWM wrap value (period) to 255 for 8-bit resolution
    pwm_set_wrap(slice, 255);

    // Set duty cycle (0-255)
    pwm_set_chan_level(slice, channel, val);

    // Enable PWM on this slice
    pwm_set_enabled(slice, true);
}

/// @brief Set analog reference voltage (stub - not supported on RP2040/RP2350)
/// @param mode Reference mode (ignored - RP2040 uses fixed 3.3V reference)
///
/// RP2040/RP2350 ADC uses a fixed 3.3V reference voltage.
/// This function is provided for Arduino API compatibility but does nothing.
inline void analogReference(int /*mode*/) {
    // No-op: RP2040/RP2350 ADC uses fixed 3.3V reference
}

}  // namespace fl
