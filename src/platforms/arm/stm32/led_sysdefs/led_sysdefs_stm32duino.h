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
// This breaks template parameters that require compile-time constants (e.g., SPI data rates).
// We must #undef F_CPU and redefine it as a constant.
//
// FastLED's clockless driver uses SystemCoreClock at runtime for accurate LED timing,
// so F_CPU mismatches no longer affect LED protocol timing (GitHub issue #2163 resolved).

// Force F_CPU to be a compile-time constant (required for template parameters)
// STM32duino defines it as SystemCoreClock which is not constexpr
#undef F_CPU
#if defined(STM32F1)
#define F_CPU 72000000UL
#elif defined(STM32F4)
#define F_CPU 100000000UL
#else
#define F_CPU 72000000UL
#endif

