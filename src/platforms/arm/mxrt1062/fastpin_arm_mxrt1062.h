#ifndef __FASTPIN_ARM_MXRT1062_H
#define __FASTPIN_ARM_MXRT1062_H

#include "force_inline.h"

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

/// Template definition for teensy 4.0 style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  It calls through to pinMode for setting input/output on pins
/// The registers are data output, set output, clear output, toggle output, input, and direction
template<uint8_t PIN, uint32_t _BIT, uint32_t _MASK, typename _GPIO_DR, typename _GPIO_DR_SET, typename _GPIO_DR_CLEAR, typename _GPIO_DR_TOGGLE> class _ARMPIN {
public:
	typedef volatile uint32_t * port_ptr_t;
	typedef uint32_t port_t;

	inline static void setOutput() { pinMode(PIN, OUTPUT); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
	inline static void setInput() { pinMode(PIN, INPUT); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

	inline static void hi() __attribute__ ((always_inline)) { _GPIO_DR_SET::r() = _MASK; }
	inline static void lo() __attribute__ ((always_inline)) { _GPIO_DR_CLEAR::r() = _MASK; }
	inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { _GPIO_DR::r() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

	inline static void toggle() __attribute__ ((always_inline)) { _GPIO_DR_TOGGLE::r() = _MASK; }

	inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
	inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

	inline static port_t hival() __attribute__ ((always_inline)) { return _GPIO_DR::r() | _MASK; }
	inline static port_t loval() __attribute__ ((always_inline)) { return _GPIO_DR::r() & ~_MASK; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_GPIO_DR::r(); }
	inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPIO_DR_SET::r(); }
	inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPIO_DR_CLEAR::r(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
	inline static uint32_t pinbit() __attribute__ ((always_inline)) { return _BIT; }
};


#define _R(T) struct __gen_struct_ ## T
#define _RD32(T) struct __gen_struct_ ## T { static FASTLED_FORCE_INLINE reg32_t r() { return T; } };
#define _FL_IO(L) _RD32(GPIO ## L ## _DR); _RD32(GPIO ## L ## _DR_SET); _RD32(GPIO ## L ## _DR_CLEAR); _RD32(GPIO ## L ## _DR_TOGGLE); _FL_DEFINE_PORT(L, _R(GPIO ## L ## _DR));

// From the teensy core - it looks like there's the "default set" of port registers at GPIO1-5 - but then there
// are a mirrored set for GPIO1-4 at GPIO6-9, which in the teensy core is referred to as "fast" - while the pin definitiosn
// at https://forum.pjrc.com/threads/54711-Teensy-4-0-First-Beta-Test?p=193716&viewfull=1#post193716
// refer to GPIO1-4, we're going to use GPIO6-9 in the definitions below because the fast registers are what
// the teensy core is using internally
#define _FL_DEFPIN(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1U << BIT, _R(GPIO ## L ## _DR), _R(GPIO ## L ## _DR_SET), _R(GPIO ## L ## _DR_CLEAR), _R(GPIO ## L ## _DR_TOGGLE)> {};

#if defined(FASTLED_TEENSY4) && defined(CORE_TEENSY)
_FL_IO(1); _FL_IO(2); _FL_IO(3); _FL_IO(4); _FL_IO(5);
_FL_IO(6); _FL_IO(7); _FL_IO(8); _FL_IO(9);

#define MAX_PIN 39
_FL_DEFPIN( 0, 3,6); _FL_DEFPIN( 1, 2,6); _FL_DEFPIN( 2, 4,9); _FL_DEFPIN( 3, 5,9);
_FL_DEFPIN( 4, 6,9); _FL_DEFPIN( 5, 8,9); _FL_DEFPIN( 6,10,7); _FL_DEFPIN( 7,17,7);
_FL_DEFPIN( 8,16,7); _FL_DEFPIN( 9,11,7); _FL_DEFPIN(10, 0,7); _FL_DEFPIN(11, 2,7);
_FL_DEFPIN(12, 1,7); _FL_DEFPIN(13, 3,7); _FL_DEFPIN(14,18,6); _FL_DEFPIN(15,19,6);
_FL_DEFPIN(16,23,6); _FL_DEFPIN(17,22,6); _FL_DEFPIN(18,17,6); _FL_DEFPIN(19,16,6);
_FL_DEFPIN(20,26,6); _FL_DEFPIN(21,27,6); _FL_DEFPIN(22,24,6); _FL_DEFPIN(23,25,6);
_FL_DEFPIN(24,12,6); _FL_DEFPIN(25,13,6); _FL_DEFPIN(26,30,6); _FL_DEFPIN(27,31,6);
_FL_DEFPIN(28,18,8); _FL_DEFPIN(29,31,9); _FL_DEFPIN(30,23,8); _FL_DEFPIN(31,22,8);
_FL_DEFPIN(32,12,7); _FL_DEFPIN(33, 7,9); _FL_DEFPIN(34,15,8); _FL_DEFPIN(35,14,8);
_FL_DEFPIN(36,13,8); _FL_DEFPIN(37,12,8); _FL_DEFPIN(38,17,8); _FL_DEFPIN(39,16,8);

#define HAS_HARDWARE_PIN_SUPPORT

#define ARM_HARDWARE_SPI
#define SPI_DATA 11
#define SPI_CLOCK 13

#define SPI1_DATA 26
#define SPI1_CLOCK 27

#define SPI2_DATA 35
#define SPI2_CLOCK 37

#endif // defined FASTLED_TEENSY4

#endif // FASTLED_FORCE_SOFTWARE_PINSs

FASTLED_NAMESPACE_END

#endif
