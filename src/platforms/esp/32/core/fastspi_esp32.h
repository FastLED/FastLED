#pragma once

// IWYU pragma: private
// ESP32 Hardware SPI Dispatch Header
//
// This file follows the FastLED dispatch header pattern (see src/platforms/README.md).
// It routes to the appropriate platform-specific implementation based on the build
// configuration, while keeping the interface visible to IDEs regardless of defines.
//
// Architecture:
// - fastspi_esp32.h (this file) - Dispatch/trampoline header
// - fastspi_esp32_arduino.h - Arduino framework SPI implementation
// - fastspi_esp32_idf.h - Native ESP-IDF SPI implementation (driver/spi_master.h)
//
// DEPRECATION NOTICE:
// FASTLED_ALL_PINS_HARDWARE_SPI is now deprecated on ESP32. Hardware SPI is enabled
// by default via GPIO matrix routing. The define is accepted for backwards compatibility
// but no longer required. Use FASTLED_FORCE_SOFTWARE_SPI to disable hardware SPI if needed.
//
// The ESP-IDF native backend (driver/spi_master.h) is the only ESP32 SPI
// implementation. It is present in every Arduino-ESP32 install (Arduino-ESP32
// bundles ESP-IDF), so there is no behavioral difference under Arduino vs.
// bare ESP-IDF builds. FASTLED_ESP32_SPI_ARDUINO=1 is a temporary escape
// hatch back to the retired Arduino SPIClass backend if a regression is
// found; file a bug at github.com/FastLED/FastLED/issues if you need it.

// ok no namespace fl

// When enabled, use the bulk transfer mode to speed up SPI writes and avoid
// lock contention.
#ifndef FASTLED_ESP32_SPI_BULK_TRANSFER
#define FASTLED_ESP32_SPI_BULK_TRANSFER 0
#endif

#ifndef FASTLED_ESP32_SPI_BULK_TRANSFER_SIZE
#define FASTLED_ESP32_SPI_BULK_TRANSFER_SIZE 64
#endif

// Dispatch to platform-specific implementation

#if defined(FASTLED_ESP32_SPI_ARDUINO) && FASTLED_ESP32_SPI_ARDUINO && defined(ARDUINO)
    // Opt-in escape hatch back to the Arduino SPIClass backend.
    // IWYU pragma: begin_keep
    #include "platforms/esp/32/core/fastspi_esp32_arduino.h"
    // IWYU pragma: end_keep
#else
    // Default: native ESP-IDF SPI (driver/spi_master.h). Used for both
    // Arduino-ESP32 (which bundles ESP-IDF) and bare ESP-IDF builds.
    // IWYU pragma: begin_keep
    #include "platforms/esp/32/core/fastspi_esp32_idf.h"
    // IWYU pragma: end_keep
#endif
