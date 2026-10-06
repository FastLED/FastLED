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
/// Registers only the platform-default ObjectFLED engine (#4708). See
/// registerAllTeensyChannelDrivers() for the full set.
///
/// @note Implementation is in src/platforms/arm/teensy/teensy4_common/init_channel_driver_mxrt1062.cpp.hpp
void initChannelDrivers() FL_NO_EXCEPT;

/// @brief Register FlexIO, ObjectFLED and unified SPI drivers.
///
/// Called from `fl::enableAllDrivers()` so runtime driver selection keeps
/// every Teensy 4.x driver available.
void registerAllTeensyChannelDrivers() FL_NO_EXCEPT;

}  // namespace platforms
}  // namespace fl
