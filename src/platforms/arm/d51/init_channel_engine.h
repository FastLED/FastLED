#pragma once

// IWYU pragma: private

/// @file init_channel_engine.h
/// @brief SAMD51-specific channel engine initialization
///
/// This header declares the platform-specific function to initialize channel engines
/// for SAMD51. It is called lazily on first access to ChannelBusManager.

namespace fl {
namespace platforms {

/// @brief Initialize channel engines for SAMD51
///
/// Registers platform-specific engines (SPI hardware) with the ChannelBusManager.
/// This function is called lazily on first access to the ChannelBusManager singleton.
///
/// @note Implementation is in src/platforms/arm/d51/init_channel_engine_samd51.cpp.hpp
void initChannelEngines();

}  // namespace platforms
}  // namespace fl
