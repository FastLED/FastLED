#ifndef __FASTPIN_ARM_RP2040_H
#define __FASTPIN_ARM_RP2040_H

#include "pico.h"
#include "hardware/gpio.h"
#include "hardware/structs/sio.h"

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be sloightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

// warning: set and fastset are not thread-safe! use with caution!
template<uint PIN, uint32_t _MASK> class _RP2040PIN {
public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;
  
  inline static void setOutput() { gpio_set_function(PIN, GPIO_FUNC_SIO); sio_hw->gpio_oe_set = _MASK; }
  inline static void setInput() { gpio_set_function(PIN, GPIO_FUNC_SIO); sio_hw->gpio_oe_clr = _MASK; }

  inline static void hi() __attribute__ ((always_inline)) { sio_hw->gpio_set = _MASK; }
  inline static void lo() __attribute__ ((always_inline)) { sio_hw->gpio_clr = _MASK; }
  inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { sio_hw->gpio_out = val; }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) { sio_hw->gpio_togl = _MASK; }

  inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) { return sio_hw->gpio_out | _MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return sio_hw->gpio_out & ~_MASK; }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return &sio_hw->gpio_out; }
  inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &sio_hw->gpio_set; }
  inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &sio_hw->gpio_clr; }
  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

#define _FL_DEFPIN(PIN) template<> class FastPin<PIN> : public _RP2040PIN<PIN, 1 << PIN> {};

#define MAX_PIN 29
_FL_DEFPIN(0); _FL_DEFPIN(1); _FL_DEFPIN(2); _FL_DEFPIN(3);
_FL_DEFPIN(4); _FL_DEFPIN(5); _FL_DEFPIN(6); _FL_DEFPIN(7);
_FL_DEFPIN(8); _FL_DEFPIN(9); _FL_DEFPIN(10); _FL_DEFPIN(11);
_FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14); _FL_DEFPIN(15);
_FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23);
_FL_DEFPIN(24); _FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27);
_FL_DEFPIN(28); _FL_DEFPIN(29);

#ifdef PICO_DEFAULT_SPI_TX_PIN
#define SPI_DATA PICO_DEFAULT_SPI_TX_PIN
#else
#define SPI_DATA 19
#endif

#ifdef PICO_DEFAULT_SPI_SCK_PIN
#define SPI_CLOCK PICO_DEFAULT_SPI_SCK_PIN
#else
#define SPI_CLOCK 18
#endif

#define HAS_HARDWARE_PIN_SUPPORT

#endif // FASTLED_FORCE_SOFTWARE_PINS


FASTLED_NAMESPACE_END

#endif // __FASTPIN_ARM_RP2040_H
