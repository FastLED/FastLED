#pragma once

/// @file init_channel_engine.h
/// @brief RP2040/RP2350-specific channel engine initialization
///
/// This header declares the platform-specific function to initialize channel engines
/// for RP2040/RP2350. It is called lazily on first access to ChannelBusManager.

namespace fl {
namespace platforms {

/// @brief Initialize channel engines for RP2040/RP2350
///
/// Registers platform-specific engines (SPI hardware) with the ChannelBusManager.
/// This function is called lazily on first access to the ChannelBusManager singleton.
///
/// @note Implementation is in src/platforms/arm/rp/rpcommon/init_channel_engine_rp.cpp.hpp
void initChannelEngines();

}  // namespace platforms
}  // namespace fl
