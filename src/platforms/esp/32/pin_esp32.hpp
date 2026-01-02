#pragma once

/// @file platforms/esp/32/pin_esp32.hpp
/// ESP32 pin implementation using native ESP-IDF GPIO drivers
///
/// Provides zero-overhead wrappers for ESP32 pin functions using native
/// ESP-IDF HAL (Hardware Abstraction Layer) APIs directly. No Arduino
/// dependency required.
///
/// IMPORTANT: Translates fl::PinMode/fl::PinValue/fl::AdcRange enum classes
/// to platform-specific types.

// ok no namespace fl
#include "pin_esp32_native.hpp"
