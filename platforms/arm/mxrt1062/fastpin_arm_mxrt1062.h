#ifndef __FASTPIN_ARM_MXRT1062_H
#define __FASTPIN_ARM_MXRT1062_H

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
	inline static void set(register port_t val) __attribute__ ((always_inline)) { _GPIO_DR::r() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

	inline static void toggle() __attribute__ ((always_inline)) { _GPIO_DR_TOGGLE::r() = _MASK; }

	inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
	inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

	inline static port_t hival() __attribute__ ((always_inline)) { return _GPIO_DR::r() | _MASK; }
	inline static port_t loval() __attribute__ ((always_inline)) { return _GPIO_DR::r() & ~_MASK; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_GPIO_DR::r(); }
	inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPIO_DR_SET::r(); }
	inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPIO_DR_CLEAR::r(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
  inline static uint32_t pinbit() __attribute__ ((always_inline)) { return _BIT; }
};


#define _R(T) struct __gen_struct_ ## T
#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline reg32_t r() { return T; } };
#define _IO32(L) _RD32(GPIO ## L ## _DR); _RD32(GPIO ## L ## _DR_SET); _RD32(GPIO ## L ## _DR_CLEAR); _RD32(GPIO ## L ## _DR_TOGGLE);

// From the teensy core - it looks like there's the "default set" of port registers at GPIO1-5 - but then there
// are a mirrored set for GPIO1-4 at GPIO6-9, which in the teensy core is referred to as "fast" - while the pin definitiosn
// at https://forum.pjrc.com/threads/54711-Teensy-4-0-First-Beta-Test?p=193716&viewfull=1#post193716
// refer to GPIO1-4, we're going to use GPIO6-9 in the definitions below because the fast registers are what
// the teensy core is using internally
#define _DEFPIN_T4(PIN, L, BIT) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1 << BIT, _R(GPIO ## L ## _DR), _R(GPIO ## L ## _DR_SET), _R(GPIO ## L ## _DR_CLEAR), _R(GPIO ## L ## _DR_TOGGLE)> {};

#if defined(FASTLED_TEENSY4) && defined(CORE_TEENSY)
_IO32(1); _IO32(2); _IO32(3); _IO32(4); _IO32(5);
_IO32(6); _IO32(7); _IO32(8); _IO32(9);

#define MAX_PIN 39
_DEFPIN_T4( 0,6, 3); _DEFPIN_T4( 1,6, 2); _DEFPIN_T4( 2,9, 4); _DEFPIN_T4( 3,9, 5);
_DEFPIN_T4( 4,9, 6); _DEFPIN_T4( 5,9, 8); _DEFPIN_T4( 6,7,10); _DEFPIN_T4( 7,7,17);
_DEFPIN_T4( 8,7,16); _DEFPIN_T4( 9,7,11); _DEFPIN_T4(10,7, 0); _DEFPIN_T4(11,7, 2);
_DEFPIN_T4(12,7, 1); _DEFPIN_T4(13,7, 3); _DEFPIN_T4(14,6,18); _DEFPIN_T4(15,6,19);
_DEFPIN_T4(16,6,23); _DEFPIN_T4(17,6,22); _DEFPIN_T4(18,6,17); _DEFPIN_T4(19,6,16);
_DEFPIN_T4(20,6,26); _DEFPIN_T4(21,6,27); _DEFPIN_T4(22,6,24); _DEFPIN_T4(23,6,25);
_DEFPIN_T4(24,6,12); _DEFPIN_T4(25,6,13); _DEFPIN_T4(26,6,30); _DEFPIN_T4(27,6,31);
_DEFPIN_T4(28,8,18); _DEFPIN_T4(29,9,31); _DEFPIN_T4(30,8,23); _DEFPIN_T4(31,8,22);
_DEFPIN_T4(32,7,12); _DEFPIN_T4(33,9, 7); _DEFPIN_T4(34,8,15); _DEFPIN_T4(35,8,14);
_DEFPIN_T4(36,8,13); _DEFPIN_T4(37,8,12); _DEFPIN_T4(38,8,17); _DEFPIN_T4(39,8,16);

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
