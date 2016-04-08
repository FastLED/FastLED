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

/// Template definition for CC3200 style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the individual port GPIO registers.  In theory, in some way, bit-band register access -should- be faster. However, in previous versions (i.e. Teensy LC)
/// something about the way gcc does register allocation results in the bit-band code being slower.  This is good enough for my purposes - DRR
/// The registers are data output, set output, clear output, toggle output, input, and direction
//Best if already initialized by pinmux tool, but just confirms the PINMUX settings anyways.

//CC3200 uses a special method to bitmask data reg. Essentially, the address bits 9:2 specify the mask, and written value specifies desired value
template<uint8_t PIN, uint32_t _MASK, typename _REGBASE> class _ARMPIN {
public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;

  inline static void setOutput() { MAP_GPIODirModeSet(_REGBASE::r(), _MASK, GPIO_DIR_MODE_OUT); } //ignoring MUX config.
  inline static void setInput() { MAP_GPIODirModeSet(_REGBASE::r(), _MASK, GPIO_DIR_MODE_IN); }

  inline static void hi() __attribute__ ((always_inline)) { HWREG(_REGBASE::r() + (_MASK << 2)) = 0xFF; }
  inline static void lo() __attribute__ ((always_inline)) { HWREG(_REGBASE::r() + (_MASK << 2)) = 0x00; }
  inline static void set(register port_t val) __attribute__ ((always_inline)) { HWREG(_REGBASE::r() + (0xFF << 2)) = (val & 0xFF); }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) { HWREG(_REGBASE::r() + (_MASK << 2)) ^= _MASK; }

  inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *(port + (val << 2)) = 0xFF; }

  inline static port_t hival() __attribute__ ((always_inline)) { return HWREG(_REGBASE::r() + (0xFF << 2)) | _MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return HWREG(_REGBASE::r() + (0xFF << 2)) & ~(_MASK); }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return (port_ptr_t)_REGBASE::r(); }
  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

// Macros for CC3200 pin access/definition
#ifndef HWREGB
#warning "HW_types.h is not included"
#endif

//couldn't get the following macros to work, so see below for long-form declarations
#define _R(L) struct __gen_struct_A ## L ## _BASE
//returns the address using class::r()
#define _RD32(L) struct __gen_struct_A ## L ## _BASE  { static __attribute__((always_inline)) inline uint32_t r() { return GPIOA ## L ## _BASE; } };

//NOTE: could simplify to only PIN, but not sure PIN/8 will provide [0, 1, ..., 4]
#define _DEFPIN_ARM(PIN, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, (1 << (PIN % 8)), _R(L)> {}

// Actual pin definitions
#if defined(FASTLED_CC3200)
//defines data structs for Ports A0-A4 (A4 is only available on CC3200 with GPIO32)

_RD32(0); _RD32(1); _RD32(2); _RD32(3); _RD32(4);

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

//note: these are pin values (referring to the pads on the CC3200 (non-module). These are not GPIO pins.
#define SPI_DATA 7		
//pin 7 on CC3200mod, or GPIO14. Same on CC3200. One of two possible muxes, other is SPI2_DATA. MOSI
#define SPI_CLOCK 5		
//pin 5 on CC3200mod, or GPIO16. Same on CC3200. One of two possible muxes, other is SPI2_CLOCK
#define SPI2_DATA 52	
//pin 52, GPIO32. Not present on CC3200mod.
#define SPI2_CLOCK 45	
//pin 45, GPIO31. Not present on CC3200mod.

#define HAS_HARDWARE_PIN_SUPPORT
#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END

#endif // __INC_FASTPIN_ARM_CC3200
