// ok no namespace fl
#pragma once

// IWYU pragma: private

/// @file interrupts_stm32.h
/// Interrupt control trampoline for STM32 platforms
///
/// Routes to core-specific interrupt implementations:
/// - interrupts_stm32_particle.h  - Particle Photon/Electron (STM32F2)
/// - interrupts_stm32_libmaple.h  - Arduino_STM32 (Roger Clark libmaple)
/// - interrupts_stm32duino.h      - Official STM32duino core
///
/// Interrupt control is provided by fl::interruptsDisable() and
/// fl::interruptsEnable() in fl/isr.h (implemented in isr_stm32.hpp)

#include "platforms/is_platform.h"

#if defined(FL_IS_STM32_PARTICLE)
    #include "platforms/arm/stm32/interrupts/interrupts_stm32_particle.h"
#elif defined(FL_IS_STM32_LIBMAPLE)
    #include "platforms/arm/stm32/interrupts/interrupts_stm32_libmaple.h"
#elif defined(FL_IS_STM32_STMDUINO)
    #include "platforms/arm/stm32/interrupts/interrupts_stm32duino.h"
#else
    #error "Unknown STM32 core - cannot configure interrupt control"
#endif
