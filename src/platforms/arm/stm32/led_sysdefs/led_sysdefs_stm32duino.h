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
// STM32duino defines F_CPU as SystemCoreClock (runtime variable, not compile-time constant)
// FastLED requires compile-time constant for timing calculations
// We MUST #undef F_CPU before redefining it to override the framework's definition
//
// FL_CPU_DYNAMIC provides access to the actual runtime clock value (SystemCoreClock)
// This is useful for runtime clock frequency queries and debugging
// IMPORTANT: We define FL_CPU_DYNAMIC directly as SystemCoreClock, NOT as F_CPU,
// because F_CPU may be overridden by the build system with a constant value

// Forward declare SystemCoreClock (defined by CMSIS in system_stm32*.c)
extern "C" uint32_t SystemCoreClock;
#define FL_CPU_DYNAMIC SystemCoreClock

#if defined(STM32F1)
// STM32F1 family - typically 72 MHz
// F_CPU is already defined on stm32duino, but it's not constant
#undef F_CPU
#define F_CPU 72000000

#elif defined(STM32F4)
// STM32F4 F_CPU Issue (GitHub issue #2163 & #612):
// - STM32duino defines F_CPU as SystemCoreClock (runtime variable, not compile-time constant)
// - FastLED requires compile-time constant for timing calculations (preprocessor expressions)
// - SOLUTION: Override F_CPU with conservative fallback, users can customize via build_opt.h
//
// Example build_opt.h content (place in sketch directory):
//   -UF_CPU
//   -DF_CPU=168000000UL  // STM32F407 Discovery (168 MHz)
//   -DF_CPU=100000000UL  // STM32F411 Black Pill (100 MHz)
//   -DF_CPU=84000000UL   // STM32F401 (84 MHz)
//   -DF_CPU=180000000UL  // STM32F429/F446 (180 MHz)
//
// PlatformIO equivalent in platformio.ini:
//   build_flags = -UF_CPU -DF_CPU=168000000UL
//
// Why this is critical:
// - Incorrect F_CPU breaks WS2812/SK6812 LED timing (see issue #2163)
// - STM32F407 @ 168 MHz with F_CPU=100MHz -> timing 68% too fast -> protocol violations
// - STM32F401 @ 84 MHz with F_CPU=100MHz -> timing 16% too slow -> marginal operation
//
// Common STM32F4 speeds ([STM32F4 series](https://www.st.com/stm32f4)):
// - STM32F401: 84 MHz
// - STM32F411: 100 MHz
// - STM32F407: 168 MHz
// - STM32F429/F446: 180 MHz

// Force override F_CPU (STM32duino's SystemCoreClock is not a compile-time constant)
#undef F_CPU
#define F_CPU 100000000UL
#warning "STM32F4: F_CPU set to 100 MHz fallback. LED timing may be incorrect for other clock speeds!"
#warning "For accurate timing: Add build_opt.h with: -UF_CPU -DF_CPU=<your_board_mhz>UL"
#warning "Or in platformio.ini: build_flags = -UF_CPU -DF_CPU=<your_board_mhz>UL"

#else
// Other STM32 families - conservative fallback
#ifndef F_CPU
#define F_CPU 72000000
#endif
#endif
