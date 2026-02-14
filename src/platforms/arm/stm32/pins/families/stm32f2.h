#pragma once

// IWYU pragma: private

// STM32F2 family variant using unified template
// STM32F2 does NOT have BRR register (removed in F2/F4/F7/H7 families)
//
// KEY DIFFERENCE FROM F1:
// - STM32F1/F0/F3/L0/L4/G0/G4: Have separate BRR register at offset 0x28
// - STM32F2/F4/F7/H7: BRR register does NOT exist, use BSRR upper 16 bits
//
// BOARDS:
// - Spark Core (STM32F103-based, but uses F2-style registers)
// - Particle Photon (STM32F205)

#include "platforms/arm/stm32/pins/core/armpin_template.h"
#include "platforms/arm/stm32/pins/core/gpio_port_init.h"
#include "platforms/arm/stm32/pins/core/pin_macros.h"

namespace fl {

// F2 family convenience macro: HAS_BRR = false (no BRR register)
#define _DEFPIN_ARM_F2(PIN, BIT, PORT) _DEFPIN_STM32(PIN, BIT, PORT, false)

// Initialize GPIO ports based on what's available in hardware
// STM32F2 variants typically have ports A-G
_STM32_INIT_PORT(A);
_STM32_INIT_PORT(B);
_STM32_INIT_PORT(C);
_STM32_INIT_PORT(D);
_STM32_INIT_PORT(E);

// Optional ports (not all F2 variants have these)
#if defined(GPIOF)
  _STM32_INIT_PORT(F);
#endif
#if defined(GPIOG)
  _STM32_INIT_PORT(G);
#endif
#if defined(GPIOH)
  _STM32_INIT_PORT(H);
#endif

// Include board-specific pin mappings
// Board file is set by fastpin_dispatcher.h via FASTLED_STM32_BOARD_FILE macro
#ifdef FASTLED_STM32_BOARD_FILE
  #include FASTLED_STM32_BOARD_FILE  // nolint
#else
  #error "FASTLED_STM32_BOARD_FILE not defined - include this file via fastpin_dispatcher.h"
#endif

#define HAS_HARDWARE_PIN_SUPPORT

}  // namespace fl
