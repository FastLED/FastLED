#pragma once
#include "fl/stl/noexcept.h"

// IWYU pragma: private

/// @file init_channel_driver.h
/// @brief Teensy 4.x-specific channel driver initialization
///
/// This header declares the platform-specific function to initialize channel drivers
/// for Teensy 4.x (IMXRT1062). It is called lazily on first access to ChannelManager.

namespace fl {
namespace platforms {

/// @brief Initialize channel drivers for Teensy 4.x
///
/// Registers platform-specific drivers (SPI hardware) with the ChannelManager.
/// This function is called lazily on first access to the ChannelManager singleton.
///
/// @note Implementation is in src/platforms/arm/teensy/teensy4_common/init_channel_driver_mxrt1062.cpp.hpp
void initChannelDrivers() FL_NOEXCEPT;

}  // namespace platforms
}  // namespace fl
