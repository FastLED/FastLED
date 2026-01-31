// ok no namespace fl
#pragma once

/// @file interrupts_stm32_particle.h
/// Interrupt control for Particle (Photon/Electron) devices
/// Hardware: STM32F205RGY6 (STM32F2 family, 120 MHz)
///
/// Interrupt control is provided by fl::interruptsDisable() and
/// fl::interruptsEnable() in fl/isr.h (implemented in isr_stm32.hpp)

#include "platforms/is_platform.h"

#ifndef FL_IS_STM32_PARTICLE
#error "This header is only for Particle devices (Photon/Electron)"
#endif

// Interrupt control: use fl::interruptsDisable() / fl::interruptsEnable()
