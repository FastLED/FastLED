// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/nrf52/init_nrf52.h
/// @brief nRF52 platform initialization
///
/// nRF52 platforms (Cortex-M4F, Adafruit Feather nRF52840, etc.) support
/// dual-lane and quad-lane SPI for LED output. This initialization ensures
/// the SPI hardware controllers are initialized early via weak linkage pattern.

namespace fl {
namespace platforms {

/// @brief Initialize nRF52 platform
///
/// Performs one-time initialization of nRF52-specific subsystems:
/// - SPI Hardware Controllers: Dual-lane and quad-lane SPI support (2/4 lanes)
/// - Weak linkage initialization: Triggers static vector population
///
/// nRF52 platforms use weak linkage to register SPI hardware controllers.
/// Calling getAll() on each SPI lane count triggers this registration early,
/// ensuring consistent behavior across strip instantiation order.
///
/// This function is called once during FastLED::init() and is safe to call
/// multiple times (subsequent calls are no-ops).
///
/// @note Implementation is in src/platforms/arm/nrf52/init_nrf52.cpp
void init();

} // namespace platforms
} // namespace fl
