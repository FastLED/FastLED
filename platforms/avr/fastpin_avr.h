#ifndef __INC_FASTPIN_AVR_H
#define __INC_FASTPIN_AVR_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

#define AVR_PIN_CYCLES(_PIN) ((((int)FastPin<_PIN>::port())-0x20 < 64) ? 1 : 2)

/// Class definition for a Pin where we know the port registers at compile time for said pin.  This allows us to make
/// a lot of optimizations, as the inlined hi/lo methods will devolve to a single io register write/bitset.
template<uint8_t PIN, uint8_t _MASK, typename _PORT, typename _DDR, typename _PIN> class _AVRPIN {
public:
	typedef volatile uint8_t * port_ptr_t;
	typedef uint8_t port_t;

	inline static void setOutput() { _DDR::r() |= _MASK; }
	inline static void setInput() { _DDR::r() &= ~_MASK; }

	inline static void hi() __attribute__ ((always_inline)) { _PORT::r() |= _MASK; }
	inline static void lo() __attribute__ ((always_inline)) { _PORT::r() &= ~_MASK; }
	inline static void set(register uint8_t val) __attribute__ ((always_inline)) { _PORT::r() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

	inline static void toggle() __attribute__ ((always_inline)) { _PIN::r() = _MASK; }

	inline static void hi(register port_ptr_t /*port*/) __attribute__ ((always_inline)) { hi(); }
	inline static void lo(register port_ptr_t /*port*/) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(register port_ptr_t /*port*/, register uint8_t val) __attribute__ ((always_inline)) { set(val); }

	inline static port_t hival() __attribute__ ((always_inline)) { return _PORT::r() | _MASK; }
	inline static port_t loval() __attribute__ ((always_inline)) { return _PORT::r() & ~_MASK; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_PORT::r(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};



/// AVR definitions for pins.  Getting around  the fact that I can't pass GPIO register addresses in as template arguments by instead creating
/// a custom type for each GPIO register with a single, static, aggressively inlined function that returns that specific GPIO register.  A similar
/// trick is used a bit further below for the ARM GPIO registers (of which there are far more than on AVR!)
typedef volatile uint8_t & reg8_t;
#define _R(T) struct __gen_struct_ ## T
#define _RD8(T) struct __gen_struct_ ## T { static inline reg8_t r() { return T; }};
#define _FL_IO(L,C) _RD8(DDR ## L); _RD8(PORT ## L); _RD8(PIN ## L); _FL_DEFINE_PORT3(L, C, _R(PORT ## L));
#define _FL_DEFPIN(_PIN, BIT, L) template<> class FastPin<_PIN> : public _AVRPIN<_PIN, 1<<BIT, _R(PORT ## L), _R(DDR ## L), _R(PIN ## L)> {};

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

#if defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny25__)

#if defined(__AVR_ATtiny25__)
#pragma message "ATtiny25 has very limited storage. This library could use up to more than 100% of its flash size"
#endif

#define MAX_PIN 5

_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B); _FL_DEFPIN(3, 3, B);
_FL_DEFPIN(4, 4, B); _FL_DEFPIN(5, 5, B);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(__AVR_ATtiny841__) || defined(__AVR_ATtiny441__)
#define MAX_PIN 11

_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B);
_FL_DEFPIN(3, 7, A); _FL_DEFPIN(4, 6, A); _FL_DEFPIN(5, 5, A);
_FL_DEFPIN(6, 4, A); _FL_DEFPIN(7, 3, A); _FL_DEFPIN(8, 2, A);
_FL_DEFPIN(9, 1, A); _FL_DEFPIN(10, 0, A); _FL_DEFPIN(11, 3, B);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AVR_DIGISPARK) // digispark pin layout
#define MAX_PIN 5
#define HAS_HARDWARE_PIN_SUPPORT 1

_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B);
_FL_DEFPIN(3, 7, A); _FL_DEFPIN(4, 6, A); _FL_DEFPIN(5, 5, A);

#elif defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)

#define MAX_PIN 10

_FL_DEFPIN(0, 0, A); _FL_DEFPIN(1, 1, A); _FL_DEFPIN(2, 2, A); _FL_DEFPIN(3, 3, A);
_FL_DEFPIN(4, 4, A); _FL_DEFPIN(5, 5, A); _FL_DEFPIN(6, 6, A); _FL_DEFPIN(7, 7, A);
_FL_DEFPIN(8, 2, B); _FL_DEFPIN(9, 1, B); _FL_DEFPIN(10, 0, B);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AVR_DIGISPARKPRO)

