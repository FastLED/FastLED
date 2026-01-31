// ok no namespace fl
#pragma once

/// @file progmem_stm32.h
/// PROGMEM trampoline for STM32 platforms
///
/// Routes to core-specific PROGMEM implementations:
/// - progmem_stm32_particle.h  - Particle Photon/Electron (pgm_read_dword not provided)
/// - progmem_stm32_libmaple.h  - Arduino_STM32/libmaple (pgm_read_dword not provided)
/// - null_progmem.h            - STM32duino (provides pgm_read_* macros)

#include "platforms/is_platform.h"

#if defined(FL_IS_STM32_PARTICLE)
    #include "platforms/arm/stm32/progmem/progmem_stm32_particle.h"
#elif defined(FL_IS_STM32_LIBMAPLE)
    #include "platforms/arm/stm32/progmem/progmem_stm32_libmaple.h"
#else
    // STM32duino provides pgm_read_* - use null_progmem for FL_* macros
    #include "platforms/null_progmem.h"
#endif
