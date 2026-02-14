#pragma once

// IWYU pragma: private

// Pin definition macros for STM32 families
// Provides uniform FastPin<> instantiation for all variants
//
// USAGE:
//   Family-specific files define convenience macros that set HAS_BRR:
//
//   // In families/stm32f1.h:
//   #define _DEFPIN_ARM_F1(PIN, BIT, PORT) _DEFPIN_STM32(PIN, BIT, PORT, true)
//
//   // In families/stm32f4.h:
//   #define _DEFPIN_ARM_F4(PIN, BIT, PORT) _DEFPIN_STM32(PIN, BIT, PORT, false)
//
//   Then board files use the family-specific macro (outside namespace):
//   _DEFPIN_ARM_F4(0, 0, A);  // FastPin<0> = PA0 on F4 board
//
// IMPLEMENTATION:
//   Creates FastPin<> template specialization using unified _ARMPIN_STM32 template
//   Macro is defined inside namespace fl, but can be used outside it

#include "armpin_template.h"

namespace fl {

// Internal helper struct for GPIO port references
#define _R(T) struct __gen_struct_ ## T

// Generic pin definition macro (used by family-specific macros)
// Defined inside namespace fl, so FastPin<PIN> refers to fl::FastPin<PIN>
// @param PIN Arduino digital pin number
// @param BIT GPIO bit position (0-15)
// @param PORT GPIO port letter (A, B, C, ...)
// @param HAS_BRR true if family has BRR register, false otherwise
#define _DEFPIN_STM32(PIN, BIT, PORT, HAS_BRR) \
  template<> class FastPin<PIN> : public _ARMPIN_STM32<PIN, BIT, 1 << BIT, _R(GPIO ## PORT), HAS_BRR> {};

}  // namespace fl
