#pragma once

// IWYU pragma: private

/// @file init_channel_engine.h
/// @brief Teensy 4.x-specific channel engine initialization
///
/// This header declares the platform-specific function to initialize channel engines
/// for Teensy 4.x (IMXRT1062). It is called lazily on first access to ChannelBusManager.

namespace fl {
namespace platforms {

/// @brief Initialize channel engines for Teensy 4.x
///
/// Registers platform-specific engines (SPI hardware) with the ChannelBusManager.
/// This function is called lazily on first access to the ChannelBusManager singleton.
///
/// @note Implementation is in src/platforms/arm/teensy/teensy4_common/init_channel_engine_mxrt1062.cpp.hpp
void initChannelEngines();

}  // namespace platforms
}  // namespace fl
