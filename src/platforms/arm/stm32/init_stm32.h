// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/stm32/init_stm32.h
/// @brief STM32 platform initialization
///
/// STM32 platforms support multi-lane SPI (2/4/8 lanes) for high-speed LED output.
/// This initialization ensures the SPI hardware controllers are initialized early
/// via weak linkage pattern.

namespace fl {
namespace platforms {

/// @brief Initialize STM32 platform
///
/// Performs one-time initialization of STM32-specific subsystems:
/// - SPI Hardware Controllers: Multi-lane SPI support (1/2/4/8 lanes)
/// - Weak linkage initialization: Triggers static vector population
///
/// STM32 platforms use weak linkage to register SPI hardware controllers.
/// Calling getAll() on each SPI lane count triggers this registration early,
/// ensuring consistent behavior across strip instantiation order.
///
/// This function is called once during FastLED::init() and is safe to call
/// multiple times (subsequent calls are no-ops).
///
/// @note Implementation is in src/platforms/arm/stm32/init_stm32.cpp
void init();

} // namespace platforms
} // namespace fl
