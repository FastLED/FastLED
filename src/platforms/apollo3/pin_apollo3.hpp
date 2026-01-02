#pragma once
// ok no namespace fl

/// @file platforms/apollo3/pin_apollo3.hpp
/// Apollo3 (SparkFun RedBoard Artemis, etc.) pin implementation (header-only)
///
/// Provides native Apollo3 HAL-based GPIO functions using am_hal_gpio_* APIs.
/// Used for all Apollo3 builds (Arduino and non-Arduino).
///
/// IMPORTANT: All functions use fl::PinMode, fl::PinValue, fl::AdcRange enum classes.

#include "pin_apollo3_native.hpp"
