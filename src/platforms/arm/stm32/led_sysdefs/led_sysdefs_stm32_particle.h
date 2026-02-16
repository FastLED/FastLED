// ok no namespace fl
#pragma once

// IWYU pragma: private

/// @file led_sysdefs_stm32_particle.h
/// LED system definitions for Particle (Photon/Electron) devices
/// Hardware: STM32F205RGY6 (STM32F2 family, 120 MHz)

#include "platforms/is_platform.h"

#ifndef FL_IS_STM32_PARTICLE
#error "This header is only for Particle devices (Photon/Electron)"
#endif

// IWYU pragma: begin_keep
#include <application.h>
// IWYU pragma: end_keep
#include "fl/stl/stdint.h"

// Interrupt control is provided by interrupts_stm32.h (included by led_sysdefs_arm_stm32.h)

// ============================================================================
// CPU Frequency
// ============================================================================
#if defined(FL_IS_STM32_F2)
#ifndef F_CPU
#define F_CPU 120000000
#endif
#else
#ifndef F_CPU
#define F_CPU 72000000
#endif
#endif

// ============================================================================
// Yield Function
// ============================================================================
// Photon doesn't provide yield - we need to define it
#if defined(FL_IS_STM32_F2)
#define FASTLED_NEEDS_YIELD
extern "C" void yield();
#endif

