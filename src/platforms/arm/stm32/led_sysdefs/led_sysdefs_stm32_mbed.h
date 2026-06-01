// ok no namespace fl
#pragma once

// IWYU pragma: private

/// @file led_sysdefs_stm32_mbed.h
/// LED system definitions for Arduino Mbed OS STM32 boards.

#include "platforms/is_platform.h"
#include "fl/stl/stdint.h"

#ifndef FL_IS_STM32_MBED
#error "This header is only for Arduino Mbed OS STM32 boards"
#endif

#ifndef F_CPU
#error "F_CPU must be defined for Arduino Mbed OS STM32 boards"
#endif
