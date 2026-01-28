#pragma once

// GPIO port initialization macros for STM32 families
// Provides uniform port registration for all STM32 variants
//
// USAGE:
//   _STM32_INIT_PORT(A);  // Initializes GPIOA
//   _STM32_INIT_PORT(B);  // Initializes GPIOB
//   ...
//
// CONDITIONAL PORTS:
//   Not all STM32 variants have all GPIO ports (e.g., GPIOF, GPIOG, GPIOH, etc.)
//   Use preprocessor guards for optional ports:
//
//   #if defined(GPIOF)
//     _STM32_INIT_PORT(F);
//   #endif
//
// IMPLEMENTATION:
//   Creates a struct that provides static access to the GPIO_TypeDef* pointer
//   Compatible with STM32duino HAL (uses standard GPIO_TypeDef structure)

namespace fl {

// Generic STM32 GPIO port initializer (uses STM32duino HAL GPIO_TypeDef*)
#define _RD32(T) \
  struct __gen_struct_ ## T { \
    static __attribute__((always_inline)) FASTLED_FORCE_INLINE volatile GPIO_TypeDef* r() { \
      return T; \
    } \
  };

#define _STM32_INIT_PORT(L) _RD32(GPIO ## L)

}  // namespace fl
