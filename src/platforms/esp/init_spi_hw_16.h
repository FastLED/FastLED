#pragma once

/// @file init_spi_hw_16.h
/// @brief ESP32 platform SpiHw16 initialization
///
/// This header provides lazy initialization for ESP32 I2S-based 16-lane SPI hardware.
/// Only ESP32 and ESP32-S2 have I2S parallel mode that supports 16-lane output.

// allow-include-after-namespace

#include "platforms/esp/is_esp.h"

// The I2S parallel mode driver only works on ESP32 and ESP32-S2
#if defined(ESP32) && !defined(FL_IS_ESP_32S3) && !defined(FL_IS_ESP_32C2) && !defined(FL_IS_ESP_32C3) && !defined(FL_IS_ESP_32C5) && !defined(FL_IS_ESP_32C6) && !defined(FL_IS_ESP_32H2) && !defined(FL_IS_ESP_32P4)

namespace fl {
namespace platform {

/// @brief Initialize ESP32 I2S-based SpiHw16 instances
///
/// Registers ESP32 I2S parallel mode controller for 16-lane SPI output.
/// Implementation is in src/platforms/esp/32/drivers/i2s/spi_hw_i2s_esp32.cpp
void initSpiHw16Instances();

}  // namespace platform
}  // namespace fl

#elif !defined(ESP32) || defined(FL_IS_ESP_32S3) || defined(FL_IS_ESP_32C2) || defined(FL_IS_ESP_32C3) || defined(FL_IS_ESP_32C5) || defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32H2) || defined(FL_IS_ESP_32P4)

// For other ESP variants, use the default no-op implementation
#include "platforms/shared/init_spi_hw_16.h"

#endif  // ESP32 I2S check
