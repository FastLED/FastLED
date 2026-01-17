#pragma once
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

#if defined(ARDUINO) && !defined(FL_NO_ARDUINO)
    // Arduino framework build - use Arduino SPI implementation
    #include "fastspi_esp32_arduino.h"
#else
    // Pure ESP-IDF build or NO_ARDUINO override - use native SPI
    #include "fastspi_esp32_idf.h"
#endif
