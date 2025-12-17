#pragma once

/// @file chipsets/encoders/encoder_constants.h
/// @brief Constants for SPI chipset encoders

#include "fl/stl/stdint.h"

namespace fl {

/// @brief Number of bytes per RGB pixel (order-agnostic)
constexpr size_t BYTES_PER_PIXEL_RGB = 3;

/// @brief Number of bytes per RGBW pixel (order-agnostic)
constexpr size_t BYTES_PER_PIXEL_RGBW = 4;

} // namespace fl
