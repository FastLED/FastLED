#pragma once

/// @file init_channel_engine.h
/// @brief ESP32-specific channel engine initialization
///
/// This header declares the platform-specific function to initialize channel engines
/// for ESP32. It is called lazily on first access to ChannelBusManager.

namespace fl {
namespace platform {

/// @brief Initialize channel engines for ESP32
///
/// Registers platform-specific engines (PARLIO, SPI, RMT) with the ChannelBusManager.
/// This function is called lazily on first access to the ChannelBusManager singleton.
///
/// @note Implementation is in src/platforms/esp/32/drivers/channel_bus_manager_esp32.cpp
void initChannelEngines();

}  // namespace platform
}  // namespace fl
