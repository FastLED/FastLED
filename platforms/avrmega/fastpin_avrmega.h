#ifndef __INC_FASTPIN_AVRMEGA_H
#define __INC_FASTPIN_AVRMEGA_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

// THe AVRmega doesn't have the low memory fast GPIO access that the avr does
#define AVR_PIN_CYCLES(_PIN) (2)

/// Class definition for a Pin where we know the port registers at compile time for said pin.  This allows us to make
/// a lot of optimizations, as the inlined hi/lo methods will devolve to a single io register write/bitset.
template<uint8_t PIN, uint8_t _MASK, typename _PORT> class _AVRMEGAPIN {
public:
	typedef volatile uint8_t * port_ptr_t;
	typedef uint8_t port_t;

	inline static void setOutput() { pinMode(PIN, OUTPUT); }
	inline static void setInput() { pinMode(PIN, INPUT); }

	inline static void hi() __attribute__ ((always_inline)) { _PORT::r().OUTSET = _MASK; }
	inline static void lo() __attribute__ ((always_inline)) { _PORT::r().OUTCLR = _MASK; }
	inline static void set(register uint8_t val) __attribute__ ((always_inline)) { _PORT::r().OUT = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

	inline static void toggle() __attribute__ ((always_inline)) { _PORT::r().OUTTGL = _MASK; }

	inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
	inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(register port_ptr_t port, register uint8_t val) __attribute__ ((always_inline)) { set(val); }

	inline static port_t hival() __attribute__ ((always_inline)) { return _PORT::r().OUT | _MASK; }
	inline static port_t loval() __attribute__ ((always_inline)) { return _PORT::r().OUT & ~_MASK; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_PORT::r().OUT; }
	inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};



/// AVR definitions for pins.  Getting around  the fact that I can't pass GPIO register addresses in as template arguments by instead creating
/// a custom type for each GPIO register with a single, static, aggressively inlined function that returns that specific GPIO register.  A similar
/// trick is used a bit further below for the ARM GPIO registers (of which there are far more than on AVR!)
typedef volatile uint8_t & reg8_t;
#define _R(T) struct __gen_struct_ ## T
#define _RD8(T) struct __gen_struct_ ## T { static inline volatile PORT_t & r() { return T; }};
#define _FL_IO(L,C) _RD8(PORT ## L); template<> struct __FL_PORT_INFO<C> { static bool hasPort() { return 1; } \
										static const char *portName() { return #L; } \
										typedef _R(PORT ## L) __t_baseType;  \
										static const void *portAddr() { return (void*)&(__t_baseType::r().OUT); } };
#define _FL_DEFPIN(_PIN, BIT, L) template<> class FastPin<_PIN> : public _AVRMEGAPIN<_PIN, 1<<BIT, _R(PORT ## L)> {};

// Pre-do all the port definitions
#ifdef PORTA
  _FL_IO(A,0)
#endif
#ifdef PORTB
  _FL_IO(B,1)
#endif
#ifdef PORTC
  _FL_IO(C,2)
#endif
#ifdef PORTD
  _FL_IO(D,3)
#endif
#ifdef PORTE
  _FL_IO(E,4)
#endif
#ifdef PORTF
  _FL_IO(F,5)
#endif
#ifdef PORTG
  _FL_IO(G,6)
#endif
#ifdef PORTH
  _FL_IO(H,7)
#endif
#ifdef PORTI
  _FL_IO(I,8)
#endif
#ifdef PORTJ
  _FL_IO(J,9)
#endif
#ifdef PORTK
  _FL_IO(K,10)
#endif
#ifdef PORTL
  _FL_IO(L,11)
#endif
#ifdef PORTM
  _FL_IO(M,12)
#endif
#ifdef PORTN
  _FL_IO(N,13)
#endif

#if defined(ARDUINO_AVR_NANO_EVERY)
_FL_DEFPIN(0,5,C); _FL_DEFPIN(1,4,C); _FL_DEFPIN(2,0,A); _FL_DEFPIN(3,5,F);
_FL_DEFPIN(4,6,C); _FL_DEFPIN(5,2,B); _FL_DEFPIN(6,4,F); _FL_DEFPIN(7,1,A);
_FL_DEFPIN(8,3,E); _FL_DEFPIN(9,0,B); _FL_DEFPIN(10,1,B); _FL_DEFPIN(11,0,E);
_FL_DEFPIN(12,1,E); _FL_DEFPIN(13,2,E); _FL_DEFPIN(14,3,D); _FL_DEFPIN(15,2,D);
_FL_DEFPIN(16,1,D); _FL_DEFPIN(17,0,D); _FL_DEFPIN(18,2,F); _FL_DEFPIN(19,3,F);
_FL_DEFPIN(20,4,D); _FL_DEFPIN(21,5,D); _FL_DEFPIN(22,2,A);

#define HAS_HARDWARE_PIN_SUPPORT 1
#define MAX_PIN 22

// no hardware SPI yet

#else
#pragma warning No pin definitions found.  Try running examples/Pintest/Pintest.ino to get definitions from your AVRMEGA device
#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END

#endif // __INC_FASTPIN_AVR_H
