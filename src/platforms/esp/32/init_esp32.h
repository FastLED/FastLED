// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/esp/32/init_esp32.h
/// @brief ESP32-specific platform initialization
///
/// This header provides ESP32-specific initialization functions that are called
/// once during FastLED::init(). It initializes the channel bus manager and
/// SPI system for ESP32 platforms.

namespace fl {
namespace platforms {

/// @brief Initialize ESP32 platform
///
/// Performs one-time initialization of ESP32-specific subsystems:
/// - Channel bus manager (PARLIO, SPI, RMT, UART engines)
/// - SPI bus manager (multi-lane SPI support)
///
/// This function is called once during FastLED::init() and is safe to call
/// multiple times (subsequent calls are no-ops).
///
/// @note Implementation is in src/platforms/esp/32/init_esp32.cpp
void init();

} // namespace platforms
} // namespace fl
