#ifndef __FASTPIN_ARM_CC3200_H
#define __FASTPIN_ARM_CC3200_H

#include <inc/hw_gpio.h>	//from CC3200 Energia library. Check your includes if you can't find it.
#include <inc/hw_memmap.h>

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else //FASTLED_FORCE_SOFTWARE_PINS

#if !defined(GPIO_O_GPIO_DATA) || !defined(GPIOA0_BASE)
#error ERROR: HWGPIO Not included. Try including hw_memmap.h and/or hw_gpio.h
#endif
//sets the register base from a port number. assumes hw_memmap.h included before this lib
//#define ULREG(P) ((P <= 3) ? (GPIOA0_BASE + 0x00001000*P) : GPIOA4_BASE)	//Returns the base reg address of the requested GPIO port
#define ADDRESSREF(A) (*(volatile uint32_t *)(A))

/// Template definition for Teensy 3.0 style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the individual port GPIO registers.  In theory, in some way, bit-band register access -should- be faster. However, in previous versions (i.e. Teensy LC)
/// something about the way gcc does register allocation results in the bit-band code being slower.  This is good enough for my purposes - DRR
/// The registers are data output, set output, clear output, toggle output, input, and direction
//Best if already initialized by pinmux tool, but just confirms the PINMUX settings anyways.

//CC3200 uses a special method to bitmask data reg. Essentially, the address bits 9:2 specify the mask, and written value specifies desired value
template<uint8_t PIN, uint32_t _MASK, typename _PDOR, typename _PDIR> class _ARMPIN {
public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;

  inline static void setOutput() { HWREG(_PDIR::r()) |= _MASK; } //ignoring MUX config.
  inline static void setInput() { HWREG(_PDIR::r()) &= ~(_MASK); } 

  inline static void hi() __attribute__ ((always_inline)) { HWREG(_PDOR::r() + (_MASK << 2)) = 0xFF; }
  inline static void lo() __attribute__ ((always_inline)) { HWREG(_PDOR::r() + (_MASK << 2)) = 0x00; }
  inline static void set(register port_t val) __attribute__ ((always_inline)) { HWREG(_PDOR::r() + (0x3FC)) = val; }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) { HWREG(_PDOR::r() + (_MASK << 2)) ^= _MASK; }

  inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *(port + (val << 2)) = 0xFF; }

  inline static port_t hival() __attribute__ ((always_inline)) { return HWREG(_PDOR::r()) | _MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return HWREG(_PDOR::r()) & ~(_MASK); }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_PDOR::r(); }
  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

// Macros for CC3200 pin access/definition
#ifndef HWREGB
#warning HW_types.h is not included
#endif

//couldn't get the following macros to work, so see below for long-form declarations
#define _R(L, T) struct __gen_struct_A ## L ## _ ## T
//#define _RD32(L, R) struct __gen_struct_A ## L ## _ ## R { static __attribute__((always_inline)) inline reg32_t r() { return GPIOA ## L ## _BASE + R ; } };	//returns the address, port(P) + offset (R)
//#define _IO32(L) _RD32(L, GPIO_O_GPIO_DATA); _RD32(L, GPIO_O_GPIO_DIR);
//CC3200 uses a special method to bitmask. Essentially, the address bits 9:2 specify the mask, and written value specifies desired value

//NOTE: could simplify to only PIN, but not sure PIN/8 will provide [0, 1, ..., 4]
#define _DEFPIN_ARM(PIN, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, (1 << (PIN % 8)), \
_R(L, GPIO_O_GPIO_DATA), _R(L, GPIO_O_GPIO_DIR)> {}

// Actual pin definitions
#if defined(FASTLED_CC3200)
//defines data structs for Ports A0-A4 (A4 is only available on CC3200 with GPIO32)

//_IO32(0); _IO32(1); _IO32(2); _IO32(3); _IO32(4);
struct __gen_struct_A0_GPIO_O_GPIO_DATA{ static __attribute__((always_inline)) inline reg32_t r() { return ADDRESSREF(GPIOA0_BASE + GPIO_O_GPIO_DATA); }};
struct __gen_struct_A0_GPIO_O_GPIO_DIR{ static __attribute__((always_inline)) inline reg32_t r() { return ADDRESSREF(GPIOA0_BASE + GPIO_O_GPIO_DIR); }};
struct __gen_struct_A1_GPIO_O_GPIO_DATA{ static __attribute__((always_inline)) inline reg32_t r() { return ADDRESSREF(GPIOA1_BASE + GPIO_O_GPIO_DATA); }};
struct __gen_struct_A1_GPIO_O_GPIO_DIR{ static __attribute__((always_inline)) inline reg32_t r() { return ADDRESSREF(GPIOA1_BASE + GPIO_O_GPIO_DIR); }};
struct __gen_struct_A2_GPIO_O_GPIO_DATA{ static __attribute__((always_inline)) inline reg32_t r() { return ADDRESSREF(GPIOA2_BASE + GPIO_O_GPIO_DATA); }};
struct __gen_struct_A2_GPIO_O_GPIO_DIR{ static __attribute__((always_inline)) inline reg32_t r() { return ADDRESSREF(GPIOA2_BASE + GPIO_O_GPIO_DIR); }};
struct __gen_struct_A3_GPIO_O_GPIO_DATA{ static __attribute__((always_inline)) inline reg32_t r() { return ADDRESSREF(GPIOA3_BASE + GPIO_O_GPIO_DATA); }};
struct __gen_struct_A3_GPIO_O_GPIO_DIR{ static __attribute__((always_inline)) inline reg32_t r() { return ADDRESSREF(GPIOA3_BASE + GPIO_O_GPIO_DIR); }};



#define MAX_PIN 27
_DEFPIN_ARM(0, 0); 	_DEFPIN_ARM(1, 0);	_DEFPIN_ARM(2, 0);	_DEFPIN_ARM(3, 0);
_DEFPIN_ARM(4, 0); 	_DEFPIN_ARM(5, 0);	_DEFPIN_ARM(6, 0);	_DEFPIN_ARM(7, 0);
_DEFPIN_ARM(8, 1); 	_DEFPIN_ARM(9, 1);	_DEFPIN_ARM(10, 1);	_DEFPIN_ARM(11, 1);
_DEFPIN_ARM(12, 1);	_DEFPIN_ARM(13, 1);	_DEFPIN_ARM(14, 1);	_DEFPIN_ARM(15, 1);
_DEFPIN_ARM(16, 2);	_DEFPIN_ARM(17, 2);	_DEFPIN_ARM(22, 2);	_DEFPIN_ARM(23, 2);
_DEFPIN_ARM(24, 3);	_DEFPIN_ARM(25, 3);	_DEFPIN_ARM(28, 3);	_DEFPIN_ARM(29, 3);
_DEFPIN_ARM(30, 3);
//Last pin used on CC3200mod is GPIO30. Don't assume able to access A4 (GPIO32). Might not be available...

_DEFPIN_ARM(31, 3);	_DEFPIN_ARM(32, 4);

//TODO: CHECK THESE VALUES. Only uncomment if fastspi_arm_cc3200 is finished.
//#define SPI_DATA 7		//pin 7 on CC3200mod. Same on CC3200. Assuming default mux. MOSI
//#define SPI_CLOCK 5		//pin 5 on CC3200mod. Same on CC3200. Assuming default mux

#define HAS_HARDWARE_PIN_SUPPORT
#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END

#endif // __INC_FASTPIN_ARM_CC3200
