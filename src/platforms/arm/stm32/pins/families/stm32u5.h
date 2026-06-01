#pragma once

// IWYU pragma: private

// STM32U5 family variant using unified template.
// STM32U5 GPIO has a BRR register, so reset can use BRR directly.

#include "platforms/arm/stm32/pins/core/armpin_template.h"
#include "platforms/arm/stm32/pins/core/gpio_port_init.h"
#include "platforms/arm/stm32/pins/core/pin_macros.h"

namespace fl {

#define _DEFPIN_ARM_U5(PIN, BIT, PORT) _DEFPIN_STM32(PIN, BIT, PORT, true)

_STM32_INIT_PORT(A);
_STM32_INIT_PORT(B);
_STM32_INIT_PORT(C);
_STM32_INIT_PORT(D);
_STM32_INIT_PORT(E);

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

#ifdef FASTLED_STM32_BOARD_FILE
  #include FASTLED_STM32_BOARD_FILE  // nolint
#else
  #error "FASTLED_STM32_BOARD_FILE not defined - include this file via fastpin_dispatcher.h"
#endif

#define HAS_HARDWARE_PIN_SUPPORT

}  // namespace fl
