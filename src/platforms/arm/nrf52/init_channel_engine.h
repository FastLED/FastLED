#pragma once

/// @file init_channel_engine.h
/// @brief NRF52-specific channel engine initialization
///
/// This header declares the platform-specific function to initialize channel engines
/// for NRF52. It is called lazily on first access to ChannelBusManager.

namespace fl {
namespace platforms {

/// @brief Initialize channel engines for NRF52
///
/// Registers platform-specific engines (SPI hardware) with the ChannelBusManager.
/// This function is called lazily on first access to the ChannelBusManager singleton.
///
/// @note Implementation is in src/platforms/arm/nrf52/init_channel_engine_nrf52.cpp.hpp
void initChannelEngines();

}  // namespace platforms
}  // namespace fl
