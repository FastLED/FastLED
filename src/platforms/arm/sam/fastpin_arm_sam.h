#ifndef __INC_FASTPIN_ARM_SAM_H
#define __INC_FASTPIN_ARM_SAM_H

#include "force_inline.h"

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be sloightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else


/// Template definition for arduino due style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
/// The registers are data register, set output register, clear output register, set data direction register
template<uint8_t PIN, uint32_t _MASK, typename _PDOR, typename _PSOR, typename _PCOR, typename _PDDR> class _DUEPIN {
public:
	typedef volatile uint32_t * port_ptr_t;
	typedef uint32_t port_t;

	inline static void setOutput() { pinMode(PIN, OUTPUT); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
	inline static void setInput() { pinMode(PIN, INPUT); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

	inline static void hi() __attribute__ ((always_inline)) { _PSOR::r() = _MASK; }
	inline static void lo() __attribute__ ((always_inline)) { _PCOR::r() = _MASK; }
	inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { _PDOR::r() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle();  }

	inline static void toggle() __attribute__ ((always_inline)) { _PDOR::r() ^= _MASK; }

	inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
	inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

	inline static port_t hival() __attribute__ ((always_inline)) { return _PDOR::r() | _MASK; }
	inline static port_t loval() __attribute__ ((always_inline)) { return _PDOR::r() & ~_MASK; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_PDOR::r(); }
	inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_PSOR::r(); }
	inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_PCOR::r(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};


/// Template definition for DUE  style ARM pins using bit banding, providing direct access to the various GPIO registers.  GCC
/// does a poor job of optimizing around these accesses so they are not being used just yet.
template<uint8_t PIN, uint32_t _BIT, typename _PDOR, typename _PSOR, typename _PCOR, typename _PDDR> class _DUEPIN_BITBAND {
public:
	typedef volatile uint32_t * port_ptr_t;
	typedef uint32_t port_t;

	inline static void setOutput() { pinMode(PIN, OUTPUT); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
	inline static void setInput() { pinMode(PIN, INPUT); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

	inline static void hi() __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() = 1; }
	inline static void lo() __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() = 0; }
	inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

	inline static void toggle() __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() ^= 1; }

	inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi();  }
	inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

	inline static port_t hival() __attribute__ ((always_inline)) { return 1; }
	inline static port_t loval() __attribute__ ((always_inline)) { return 0; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return _PDOR::template rx<_BIT>(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return 1; }
};

#define GPIO_BITBAND_ADDR(reg, bit) (((uint32_t)&(reg) - 0x40000000) * 32 + (bit) * 4 + 0x42000000)
#define GPIO_BITBAND_PTR(reg, bit) ((uint32_t *)GPIO_BITBAND_ADDR((reg), (bit)))

#define _R(T) struct __gen_struct_ ## T
#define _RD32(T) struct __gen_struct_ ## T { static FASTLED_FORCE_INLINE reg32_t r() { return T; } \
	template<int BIT> static FASTLED_FORCE_INLINE ptr_reg32_t rx() { return GPIO_BITBAND_PTR(T, BIT); } };
#define _FL_IO(L,C) _RD32(REG_PIO ## L ## _ODSR); _RD32(REG_PIO ## L ## _SODR); _RD32(REG_PIO ## L ## _CODR); _RD32(REG_PIO ## L ## _OER); _FL_DEFINE_PORT3(L, C, _R(REG_PIO ## L ## _ODSR));

