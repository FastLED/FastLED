#pragma once

// STM32F4 family variant using unified template
// STM32F4 does NOT have BRR register (removed in F2/F4/F7/H7 families)
//
// KEY DIFFERENCE FROM F1:
// - STM32F1/F0/F3/L0/L4/G0/G4: Have separate BRR register at offset 0x28
// - STM32F2/F4/F7/H7: BRR register does NOT exist, use BSRR upper 16 bits
//
// BOARD-SPECIFIC PIN MAPPINGS:
// - FastPin templates are indexed by Arduino digital pin NUMBERS (0, 1, 2, ...), not pin NAMES (PA0, PB0)
// - STM32duino defines PA0, PB0 as macros that expand to Arduino pin numbers (board-specific)
// - Board mapping files in boards/f4/ define the Arduino pin → GPIO mapping
//
// References:
// - https://www.eevblog.com/forum/microcontrollers/bsrr-in-stm32f4xx-h/
// - https://community.st.com/t5/stm32-mcus-products/rm0385-has-references-to-nonexistent-gpiox-brr-register/td-p/138531

#include "platforms/arm/stm32/pins/core/armpin_template.h"
#include "platforms/arm/stm32/pins/core/gpio_port_init.h"
#include "platforms/arm/stm32/pins/core/pin_macros.h"

namespace fl {

// F4 family convenience macro: HAS_BRR = false (no BRR register)
#define _DEFPIN_ARM_F4(PIN, BIT, PORT) _DEFPIN_STM32(PIN, BIT, PORT, false)

// Initialize GPIO ports based on what's available in hardware
// All STM32F4 variants have at least ports A-E
_STM32_INIT_PORT(A);
_STM32_INIT_PORT(B);
_STM32_INIT_PORT(C);
_STM32_INIT_PORT(D);
_STM32_INIT_PORT(E);

// Optional ports (not all STM32F4 variants have these)
#if defined(GPIOF)
  _STM32_INIT_PORT(F);
#endif
#if defined(GPIOG)
  _STM32_INIT_PORT(G);
#endif
#if defined(GPIOH)
  _STM32_INIT_PORT(H);
#endif
#if defined(GPIOI)
  _STM32_INIT_PORT(I);
#endif
#if defined(GPIOJ)
  _STM32_INIT_PORT(J);
#endif
#if defined(GPIOK)
  _STM32_INIT_PORT(K);
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