#define MAX_PIN 12

_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B); _FL_DEFPIN(3, 5, B);
_FL_DEFPIN(4, 3, B); _FL_DEFPIN(5, 7, A); _FL_DEFPIN(6, 0, A); _FL_DEFPIN(7, 1, A);
_FL_DEFPIN(8, 2, A); _FL_DEFPIN(9, 3, A); _FL_DEFPIN(10, 4, A); _FL_DEFPIN(11, 5, A);
_FL_DEFPIN(12, 6, A);

#elif defined(__AVR_ATtiny167__) || defined(__AVR_ATtiny87__)

#define MAX_PIN 15

_FL_DEFPIN(0, 0, A);  _FL_DEFPIN(1, 1, A);   _FL_DEFPIN(2, 2, A);  _FL_DEFPIN(3, 3, A);
_FL_DEFPIN(4, 4, A);  _FL_DEFPIN(5, 5, A);   _FL_DEFPIN(6, 6, A);  _FL_DEFPIN(7, 7, A);
_FL_DEFPIN(8, 0, B);  _FL_DEFPIN(9, 1, B);   _FL_DEFPIN(10, 2, B); _FL_DEFPIN(11, 3, B);
_FL_DEFPIN(12, 4, B); _FL_DEFPIN(13, 5, B); _FL_DEFPIN(14, 6, B); _FL_DEFPIN(15, 7, B);

#define SPI_DATA 4
#define SPI_CLOCK 5
#define AVR_HARDWARE_SPI 1

#define HAS_HARDWARE_PIN_SUPPORT 1
#elif defined(ARDUINO_HOODLOADER2) && (defined(__AVR_ATmega32U2__) || defined(__AVR_ATmega16U2__) || defined(__AVR_ATmega8U2__)) || defined(__AVR_AT90USB82__) || defined(__AVR_AT90USB162__)

#define MAX_PIN 20

_FL_DEFPIN( 0, 0, B); _FL_DEFPIN( 1, 1, B); _FL_DEFPIN( 2, 2, B); _FL_DEFPIN( 3, 3, B);
_FL_DEFPIN( 4, 4, B); _FL_DEFPIN( 5, 5, B); _FL_DEFPIN( 6, 6, B); _FL_DEFPIN( 7, 7, B);

_FL_DEFPIN( 8, 7, C); _FL_DEFPIN( 9, 6, C); _FL_DEFPIN( 10, 5,C); _FL_DEFPIN( 11, 4, C);
_FL_DEFPIN( 12, 2, C); _FL_DEFPIN( 13, 0, D); _FL_DEFPIN( 14, 1, D); _FL_DEFPIN(15, 2, D);
_FL_DEFPIN( 16, 3, D); _FL_DEFPIN( 17, 4, D); _FL_DEFPIN( 18, 5, D); _FL_DEFPIN( 19, 6, D);
_FL_DEFPIN( 20, 7, D);

#define HAS_HARDWARE_PIN_SUPPORT 1
// #define SPI_DATA 2
// #define SPI_CLOCK 1
// #define AVR_HARDWARE_SPI 1

#elif defined(IS_BEAN)

#define MAX_PIN 19
_FL_DEFPIN( 0, 6, D); _FL_DEFPIN( 1, 1, B); _FL_DEFPIN( 2, 2, B); _FL_DEFPIN( 3, 3, B);
_FL_DEFPIN( 4, 4, B); _FL_DEFPIN( 5, 5, B); _FL_DEFPIN( 6, 0, D); _FL_DEFPIN( 7, 7, D);
_FL_DEFPIN( 8, 0, B); _FL_DEFPIN( 9, 1, D); _FL_DEFPIN(10, 2, D); _FL_DEFPIN(11, 3, D);
_FL_DEFPIN(12, 4, D); _FL_DEFPIN(13, 5, D); _FL_DEFPIN(14, 0, C); _FL_DEFPIN(15, 1, C);
_FL_DEFPIN(16, 2, C); _FL_DEFPIN(17, 3, C); _FL_DEFPIN(18, 4, C); _FL_DEFPIN(19, 5, C);

#define SPI_DATA 3
#define SPI_CLOCK 5
#define SPI_SELECT 2
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#ifndef __AVR_ATmega8__
#define SPI_UART0_DATA 9
#define SPI_UART0_CLOCK 12
#endif

#elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328PB__) || defined(__AVR_ATmega328__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega168P__) || defined(__AVR_ATmega8__)

