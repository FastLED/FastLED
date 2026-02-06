#pragma once

/// @file init_channel_engine.h
/// @brief STM32-specific channel engine initialization
///
/// This header declares the platform-specific function to initialize channel engines
/// for STM32. It is called lazily on first access to ChannelBusManager.

namespace fl {
namespace platforms {

/// @brief Initialize channel engines for STM32
///
/// Registers platform-specific engines (SPI hardware) with the ChannelBusManager.
/// This function is called lazily on first access to the ChannelBusManager singleton.
///
/// @note Implementation is in src/platforms/arm/stm32/init_channel_engine_stm32.cpp.hpp
void initChannelEngines();

}  // namespace platforms
}  // namespace fl
