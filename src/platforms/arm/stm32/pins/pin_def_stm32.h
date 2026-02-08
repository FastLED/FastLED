#pragma once

#warning "pin_def_stm32.h is deprecated. Use core/armpin_template.h instead. See src/platforms/arm/stm32/pins/README.md for migration guide."

// DEPRECATED: This file is deprecated in favor of the new unified architecture
// Please use core/armpin_template.h which provides a single template for all STM32 families
// This file will be removed in a future release

// STM32 base definitions - shared class and macros for all STM32 pin definitions
// Used by fastpin_stm32f1.h, fastpin_stm32_spark.h, and other STM32 board-specific files

#include "fl/stl/stdint.h"
#include "fl/fastpin_base.h"

// Arduino constants for pinMode()
// These are typically provided by Arduino.h, but we define them here
// if not already defined to make the header self-contained
#ifndef INPUT
#define INPUT 0
#endif
#ifndef OUTPUT
#define OUTPUT 1
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 2
#endif

namespace fl {

/// Template definition for STM32 style ARM pins, providing direct access to the various GPIO registers.
/// This class is shared across STM32F1, Spark Core, and similar STM32 boards that use BRR register.
template<u8 PIN, u8 _BIT, u32 _MASK, typename _GPIO> class _ARMPIN {
public:
  typedef volatile u32 * port_ptr_t;
  typedef u32 port_t;

  inline static void setOutput() { pinMode(PIN, OUTPUT); }
  inline static void setInput() { pinMode(PIN, INPUT); }

  inline static void hi() __attribute__ ((always_inline)) { _GPIO::r()->BSRR = _MASK; }
  inline static void lo() __attribute__ ((always_inline)) { _GPIO::r()->BRR = _MASK; }
  inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { _GPIO::r()->ODR = val; }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }
  inline static void toggle() __attribute__ ((always_inline)) { if(_GPIO::r()->ODR & _MASK) { lo(); } else { hi(); } }

  inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) { return _GPIO::r()->ODR | _MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return _GPIO::r()->ODR & ~_MASK; }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_GPIO::r()->ODR; }
  inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPIO::r()->BSRR; }
  inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPIO::r()->BRR; }
  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }

  static constexpr bool validpin() { return true; }
};

#define _R(T) struct __gen_struct_ ## T
#define _DEFPIN_ARM(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1 << BIT, _R(GPIO ## L)> {};

}  // namespace fl