#define MAX_PIN 19
_FL_DEFPIN( 0, 0, D); _FL_DEFPIN( 1, 1, D); _FL_DEFPIN( 2, 2, D); _FL_DEFPIN( 3, 3, D);
_FL_DEFPIN( 4, 4, D); _FL_DEFPIN( 5, 5, D); _FL_DEFPIN( 6, 6, D); _FL_DEFPIN( 7, 7, D);
_FL_DEFPIN( 8, 0, B); _FL_DEFPIN( 9, 1, B); _FL_DEFPIN(10, 2, B); _FL_DEFPIN(11, 3, B);
_FL_DEFPIN(12, 4, B); _FL_DEFPIN(13, 5, B); _FL_DEFPIN(14, 0, C); _FL_DEFPIN(15, 1, C);
_FL_DEFPIN(16, 2, C); _FL_DEFPIN(17, 3, C); _FL_DEFPIN(18, 4, C); _FL_DEFPIN(19, 5, C);

#define SPI_DATA 11
#define SPI_CLOCK 13
#define SPI_SELECT 10
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#ifndef __AVR_ATmega8__
#define SPI_UART0_DATA 1
#define SPI_UART0_CLOCK 4
#endif

#elif defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega644P__) || defined(__AVR_ATmega32__) || defined(__AVR_ATmega16__)

#define MAX_PIN 31
_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B); _FL_DEFPIN(3, 3, B);
_FL_DEFPIN(4, 4, B); _FL_DEFPIN(5, 5, B); _FL_DEFPIN(6, 6, B); _FL_DEFPIN(7, 7, B);
_FL_DEFPIN(8, 0, D); _FL_DEFPIN(9, 1, D); _FL_DEFPIN(10, 2, D); _FL_DEFPIN(11, 3, D);
_FL_DEFPIN(12, 4, D); _FL_DEFPIN(13, 5, D); _FL_DEFPIN(14, 6, D); _FL_DEFPIN(15, 7, D);
_FL_DEFPIN(16, 0, C); _FL_DEFPIN(17, 1, C); _FL_DEFPIN(18, 2, C); _FL_DEFPIN(19, 3, C);
_FL_DEFPIN(20, 4, C); _FL_DEFPIN(21, 5, C); _FL_DEFPIN(22, 6, C); _FL_DEFPIN(23, 7, C);
_FL_DEFPIN(24, 0, A); _FL_DEFPIN(25, 1, A); _FL_DEFPIN(26, 2, A); _FL_DEFPIN(27, 3, A);
_FL_DEFPIN(28, 4, A); _FL_DEFPIN(29, 5, A); _FL_DEFPIN(30, 6, A); _FL_DEFPIN(31, 7, A);

#define SPI_DATA 5
#define SPI_CLOCK 7
#define SPI_SELECT 4
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#elif  defined(__AVR_ATmega128RFA1__) || defined(__AVR_ATmega256RFR2__)

// AKA the Pinoccio
_FL_DEFPIN( 0, 0, E); _FL_DEFPIN( 1, 1, E); _FL_DEFPIN( 2, 7, B); _FL_DEFPIN( 3, 3, E);
_FL_DEFPIN( 4, 4, E); _FL_DEFPIN( 5, 5, E); _FL_DEFPIN( 6, 2, E); _FL_DEFPIN( 7, 6, E);
_FL_DEFPIN( 8, 5, D); _FL_DEFPIN( 9, 0, B); _FL_DEFPIN(10, 2, B); _FL_DEFPIN(11, 3, B);
_FL_DEFPIN(12, 1, B); _FL_DEFPIN(13, 2, D); _FL_DEFPIN(14, 3, D); _FL_DEFPIN(15, 0, D);
_FL_DEFPIN(16, 1, D); _FL_DEFPIN(17, 4, D); _FL_DEFPIN(18, 7, E); _FL_DEFPIN(19, 6, D);
_FL_DEFPIN(20, 7, D); _FL_DEFPIN(21, 4, B); _FL_DEFPIN(22, 5, B); _FL_DEFPIN(23, 6, B);
_FL_DEFPIN(24, 0, F); _FL_DEFPIN(25, 1, F); _FL_DEFPIN(26, 2, F); _FL_DEFPIN(27, 3, F);
_FL_DEFPIN(28, 4, F); _FL_DEFPIN(29, 5, F); _FL_DEFPIN(30, 6, F); _FL_DEFPIN(31, 7, F);

#define SPI_DATA 10
#define SPI_CLOCK 12
#define SPI_SELECT 9

