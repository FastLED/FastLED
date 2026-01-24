#pragma once

/// @file init_spi_hw.h
/// @brief ESP32 platform SPI hardware initialization
///
/// This header provides the unified initialization entry point for ESP32 SPI hardware.
/// Replaces the old per-lane headers (init_spi_hw_1.h, init_spi_hw_16.h) with a single
/// initialization function that handles all SPI hardware variants.

// allow-include-after-namespace

#include "platforms/esp/is_esp.h"

#ifdef FL_IS_ESP32

// Include the manager implementation
#include "platforms/esp/32/drivers/spi_hw_manager_esp32.cpp.hpp"

#else

// For non-ESP32 platforms, use the default no-op implementation
namespace fl {
namespace platform {

/// @brief No-op SPI hardware initialization for non-ESP32 platforms
inline void initSpiHardware() {
    // No-op: This platform doesn't have ESP32 SPI hardware
}

}  // namespace platform
}  // namespace fl

#endif  // FL_IS_ESP32
