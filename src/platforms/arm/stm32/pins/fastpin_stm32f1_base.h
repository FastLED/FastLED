#pragma once

// STM32F1 base definitions - F1-specific register mapping and GPIO initialization
// Used by fastpin_stm32f1.h and other STM32F1 board-specific files

#include "pin_def_stm32.h"

namespace fl {

// One style of register mapping (for Maple Mini and similar)
#define _RD32_REG_MAP(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) FASTLED_FORCE_INLINE volatile gpio_reg_map* r() { return T->regs; } };

// Second style of register mapping (for generic STM32F1)
#define _RD32_GPIO_MAP(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) FASTLED_FORCE_INLINE volatile GPIO_TypeDef* r() { return T; } };

#if defined(ARDUINO_MAPLE_MINI)
#define _RD32 _RD32_REG_MAP
#else
#define _RD32 _RD32_GPIO_MAP
#endif

#define _IO32(L) _RD32(GPIO ## L)

// Initialize GPIO ports A, B, C, D
_IO32(A); _IO32(B); _IO32(C); _IO32(D);

}  // namespace fl