#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
// megas
#define MAX_PIN 69
_FL_DEFPIN(0, 0, E); _FL_DEFPIN(1, 1, E); _FL_DEFPIN(2, 4, E); _FL_DEFPIN(3, 5, E);
_FL_DEFPIN(4, 5, G); _FL_DEFPIN(5, 3, E); _FL_DEFPIN(6, 3, H); _FL_DEFPIN(7, 4, H);
_FL_DEFPIN(8, 5, H); _FL_DEFPIN(9, 6, H); _FL_DEFPIN(10, 4, B); _FL_DEFPIN(11, 5, B);
_FL_DEFPIN(12, 6, B); _FL_DEFPIN(13, 7, B); _FL_DEFPIN(14, 1, J); _FL_DEFPIN(15, 0, J);
_FL_DEFPIN(16, 1, H); _FL_DEFPIN(17, 0, H); _FL_DEFPIN(18, 3, D); _FL_DEFPIN(19, 2, D);
_FL_DEFPIN(20, 1, D); _FL_DEFPIN(21, 0, D); _FL_DEFPIN(22, 0, A); _FL_DEFPIN(23, 1, A);
_FL_DEFPIN(24, 2, A); _FL_DEFPIN(25, 3, A); _FL_DEFPIN(26, 4, A); _FL_DEFPIN(27, 5, A);
_FL_DEFPIN(28, 6, A); _FL_DEFPIN(29, 7, A); _FL_DEFPIN(30, 7, C); _FL_DEFPIN(31, 6, C);
_FL_DEFPIN(32, 5, C); _FL_DEFPIN(33, 4, C); _FL_DEFPIN(34, 3, C); _FL_DEFPIN(35, 2, C);
_FL_DEFPIN(36, 1, C); _FL_DEFPIN(37, 0, C); _FL_DEFPIN(38, 7, D); _FL_DEFPIN(39, 2, G);
_FL_DEFPIN(40, 1, G); _FL_DEFPIN(41, 0, G); _FL_DEFPIN(42, 7, L); _FL_DEFPIN(43, 6, L);
_FL_DEFPIN(44, 5, L); _FL_DEFPIN(45, 4, L); _FL_DEFPIN(46, 3, L); _FL_DEFPIN(47, 2, L);
_FL_DEFPIN(48, 1, L); _FL_DEFPIN(49, 0, L); _FL_DEFPIN(50, 3, B); _FL_DEFPIN(51, 2, B);
_FL_DEFPIN(52, 1, B); _FL_DEFPIN(53, 0, B); _FL_DEFPIN(54, 0, F); _FL_DEFPIN(55, 1, F);
_FL_DEFPIN(56, 2, F); _FL_DEFPIN(57, 3, F); _FL_DEFPIN(58, 4, F); _FL_DEFPIN(59, 5, F);
_FL_DEFPIN(60, 6, F); _FL_DEFPIN(61, 7, F); _FL_DEFPIN(62, 0, K); _FL_DEFPIN(63, 1, K);
_FL_DEFPIN(64, 2, K); _FL_DEFPIN(65, 3, K); _FL_DEFPIN(66, 4, K); _FL_DEFPIN(67, 5, K);
_FL_DEFPIN(68, 6, K); _FL_DEFPIN(69, 7, K);

#define SPI_DATA 51
#define SPI_CLOCK 52
#define SPI_SELECT 53
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

// Leonardo, teensy, blinkm
#elif defined(__AVR_ATmega32U4__) && defined(CORE_TEENSY)

// teensy defs
#define MAX_PIN 23
_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B); _FL_DEFPIN(3, 3, B);
_FL_DEFPIN(4, 7, B); _FL_DEFPIN(5, 0, D); _FL_DEFPIN(6, 1, D); _FL_DEFPIN(7, 2, D);
_FL_DEFPIN(8, 3, D); _FL_DEFPIN(9, 6, C); _FL_DEFPIN(10, 7, C); _FL_DEFPIN(11, 6, D);
_FL_DEFPIN(12, 7, D); _FL_DEFPIN(13, 4, B); _FL_DEFPIN(14, 5, B); _FL_DEFPIN(15, 6, B);
_FL_DEFPIN(16, 7, F); _FL_DEFPIN(17, 6, F); _FL_DEFPIN(18, 5, F); _FL_DEFPIN(19, 4, F);
_FL_DEFPIN(20, 1, F); _FL_DEFPIN(21, 0, F); _FL_DEFPIN(22, 4, D); _FL_DEFPIN(23, 5, D);

