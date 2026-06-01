// ok no namespace fl
#pragma once

// IWYU pragma: private

/// @file led_sysdefs_stm32_zephyr.h
/// LED system definitions for ArduinoCore-zephyr STM32 boards.

#include "platforms/is_platform.h"

#ifndef FL_IS_STM32_ZEPHYR
#error "This header is only for ArduinoCore-zephyr STM32 boards"
#endif

#ifndef F_CPU
  #if defined(CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC)
    #define F_CPU CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC
  #else
    #error "CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC must be defined for STM32 Zephyr targets"
  #endif
#endif
