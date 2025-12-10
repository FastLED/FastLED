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
// - fastspi_esp32_idf.h - ESP-IDF stub (future: native SPI using driver/spi_master.h)

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
#if defined(ARDUINO)
    #include "fastspi_esp32_arduino.h"
#else
    // Pure ESP-IDF build - use stub implementation (future: native SPI)
    #include "fastspi_esp32_idf.h"
#endif