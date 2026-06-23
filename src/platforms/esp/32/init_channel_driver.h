#pragma once

// IWYU pragma: private

/// @file init_channel_driver.h
/// @brief ESP32-specific channel driver initialization
///
/// This header declares the platform-specific function to initialize channel drivers
/// for ESP32. It is called lazily on first access to ChannelManager.

#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

/// @brief Initialize channel drivers for ESP32
///
/// Registers platform-specific drivers (PARLIO, SPI, RMT) with the ChannelManager.
/// This function is called lazily on first access to the ChannelManager singleton.
///
/// @note Implementation is in src/platforms/esp/32/drivers/channel_manager_esp32.cpp
void initChannelDrivers() FL_NOEXCEPT;

}  // namespace platforms
}  // namespace fl
