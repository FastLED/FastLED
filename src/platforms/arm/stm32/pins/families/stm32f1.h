#pragma once

// STM32F1 family variant using unified template
// STM32F1 HAS BRR register at offset 0x28 (unlike F2/F4/F7/H7)
//
// KEY DIFFERENCE FROM F4:
// - STM32F1/F0/F3/L0/L4/G0/G4: Have separate BRR register at offset 0x28
// - STM32F2/F4/F7/H7: BRR register does NOT exist, use BSRR upper 16 bits
//
// REGISTER MAPPING COMPATIBILITY:
// - Maple Mini: Uses gpio_reg_map* (->regs) from libmaple
// - Generic F1: Uses GPIO_TypeDef* from STM32duino HAL
// - Both styles are supported via conditional compilation

#include "platforms/arm/stm32/pins/core/armpin_template.h"
#include "platforms/arm/stm32/pins/core/gpio_port_init.h"
#include "platforms/arm/stm32/pins/core/pin_macros.h"

namespace fl {

// F1 family convenience macro: HAS_BRR = true (has BRR register)
#define _DEFPIN_ARM_F1(PIN, BIT, PORT) _DEFPIN_STM32(PIN, BIT, PORT, true)

// STM32F1 GPIO port initialization
// Two different register mapping styles for compatibility:
// 1. Maple Mini style: gpio_reg_map* (->regs)
// 2. Generic STM32F1: GPIO_TypeDef* (standard HAL)

#if defined(ARDUINO_MAPLE_MINI)
  // Maple Mini style register mapping (libmaple)
  #define _RD32_F1(T) \
    struct __gen_struct_ ## T { \
      static __attribute__((always_inline)) FASTLED_FORCE_INLINE volatile gpio_reg_map* r() { \
        return T->regs; \
      } \
    };
#else
  // Generic STM32F1 style register mapping (STM32duino HAL)
  #define _RD32_F1(T) \
    struct __gen_struct_ ## T { \
      static __attribute__((always_inline)) FASTLED_FORCE_INLINE volatile GPIO_TypeDef* r() { \
        return T; \
      } \
    };
#endif

#define _STM32_INIT_PORT_F1(L) _RD32_F1(GPIO ## L)

// Initialize GPIO ports based on what's available in hardware
// STM32F1 variants typically have ports A, B, C, D
_STM32_INIT_PORT_F1(A);
_STM32_INIT_PORT_F1(B);
_STM32_INIT_PORT_F1(C);
_STM32_INIT_PORT_F1(D);

// Optional port E (not all F1 variants have this)
#if defined(GPIOE)
  _STM32_INIT_PORT_F1(E);
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
