#pragma once
#include "fl/stl/noexcept.h"

// IWYU pragma: private

/// @file init_channel_driver.h
/// @brief STM32-specific channel driver initialization
///
/// This header declares the platform-specific function to initialize channel drivers
/// for STM32. It is called lazily on first access to ChannelManager.

namespace fl {
namespace platforms {

/// @brief Initialize channel drivers for STM32
///
/// Registers platform-specific drivers (SPI hardware) with the ChannelManager.
/// This function is called lazily on first access to the ChannelManager singleton.
///
/// @note Implementation is in src/platforms/arm/stm32/init_channel_driver_stm32.cpp.hpp
void initChannelDrivers() FL_NOEXCEPT;

}  // namespace platforms
}  // namespace fl
