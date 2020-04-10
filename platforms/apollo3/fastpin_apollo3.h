#ifndef __INC_FASTPIN_APOLLO3_H
#define __INC_FASTPIN_APOLLO3_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

template<uint8_t PIN> class _APOLLO3PIN {

public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;

  inline static void setOutput() { pinMode(PIN, OUTPUT); am_hal_gpio_fastgpio_enable(PIN); }
  inline static void setInput() { am_hal_gpio_fastgpio_disable(PIN); pinMode(PIN, INPUT); }

  inline static void hi() __attribute__ ((always_inline)) { am_hal_gpio_fastgpio_set(PIN); }
  inline static void lo() __attribute__ ((always_inline)) { am_hal_gpio_fastgpio_clr(PIN); }
  inline static void set(register port_t val) __attribute__ ((always_inline)) { if(val) { am_hal_gpio_fastgpio_set(PIN); } else { am_hal_gpio_fastgpio_clr(PIN); } }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) { if( am_hal_gpio_fastgpio_read(PIN)) { lo(); } else { hi(); } }

  inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { set(val); }

  inline static port_t hival() __attribute__ ((always_inline)) { return 0; }
  inline static port_t loval() __attribute__ ((always_inline)) { return 0; }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return NULL; }
  inline static port_t mask() __attribute__ ((always_inline)) { return 0; }
};

#define _FL_DEFPIN(PIN) template<> class FastPin<PIN> : public _APOLLO3PIN<PIN> {};

// Actual pin definitions
#if defined(ARDUINO_SFE_EDGE)

#define MAX_PIN 50
_FL_DEFPIN(0); _FL_DEFPIN(1); _FL_DEFPIN(3); _FL_DEFPIN(4);
_FL_DEFPIN(5); _FL_DEFPIN(6); _FL_DEFPIN(7); _FL_DEFPIN(8); _FL_DEFPIN(9);
_FL_DEFPIN(10); _FL_DEFPIN(11); _FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14);
_FL_DEFPIN(15); _FL_DEFPIN(17);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23); _FL_DEFPIN(24);
_FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27); _FL_DEFPIN(28); _FL_DEFPIN(29);
_FL_DEFPIN(33);
_FL_DEFPIN(36); _FL_DEFPIN(37); _FL_DEFPIN(38); _FL_DEFPIN(39);
_FL_DEFPIN(40); _FL_DEFPIN(42); _FL_DEFPIN(43); _FL_DEFPIN(44);
_FL_DEFPIN(46); _FL_DEFPIN(47); _FL_DEFPIN(48); _FL_DEFPIN(49);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_SFE_EDGE2)

#define MAX_PIN 50
_FL_DEFPIN(0);
_FL_DEFPIN(5); _FL_DEFPIN(6); _FL_DEFPIN(7); _FL_DEFPIN(8); _FL_DEFPIN(9);
_FL_DEFPIN(11); _FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14);
_FL_DEFPIN(15); _FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(23);
_FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27); _FL_DEFPIN(28); _FL_DEFPIN(29);
_FL_DEFPIN(31); _FL_DEFPIN(32); _FL_DEFPIN(33); _FL_DEFPIN(34);
_FL_DEFPIN(35); _FL_DEFPIN(37); _FL_DEFPIN(39);
_FL_DEFPIN(40); _FL_DEFPIN(41); _FL_DEFPIN(42); _FL_DEFPIN(43); _FL_DEFPIN(44);
_FL_DEFPIN(45); _FL_DEFPIN(48); _FL_DEFPIN(49);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AM_AP3_SFE_BB_ARTEMIS)

#define MAX_PIN 32
_FL_DEFPIN(0); _FL_DEFPIN(1); _FL_DEFPIN(2); _FL_DEFPIN(3); _FL_DEFPIN(4);
_FL_DEFPIN(5); _FL_DEFPIN(6); _FL_DEFPIN(7); _FL_DEFPIN(8); _FL_DEFPIN(9);
_FL_DEFPIN(10); _FL_DEFPIN(11); _FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14);
_FL_DEFPIN(15); _FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23); _FL_DEFPIN(24);
_FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27); _FL_DEFPIN(28); _FL_DEFPIN(29);
_FL_DEFPIN(30); _FL_DEFPIN(31);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AM_AP3_SFE_BB_ARTEMIS_NANO)

#define MAX_PIN 24
_FL_DEFPIN(0); _FL_DEFPIN(1); _FL_DEFPIN(2); _FL_DEFPIN(3); _FL_DEFPIN(4);
_FL_DEFPIN(5); _FL_DEFPIN(6); _FL_DEFPIN(7); _FL_DEFPIN(8); _FL_DEFPIN(9);
_FL_DEFPIN(10); _FL_DEFPIN(11); _FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14);
_FL_DEFPIN(15); _FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AM_AP3_SFE_THING_PLUS)

#define MAX_PIN 29
_FL_DEFPIN(0); _FL_DEFPIN(1); _FL_DEFPIN(2); _FL_DEFPIN(3); _FL_DEFPIN(4);
_FL_DEFPIN(5); _FL_DEFPIN(6); _FL_DEFPIN(7); _FL_DEFPIN(8); _FL_DEFPIN(9);
_FL_DEFPIN(10); _FL_DEFPIN(11); _FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14);
_FL_DEFPIN(15); _FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23); _FL_DEFPIN(24);
_FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27); _FL_DEFPIN(28);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AM_AP3_SFE_BB_ARTEMIS_ATP) || defined(ARDUINO_SFE_ARTEMIS)

#define MAX_PIN 50 // AP3_VARIANT_NUM_PINS
_FL_DEFPIN(0); _FL_DEFPIN(1); _FL_DEFPIN(2); _FL_DEFPIN(3); _FL_DEFPIN(4);
_FL_DEFPIN(5); _FL_DEFPIN(6); _FL_DEFPIN(7); _FL_DEFPIN(8); _FL_DEFPIN(9);
_FL_DEFPIN(10); _FL_DEFPIN(11); _FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14);
_FL_DEFPIN(15); _FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23); _FL_DEFPIN(24);
_FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27); _FL_DEFPIN(28); _FL_DEFPIN(29);
_FL_DEFPIN(31); _FL_DEFPIN(32); _FL_DEFPIN(33); _FL_DEFPIN(34);
_FL_DEFPIN(35); _FL_DEFPIN(36); _FL_DEFPIN(37); _FL_DEFPIN(38); _FL_DEFPIN(39);
_FL_DEFPIN(40); _FL_DEFPIN(41); _FL_DEFPIN(42); _FL_DEFPIN(43); _FL_DEFPIN(44);
_FL_DEFPIN(45); _FL_DEFPIN(47); _FL_DEFPIN(48); _FL_DEFPIN(49);

#define HAS_HARDWARE_PIN_SUPPORT 1

#else

#error "Unrecognised APOLLO3 board!"

#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END

#endif // __INC_FASTPIN_AVR_H