#define _FL_DEFPIN(PIN, BIT, L) template<> class FastPin<PIN> : public _DUEPIN<PIN, 1U << BIT, _R(REG_PIO ## L ## _ODSR), _R(REG_PIO ## L ## _SODR), _R(REG_PIO ## L ## _CODR), \
  																			_R(GPIO ## L ## _OER)> {}; \
  								   template<> class FastPinBB<PIN> : public _DUEPIN_BITBAND<PIN, BIT, _R(REG_PIO ## L ## _ODSR), _R(REG_PIO ## L ## _SODR), _R(REG_PIO ## L ## _CODR), \
  																			_R(GPIO ## L ## _OER)> {};

_FL_IO(A,0);
_FL_IO(B,1);
_FL_IO(C,2);
_FL_IO(D,3);

#if defined(__SAM3X8E__)

#define MAX_PIN 78
_FL_DEFPIN(0, 8, A); _FL_DEFPIN(1, 9, A); _FL_DEFPIN(2, 25, B); _FL_DEFPIN(3, 28, C);
_FL_DEFPIN(4, 26, C); _FL_DEFPIN(5, 25, C); _FL_DEFPIN(6, 24, C); _FL_DEFPIN(7, 23, C);
_FL_DEFPIN(8, 22, C); _FL_DEFPIN(9, 21, C); _FL_DEFPIN(10, 29, C); _FL_DEFPIN(11, 7, D);
_FL_DEFPIN(12, 8, D); _FL_DEFPIN(13, 27, B); _FL_DEFPIN(14, 4, D); _FL_DEFPIN(15, 5, D);
_FL_DEFPIN(16, 13, A); _FL_DEFPIN(17, 12, A); _FL_DEFPIN(18, 11, A); _FL_DEFPIN(19, 10, A);
_FL_DEFPIN(20, 12, B); _FL_DEFPIN(21, 13, B); _FL_DEFPIN(22, 26, B); _FL_DEFPIN(23, 14, A);
_FL_DEFPIN(24, 15, A); _FL_DEFPIN(25, 0, D); _FL_DEFPIN(26, 1, D); _FL_DEFPIN(27, 2, D);
_FL_DEFPIN(28, 3, D); _FL_DEFPIN(29, 6, D); _FL_DEFPIN(30, 9, D); _FL_DEFPIN(31, 7, A);
_FL_DEFPIN(32, 10, D); _FL_DEFPIN(33, 1, C); _FL_DEFPIN(34, 2, C); _FL_DEFPIN(35, 3, C);
_FL_DEFPIN(36, 4, C); _FL_DEFPIN(37, 5, C); _FL_DEFPIN(38, 6, C); _FL_DEFPIN(39, 7, C);
_FL_DEFPIN(40, 8, C); _FL_DEFPIN(41, 9, C); _FL_DEFPIN(42, 19, A); _FL_DEFPIN(43, 20, A);
_FL_DEFPIN(44, 19, C); _FL_DEFPIN(45, 18, C); _FL_DEFPIN(46, 17, C); _FL_DEFPIN(47, 16, C);
_FL_DEFPIN(48, 15, C); _FL_DEFPIN(49, 14, C); _FL_DEFPIN(50, 13, C); _FL_DEFPIN(51, 12, C);
_FL_DEFPIN(52, 21, B); _FL_DEFPIN(53, 14, B); _FL_DEFPIN(54, 16, A); _FL_DEFPIN(55, 24, A);
_FL_DEFPIN(56, 23, A); _FL_DEFPIN(57, 22, A); _FL_DEFPIN(58, 6, A); _FL_DEFPIN(59, 4, A);
_FL_DEFPIN(60, 3, A); _FL_DEFPIN(61, 2, A); _FL_DEFPIN(62, 17, B); _FL_DEFPIN(63, 18, B);
_FL_DEFPIN(64, 19, B); _FL_DEFPIN(65, 20, B); _FL_DEFPIN(66, 15, B); _FL_DEFPIN(67, 16, B);
_FL_DEFPIN(68, 1, A); _FL_DEFPIN(69, 0, A); _FL_DEFPIN(70, 17, A); _FL_DEFPIN(71, 18, A);
_FL_DEFPIN(72, 30, C); _FL_DEFPIN(73, 21, A); _FL_DEFPIN(74, 25, A); _FL_DEFPIN(75, 26, A);
_FL_DEFPIN(76, 27, A); _FL_DEFPIN(77, 28, A); _FL_DEFPIN(78, 23, B);

// digix pins
_FL_DEFPIN(90, 0, B); _FL_DEFPIN(91, 1, B); _FL_DEFPIN(92, 2, B); _FL_DEFPIN(93, 3, B);
_FL_DEFPIN(94, 4, B); _FL_DEFPIN(95, 5, B); _FL_DEFPIN(96, 6, B); _FL_DEFPIN(97, 7, B);
_FL_DEFPIN(98, 8, B); _FL_DEFPIN(99, 9, B); _FL_DEFPIN(100, 5, A); _FL_DEFPIN(101, 22, B);
_FL_DEFPIN(102, 23, B); _FL_DEFPIN(103, 24, B); _FL_DEFPIN(104, 27, C); _FL_DEFPIN(105, 20, C);
_FL_DEFPIN(106, 11, C); _FL_DEFPIN(107, 10, C); _FL_DEFPIN(108, 21, A); _FL_DEFPIN(109, 30, C);
_FL_DEFPIN(110, 29, B); _FL_DEFPIN(111, 30, B); _FL_DEFPIN(112, 31, B); _FL_DEFPIN(113, 28, B);

#define SPI_DATA 75
#define SPI_CLOCK 76
#define ARM_HARDWARE_SPI
#define HAS_HARDWARE_PIN_SUPPORT

#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END


#endif // __INC_FASTPIN_ARM_SAM_H
