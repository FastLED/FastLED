/// @file lcd_driver_common.h
/// @brief Common structures and utilities for ESP32 LCD-based LED drivers
///
/// This file contains shared code between ESP32-S3 (LCD_CAM/I80) and ESP32-P4 (RGB LCD)
/// parallel LED drivers.

#pragma once

#include "fl/stdint.h"
#include "crgb.h"
// Data alignment for PSRAM transfers
#ifndef LCD_DRIVER_PSRAM_DATA_ALIGNMENT
#define LCD_DRIVER_PSRAM_DATA_ALIGNMENT 64
#endif

// Allow override for debugging/testing (not recommended for production)
#ifndef LCD_PCLK_HZ_OVERRIDE
#define LCD_PCLK_HZ_OVERRIDE 0
#endif

namespace fl {

/// @brief Configuration structure for LCD LED driver
struct LcdDriverConfig {
    int gpio_pins[16];           ///< GPIO numbers for data lanes D0-D15
    int num_lanes;               ///< Active lane count (1-16)
    int latch_us = 0;            ///< Reset gap duration (microseconds), 0 = use chipset default
    bool use_psram = true;       ///< Allocate DMA buffers in PSRAM
    uint32_t pclk_hz_override = LCD_PCLK_HZ_OVERRIDE;  ///< Optional: Force specific PCLK
};

/// @brief Pin validation result
struct PinValidationResult {
    bool valid;
    const char* error_message;
};

/// @brief Validate ESP32-S3 GPIO pin for LCD use
/// @param pin GPIO pin number to validate
/// @param allow_strapping If true, allow strapping pins with warning
/// @return Validation result with error message if invalid
inline PinValidationResult validate_esp32s3_lcd_pin(int pin, bool allow_strapping = false) {
    // Check for invalid USB-JTAG pins (ESP32-S2/S3)
    if (pin == 19 || pin == 20) {
        return {false, "GPIO19 and GPIO20 are reserved for USB-JTAG on ESP32-S2/S3 and CANNOT be used for LED output. "
                       "Using these pins WILL BREAK USB flashing capability. Please choose a different pin."};
    }

    // Check for Flash/PSRAM pins (GPIO26-32)
    if (pin >= 26 && pin <= 32) {
        return {false, "GPIO26-32 are reserved for SPI Flash/PSRAM and CANNOT be used for LED output. "
                       "Using these pins WILL BREAK flash/PSRAM functionality. Please choose a different pin."};
    }

    // Check for strapping pins (GPIO0, 3, 45, 46)
    if (pin == 0 || pin == 3 || pin == 45 || pin == 46) {
        if (!allow_strapping) {
            return {false, "GPIO is a strapping pin used for boot configuration. "
                           "Using this pin may affect boot behavior and requires careful external circuit design. "
                           "Define FASTLED_ESP32_ALLOW_STRAPPING_PINS to suppress this error if you know what you're doing."};
        }
        // Warning will be issued by caller
    }

#if defined(CONFIG_SPIRAM_MODE_OCT) || defined(CONFIG_ESPTOOLPY_FLASHMODE_OPI)
    // Check for Octal Flash/PSRAM pins
    if (pin >= 33 && pin <= 37) {
        return {false, "GPIO33-37 are reserved for Octal Flash/PSRAM (SPIIO4-7, SPIDQS) and CANNOT be used for LED output. "
                       "Using these pins WILL BREAK Octal flash/PSRAM functionality. Please choose a different pin."};
    }
#endif

    return {true, nullptr};
}

/// @brief Validate ESP32-P4 GPIO pin for LCD use
/// @param pin GPIO pin number to validate
/// @return Validation result with error message if invalid
inline PinValidationResult validate_esp32p4_lcd_pin(int pin) {
    // Reject invalid pin numbers
    if (pin < 0 || pin > 54) {
        return {false, "GPIO pin must be in range 0-54 for ESP32-P4"};
    }

    // Reject strapping pins (hard error)
    // GPIO34-38 are used for boot configuration and MUST NOT be used for LED output
    if (pin >= 34 && pin <= 38) {
        return {false, "GPIO34-38 are strapping pins and CANNOT be used for LED output. "
                       "Using these pins WILL PREVENT BOOT. Please choose a different pin."};
    }

    // Reject USB-JTAG pins (hard error)
    // GPIO24-25 are used for USB-JTAG debugging
    if (pin == 24 || pin == 25) {
        return {false, "GPIO24-25 are reserved for USB-JTAG on ESP32-P4. "
                       "Using these pins WILL DISABLE USB-JTAG. Please choose a different pin."};
    }

    // Note: Flash/PSRAM pins are sdkconfig-dependent and harder to detect at compile time
    // Users should consult their board documentation for Flash/PSRAM pin assignments
    return {true, nullptr};
}

}  // namespace fl
