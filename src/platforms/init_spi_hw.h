#pragma once

/// @file init_spi_hw.h
/// @brief Platform dispatch header for SPI hardware initialization
///
/// This dispatch header routes to platform-specific SPI hardware initialization.
/// Follows FastLED's coarse-to-fine delegation pattern (see platforms/README.md).
///
/// Replaces the old per-lane dispatch headers:
/// - init_spi_hw_1.h
/// - init_spi_hw_2.h
/// - init_spi_hw_4.h
/// - init_spi_hw_8.h
/// - init_spi_hw_16.h

// ok no namespace fl - Platform dispatch header only

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
    #include "platforms/stub/init_spi_hw.h"
#elif defined(ESP32)
    #include "platforms/esp/init_spi_hw.h"
#elif defined(FL_IS_ARM)
    #include "platforms/arm/init_spi_hw.h"
#else
    // Default no-op implementation for platforms without SPI hardware
    #include "platforms/shared/init_spi_hw.h"
#endif
