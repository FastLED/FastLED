#pragma once

// Platform detection and capability macros for Quad-SPI support
// Determines which platforms support hardware Quad-SPI for parallel LED control
//
// This file does coarse platform detection and delegates to platform-specific headers
// for fine-grained detection and configuration.

namespace fl {

// Coarse platform detection - delegate to platform-specific headers
#if defined(FASTLED_TESTING)
    // Testing mode: Use stub platform with mock driver
    #include "platforms/stub/quad_spi_platform_stub.h"

#elif defined(ESP32)
    // ESP32 family: Delegate to ESP-specific platform header
    #include "platforms/esp/quad_spi_platform_esp.h"

#else
    // No hardware Quad-SPI support
    #define FASTLED_HAS_QUAD_SPI 0
    #define FASTLED_QUAD_SPI_MAX_LANES 0
    #define FASTLED_QUAD_SPI_NUM_BUSES 0
#endif

// Helper macros
#ifndef FASTLED_HAS_DUAL_SPI
#define FASTLED_HAS_DUAL_SPI 0
#endif

#ifndef FASTLED_HAS_OCTAL_SPI
#define FASTLED_HAS_OCTAL_SPI 0
#endif

#define FASTLED_HAS_HARDWARE_SPI (FASTLED_HAS_QUAD_SPI || FASTLED_HAS_DUAL_SPI || FASTLED_HAS_OCTAL_SPI)

// Maximum total parallel strips (lanes × buses)
#if FASTLED_HAS_HARDWARE_SPI
#define FASTLED_QUAD_SPI_MAX_TOTAL_LANES (FASTLED_QUAD_SPI_MAX_LANES * FASTLED_QUAD_SPI_NUM_BUSES)
#else
#define FASTLED_QUAD_SPI_MAX_TOTAL_LANES 0
#endif

}  // namespace fl
