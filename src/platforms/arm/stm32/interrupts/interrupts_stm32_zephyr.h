// ok no namespace fl
#pragma once

// IWYU pragma: private

/// @file interrupts_stm32_zephyr.h
/// Interrupt control for ArduinoCore-zephyr STM32 boards.

#include "platforms/is_platform.h"

#ifndef FL_IS_STM32_ZEPHYR
#error "This header is only for ArduinoCore-zephyr STM32 boards"
#endif

#include "platforms/arm/stm32/interrupts_stm32_inline.h"
