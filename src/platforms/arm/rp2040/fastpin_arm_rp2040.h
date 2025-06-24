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

// Use 64-bit literal for pin mask calculation to support RP2350B pins 32-47
// The template parameter is still uint32_t to maintain register compatibility
#define _FL_DEFPIN(PIN) template<> class FastPin<PIN> : public _RP2040PIN<PIN, (uint32_t)(1ULL << PIN)> {};

// Set MAX_PIN based on platform capability
#if defined(PICO_RP2350)
// RP2350B has up to 48 pins (0-47), RP2350A has 30 pins (0-29) 
// We support up to 47 for RP2350 variants
#define MAX_PIN 47
#else
// RP2040 has 30 pins (0-29)
#define MAX_PIN 29
#endif

// Define pins 0-29 for all RP2040/RP2350 variants
_FL_DEFPIN(0); _FL_DEFPIN(1); _FL_DEFPIN(2); _FL_DEFPIN(3);
_FL_DEFPIN(4); _FL_DEFPIN(5); _FL_DEFPIN(6); _FL_DEFPIN(7);
_FL_DEFPIN(8); _FL_DEFPIN(9); _FL_DEFPIN(10); _FL_DEFPIN(11);
_FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14); _FL_DEFPIN(15);
_FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23);
_FL_DEFPIN(24); _FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27);
_FL_DEFPIN(28); _FL_DEFPIN(29);

// Define additional pins 30-47 for RP2350 variants with extended GPIO
#if defined(PICO_RP2350)
_FL_DEFPIN(30); _FL_DEFPIN(31); _FL_DEFPIN(32); _FL_DEFPIN(33);
_FL_DEFPIN(34); _FL_DEFPIN(35); _FL_DEFPIN(36); _FL_DEFPIN(37);
_FL_DEFPIN(38); _FL_DEFPIN(39); _FL_DEFPIN(40); _FL_DEFPIN(41);
_FL_DEFPIN(42); _FL_DEFPIN(43); _FL_DEFPIN(44); _FL_DEFPIN(45);
_FL_DEFPIN(46); _FL_DEFPIN(47);
#endif

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
