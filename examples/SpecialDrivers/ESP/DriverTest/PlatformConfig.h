// @filter: (platform is esp32)
/// @file PlatformConfig.h
/// @brief ESP32 Platform Detection and Configuration
///
/// This header provides:
/// - Automatic platform detection for all ESP32 variants
/// - Expected driver lists for each platform
/// - Configuration constants (LED count, pins, etc.)
///
/// Supported Platforms:
/// ┌─────────────┬─────────────────────────────────────────────┐
/// │ Platform    │ Available Drivers                           │
/// ├─────────────┼─────────────────────────────────────────────┤
/// │ ESP32       │ SPI, RMT, UART                              │
/// │ ESP32-S3    │ SPI, RMT, I2S, UART                         │
/// │ ESP32-C3    │ SPI, RMT, UART                              │
/// │ ESP32-C6    │ PARLIO, RMT, UART                           │
/// │ ESP32-C5    │ PARLIO, RMT, UART                           │
/// │ ESP32-H2    │ PARLIO, RMT, UART                           │
/// │ ESP32-P4    │ LCD_RGB, PARLIO, RMT, UART                  │
/// └─────────────┴─────────────────────────────────────────────┘

#pragma once

#include <FastLED.h>

// ============================================================================
// USER CONFIGURATION - Modify these settings for your setup
// ============================================================================

/// Number of LEDs in your strip
#define NUM_LEDS 60

/// Data pin connected to LED strip
/// Common pins by board:
///   - ESP32 DevKit: GPIO 2, 4, 5, 18, 19, 21, 22, 23
///   - ESP32-S3: GPIO 1-21 (avoid 0, 26-32 for strapping)
///   - ESP32-C6: GPIO 0-23
#define DATA_PIN 2

/// Brightness level (0-255), 64 = 25% for safe testing
#define TEST_BRIGHTNESS 64

// ============================================================================
// PLATFORM DETECTION
// ============================================================================

/// @brief Get a human-readable name for the current platform
/// @return String with platform name
inline const char* getPlatformName() {
#if defined(FL_IS_ESP_32C6)
    return "ESP32-C6";
#elif defined(FL_IS_ESP_32S3)
    return "ESP32-S3";
#elif defined(FL_IS_ESP_32C3)
    return "ESP32-C3";
#elif defined(FL_IS_ESP_32C5)
    return "ESP32-C5";
#elif defined(FL_IS_ESP_32H2)
    return "ESP32-H2";
#elif defined(FL_IS_ESP_32P4)
    return "ESP32-P4";
#elif defined(FL_IS_ESP_32DEV)
    return "ESP32 (classic)";
#else
    return "Unknown ESP32";
#endif
}

// ============================================================================
// EXPECTED DRIVERS PER PLATFORM
// ============================================================================

/// @brief Populate a vector with the expected drivers for this platform
/// @param expected Vector to populate with driver names
///
/// Each ESP32 variant has different peripheral support:
/// - PARLIO: Parallel I/O (ESP32-C6, C5, H2, P4 only)
/// - RMT: Remote Control peripheral (all ESP32 variants)
/// - SPI: Serial Peripheral Interface (most variants except C6)
/// - I2S: Inter-IC Sound peripheral (ESP32-S3 only)
/// - UART: Universal Async Receiver/Transmitter (all variants)
/// - LCD_RGB: LCD RGB interface (ESP32-P4 only)
inline void getExpectedDrivers(fl::vector<const char*>& expected) {
#if defined(FL_IS_ESP_32C6)
    // ESP32-C6: PARLIO (priority 4), RMT (priority 1), UART (priority 0)
    // Note: SPI disabled on C6 - only 1 SPI host available, RMT5 preferred
    expected.push_back("PARLIO");
    expected.push_back("RMT");
    expected.push_back("UART");

#elif defined(FL_IS_ESP_32S3)
    // ESP32-S3: SPI (priority 2), RMT (priority 1), I2S (priority -1), UART (priority 0)
    expected.push_back("SPI");
    expected.push_back("RMT");
    expected.push_back("I2S");
    expected.push_back("UART");

#elif defined(FL_IS_ESP_32C3)
    // ESP32-C3: SPI (priority 2), RMT (priority 1), UART (priority 0)
    expected.push_back("SPI");
    expected.push_back("RMT");
    expected.push_back("UART");

#elif defined(FL_IS_ESP_32C5)
    // ESP32-C5: PARLIO (priority 4), RMT (priority 1), UART (priority 0)
    expected.push_back("PARLIO");
    expected.push_back("RMT");
    expected.push_back("UART");

#elif defined(FL_IS_ESP_32H2)
    // ESP32-H2: PARLIO (priority 4), RMT (priority 1), UART (priority 0)
    expected.push_back("PARLIO");
    expected.push_back("RMT");
    expected.push_back("UART");

#elif defined(FL_IS_ESP_32P4)
    // ESP32-P4: LCD_RGB (priority 3), PARLIO (priority 4), RMT (priority 1), UART (priority 0)
    expected.push_back("LCD_RGB");
    expected.push_back("PARLIO");
    expected.push_back("RMT");
    expected.push_back("UART");

#elif defined(FL_IS_ESP_32DEV)
    // ESP32 (classic): SPI (priority 2), RMT (priority 1), UART (priority 0)
    expected.push_back("SPI");
    expected.push_back("RMT");
    expected.push_back("UART");

#else
    // Unknown platform - no expectations set
    (void)expected;  // Suppress unused parameter warning
#endif
}
