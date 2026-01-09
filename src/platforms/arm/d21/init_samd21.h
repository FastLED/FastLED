// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/d21/init_samd21.h
/// @brief SAMD21 platform initialization
///
/// SAMD21 platforms (Cortex-M0+, Arduino Zero, etc.) support dual-lane SPI
/// for LED output. This initialization ensures the SPI hardware controllers
/// are initialized early via weak linkage pattern.

namespace fl {
namespace platforms {

/// @brief Initialize SAMD21 platform
///
/// Performs one-time initialization of SAMD21-specific subsystems:
/// - SPI Hardware Controllers: Dual-lane SPI support (2 lanes)
/// - Weak linkage initialization: Triggers static vector population
///
/// SAMD21 platforms use weak linkage to register SPI hardware controllers.
/// Calling getAll() triggers this registration early, ensuring consistent
/// behavior across strip instantiation order.
///
/// This function is called once during FastLED::init() and is safe to call
/// multiple times (subsequent calls are no-ops).
///
/// @note Implementation is in src/platforms/arm/d21/init_samd21.cpp
void init();

} // namespace platforms
} // namespace fl