#define SPI_DATA 2
#define SPI_CLOCK 1
#define SPI_SELECT 0
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

// PD3/PD5
#define SPI_UART1_DATA 8
#define SPI_UART1_CLOCK 23

#elif defined(__AVR_AT90USB646__) || defined(__AVR_AT90USB1286__)
// teensy++ 2 defs
#define MAX_PIN 45
_FL_DEFPIN(0, 0, D); _FL_DEFPIN(1, 1, D); _FL_DEFPIN(2, 2, D); _FL_DEFPIN(3, 3, D);
_FL_DEFPIN(4, 4, D); _FL_DEFPIN(5, 5, D); _FL_DEFPIN(6, 6, D); _FL_DEFPIN(7, 7, D);
_FL_DEFPIN(8, 0, E); _FL_DEFPIN(9, 1, E); _FL_DEFPIN(10, 0, C); _FL_DEFPIN(11, 1, C);
_FL_DEFPIN(12, 2, C); _FL_DEFPIN(13, 3, C); _FL_DEFPIN(14, 4, C); _FL_DEFPIN(15, 5, C);
_FL_DEFPIN(16, 6, C); _FL_DEFPIN(17, 7, C); _FL_DEFPIN(18, 6, E); _FL_DEFPIN(19, 7, E);
_FL_DEFPIN(20, 0, B); _FL_DEFPIN(21, 1, B); _FL_DEFPIN(22, 2, B); _FL_DEFPIN(23, 3, B);
_FL_DEFPIN(24, 4, B); _FL_DEFPIN(25, 5, B); _FL_DEFPIN(26, 6, B); _FL_DEFPIN(27, 7, B);
_FL_DEFPIN(28, 0, A); _FL_DEFPIN(29, 1, A); _FL_DEFPIN(30, 2, A); _FL_DEFPIN(31, 3, A);
_FL_DEFPIN(32, 4, A); _FL_DEFPIN(33, 5, A); _FL_DEFPIN(34, 6, A); _FL_DEFPIN(35, 7, A);
_FL_DEFPIN(36, 4, E); _FL_DEFPIN(37, 5, E); _FL_DEFPIN(38, 0, F); _FL_DEFPIN(39, 1, F);
_FL_DEFPIN(40, 2, F); _FL_DEFPIN(41, 3, F); _FL_DEFPIN(42, 4, F); _FL_DEFPIN(43, 5, F);
_FL_DEFPIN(44, 6, F); _FL_DEFPIN(45, 7, F);

#define SPI_DATA 22
#define SPI_CLOCK 21
#define SPI_SELECT 20
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

// PD3/PD5
#define SPI_UART1_DATA 3
#define SPI_UART1_CLOCK 5


#elif defined(__AVR_ATmega32U4__)

// leonard defs
#define MAX_PIN 30
_FL_DEFPIN(0, 2, D); _FL_DEFPIN(1, 3, D); _FL_DEFPIN(2, 1, D); _FL_DEFPIN(3, 0, D);
_FL_DEFPIN(4, 4, D); _FL_DEFPIN(5, 6, C); _FL_DEFPIN(6, 7, D); _FL_DEFPIN(7, 6, E);
_FL_DEFPIN(8, 4, B); _FL_DEFPIN(9, 5, B); _FL_DEFPIN(10, 6, B); _FL_DEFPIN(11, 7, B);
_FL_DEFPIN(12, 6, D); _FL_DEFPIN(13, 7, C); _FL_DEFPIN(14, 3, B); _FL_DEFPIN(15, 1, B);
_FL_DEFPIN(16, 2, B); _FL_DEFPIN(17, 0, B); _FL_DEFPIN(18, 7, F); _FL_DEFPIN(19, 6, F);
_FL_DEFPIN(20, 5, F); _FL_DEFPIN(21, 4, F); _FL_DEFPIN(22, 1, F); _FL_DEFPIN(23, 0, F);
_FL_DEFPIN(24, 4, D); _FL_DEFPIN(25, 7, D); _FL_DEFPIN(26, 4, B); _FL_DEFPIN(27, 5, B);
_FL_DEFPIN(28, 6, B); _FL_DEFPIN(29, 6, D); _FL_DEFPIN(30, 5, D);

#define SPI_DATA 16
#define SPI_CLOCK 15
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

// PD3/PD5
#define SPI_UART1_DATA 1
#define SPI_UART1_CLOCK 30


#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END

#endif // __INC_FASTPIN_AVR_H
