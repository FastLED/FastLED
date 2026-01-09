// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/d51/init_samd51.h
/// @brief SAMD51 platform initialization
///
/// SAMD51 platforms (Cortex-M4F, Adafruit Metro M4, etc.) support dual-lane
/// and quad-lane SPI for high-speed LED output. This initialization ensures
/// the SPI hardware controllers are initialized early via weak linkage pattern.

namespace fl {
namespace platforms {

/// @brief Initialize SAMD51 platform
///
/// Performs one-time initialization of SAMD51-specific subsystems:
/// - SPI Hardware Controllers: Dual-lane and quad-lane SPI support (2/4 lanes)
/// - Weak linkage initialization: Triggers static vector population
///
/// SAMD51 platforms use weak linkage to register SPI hardware controllers.
/// Calling getAll() on each SPI lane count triggers this registration early,
/// ensuring consistent behavior across strip instantiation order.
///
/// This function is called once during FastLED::init() and is safe to call
/// multiple times (subsequent calls are no-ops).
///
/// @note Implementation is in src/platforms/arm/d51/init_samd51.cpp
void init();

} // namespace platforms
} // namespace fl
