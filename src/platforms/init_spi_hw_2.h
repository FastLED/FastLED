#pragma once

/// @file init_spi_hw_2.h
/// @brief Platform dispatch header for SpiHw2 initialization
///
/// This dispatch header routes to platform-specific SpiHw2 initialization.
/// Follows FastLED's coarse-to-fine delegation pattern (see platforms/README.md).

// ok no namespace fl

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
    #include "platforms/stub/init_spi_hw_2.h"
#else
    #include "platforms/shared/init_spi_hw_2.h"
#endif
