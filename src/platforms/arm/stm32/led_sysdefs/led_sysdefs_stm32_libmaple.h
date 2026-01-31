// ok no namespace fl
#pragma once

/// @file led_sysdefs_stm32_libmaple.h
/// LED system definitions for Arduino_STM32 (Roger Clark libmaple core)
/// Legacy core - "break/fix level only", "adequate for hobby use"

#include "platforms/is_platform.h"

#ifndef FL_IS_STM32_LIBMAPLE
#error "This header is only for Arduino_STM32 (libmaple) core"
#endif

// Interrupt control is provided by interrupts_stm32.h (included by led_sysdefs_arm_stm32.h)

// ============================================================================
// CPU Frequency
// ============================================================================
#ifndef F_CPU
#define F_CPU 72000000
#endif

