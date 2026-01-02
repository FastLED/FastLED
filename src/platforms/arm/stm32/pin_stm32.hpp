#pragma once

/// @file platforms/arm/stm32/pin_stm32.hpp
/// STM32 pin implementation (header-only)
///
/// Provides STM32 HAL-based GPIO functions using native register manipulation.
/// Works for both Arduino and non-Arduino builds by using STM32duino core
/// pin mapping functions and STM32 HAL APIs directly.
///
/// IMPORTANT: All functions use fl::PinMode/fl::PinValue/fl::AdcRange enum classes.

// ok no namespace fl - namespace provided by pin_stm32_native.hpp
#include "pin_stm32_native.hpp"
