// ok no namespace fl
#pragma once

/// @file led_sysdefs_stm32duino.h
/// LED system definitions for STM32duino (Official STMicroelectronics Core)
/// Modern, actively maintained core

#include "platforms/is_platform.h"
#include "fl/stl/stdint.h"

#ifndef FL_IS_STM32_STMDUINO
#error "This header is only for STM32duino (ARDUINO_ARCH_STM32) core"
#endif

// Interrupt control is provided by interrupts_stm32.h (included by led_sysdefs_arm_stm32.h)

// ============================================================================
// CPU Frequency
// ============================================================================
// STM32duino defines F_CPU as SystemCoreClock (runtime variable).
// All timing calculations use F_CPU directly - it handles runtime vs compile-time
// frequency automatically depending on the core.

