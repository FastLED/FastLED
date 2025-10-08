#pragma once

// ESP32 family platform-specific Quad-SPI detection and configuration
// This header is included by platforms/esp/quad_spi_platform_esp.h for ESP32 variants

#include "fl/has_include.h"

// Pull in sdkconfig if present (defines CONFIG_IDF_TARGET_* macros)
#if FL_HAS_INCLUDE("sdkconfig.h")
#include "sdkconfig.h"
#endif

namespace fl {

// Full Quad-SPI support (4 parallel data lanes)
#if defined(ESP32) || defined(CONFIG_IDF_TARGET_ESP32)
    #define FASTLED_HAS_QUAD_SPI 1
    #define FASTLED_QUAD_SPI_MAX_LANES 4
    #define FASTLED_QUAD_SPI_NUM_BUSES 2
    #define FASTLED_QUAD_SPI_BUS_HSPI 2
    #define FASTLED_QUAD_SPI_BUS_VSPI 3

#elif defined(ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S2)
    #define FASTLED_HAS_QUAD_SPI 1
    #define FASTLED_QUAD_SPI_MAX_LANES 4
    #define FASTLED_QUAD_SPI_NUM_BUSES 2
    #define FASTLED_QUAD_SPI_BUS_2 2
    #define FASTLED_QUAD_SPI_BUS_3 3

#elif defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
    #define FASTLED_HAS_QUAD_SPI 1
    #define FASTLED_QUAD_SPI_MAX_LANES 4
    #define FASTLED_QUAD_SPI_NUM_BUSES 2
    #define FASTLED_QUAD_SPI_BUS_2 2
    #define FASTLED_QUAD_SPI_BUS_3 3

// Dual-SPI support (2 parallel data lanes)
#elif defined(ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    #define FASTLED_HAS_DUAL_SPI 1
    #define FASTLED_QUAD_SPI_MAX_LANES 2
    #define FASTLED_QUAD_SPI_NUM_BUSES 1
    #define FASTLED_QUAD_SPI_BUS_2 2

#elif defined(ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C2)
    #define FASTLED_HAS_DUAL_SPI 1
    #define FASTLED_QUAD_SPI_MAX_LANES 2
    #define FASTLED_QUAD_SPI_NUM_BUSES 1
    #define FASTLED_QUAD_SPI_BUS_2 2

#elif defined(ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C6)
    #define FASTLED_HAS_DUAL_SPI 1
    #define FASTLED_QUAD_SPI_MAX_LANES 2
    #define FASTLED_QUAD_SPI_NUM_BUSES 1
    #define FASTLED_QUAD_SPI_BUS_2 2

#elif defined(ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32H2)
    #define FASTLED_HAS_DUAL_SPI 1
    #define FASTLED_QUAD_SPI_MAX_LANES 2
    #define FASTLED_QUAD_SPI_NUM_BUSES 1
    #define FASTLED_QUAD_SPI_BUS_2 2

// Future: Octal-SPI support (8 parallel data lanes)
#elif defined(ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32P4)
    #define FASTLED_HAS_OCTAL_SPI 1
    #define FASTLED_QUAD_SPI_MAX_LANES 8
    #define FASTLED_QUAD_SPI_NUM_BUSES 2
    #define FASTLED_QUAD_SPI_BUS_2 2
    #define FASTLED_QUAD_SPI_BUS_3 3

#else
    #error "Unknown ESP32 variant for Quad-SPI support"
#endif

}  // namespace fl
