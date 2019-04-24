#ifndef __FASTPIN_ARM_NRF52_H
#define __FASTPIN_ARM_NRF52_H

#if defined(FASTLED_NRF52_NEVER_INLINE)
    #define FASTLED_NRF52_INLINE_ATTRIBUTE __attribute__((noinline))
#elif defined(FASTLED_NRF52_DO_NOT_FORCE_INLINE)
    #define FASTLED_NRF52_INLINE_FUNCTION  inline
#else
    // default is to force inlining, even when optimizations are disabled
    #define FASTLED_NRF52_INLINE_ATTRIBUTE __attribute__((always_inline)) inline
#endif

/*
//
// Background:
// ===========
// the nRF52 has more than 32 ports, and thus must support
// two distinct GPIO port registers.  
//
// For the nRF52 series, the structure to control the port is
// `NRF_GPIO_Type`, with separate addresses mapped for set, clear, etc.
// The two ports are defined as NRF_P0 and NRF_P1.
// An example declaration for the ports is:
//     #define NRF_P0_BASE   0x50000000UL
//     #define NRF_P1_BASE   0x50000300UL
//     #define NRF_P0        ((NRF_GPIO_Type*)NRF_P0_BASE)
//     #define NRF_P1        ((NRF_GPIO_Type*)NRF_P1_BASE)
//
// Therefore, ideally, the _DEFPIN_ARM() macro would simply
// conditionally pass either NRF_P0 or NRF_P1 to the underlying
// FastPin<> template class class.
//
// The "pin" provided to the FastLED<> template (and which
// the _DEFPIN_ARM() macro specializes for valid pins) is NOT
// the microcontroller port.pin, but the Arduino digital pin.
// Some boards have an identity mapping (e.g., nRF52832 Feather)
// but most do not.  Therefore, the _DEFPIN_ARM() macro
// must translate the Arduino pin to the mcu port.pin.
//
// 
// Difficulties:
// =============
// The goal is to avoid any such lookups, using compile-time
// optimized functions for speed, in line with FastLED's
// overall design goals. This means constexpr, compile-time
// and aggressive inlining of functions....
//
// Right away, this precludes the use of g_ADigitalPinMap,
// which is not constexpr, and thus not available for
// preprocessor/compile-time optimizations.  Therefore,
// we have to specialize FastPin<uint8_t PIN>, given a
// compile-time value for PIN, into at least a PORT and
// a BITMASK for the port.
//
// Arduino compiles using C++11 for at least Feather nRF52840 Express.
// C++11 is very restrictive about template parameters.
// Template parameters can only be:
// 1. a type (as most people expect)
// 2. a template
// 3. a constexpr native integer type
//
// Therefore, attempts to use `NRF_GPIO_Type *` as a
// template parameter will fail....
//
// Solution:
// =========
// The solution chosen is to define a unique structure for each port,
// whose SOLE purpose is to have a static inline function that
// returns the `NRF_GPIO_Type *` that is needed.
//
// Thus, while it's illegal to pass `NRF_P0` as a template
// parameter, it's perfectly legal to pass `__generated_struct_NRF_P0`,
// and have the template call a well-known `static inline` function
// that returns `NRF_P0` ... which is itself a compile-time constant.
//
// Note that additional magic can be applied that will automatically
// generate the structures.  If you want to add that to this platform,
// check out the KL26 platform files for a starting point.
//
*/

// manually define two structures, to avoid fighting with preprocessor macros
struct __generated_struct_NRF_P0 {
    FASTLED_NRF52_INLINE_ATTRIBUTE constexpr static NRF_GPIO_Type * r() {
        return NRF_P0;
    }
};
struct __generated_struct_NRF_P1 {
    FASTLED_NRF52_INLINE_ATTRIBUTE constexpr static NRF_GPIO_Type * r() {
        return NRF_P1;
    }
};


// The actual class template can then use a typename, for what is essentially a constexpr NRF_GPIO_Type*
template <uint32_t _MASK, typename _PORT> class _ARMPIN  {
public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;

  FASTLED_NRF52_INLINE_ATTRIBUTE static void       setOutput() { _PORT::r()->DIRSET = _MASK;            } // sets _MASK in the SET   DIRECTION register (set to output)
  FASTLED_NRF52_INLINE_ATTRIBUTE static void       setInput()  { _PORT::r()->DIRCLR = _MASK;            } // sets _MASK in the CLEAR DIRECTION register (set to input)
  FASTLED_NRF52_INLINE_ATTRIBUTE static void       hi()        { _PORT::r()->OUTSET = _MASK;            } // sets _MASK in the SET   OUTPUT register (output set high)
  FASTLED_NRF52_INLINE_ATTRIBUTE static void       lo()        { _PORT::r()->OUTCLR = _MASK;            } // sets _MASK in the CLEAR OUTPUT register (output set low)
  FASTLED_NRF52_INLINE_ATTRIBUTE static void       toggle()    { _PORT::r()->OUT ^= _MASK;              } // toggles _MASK bits in the OUTPUT GPIO port directly
  FASTLED_NRF52_INLINE_ATTRIBUTE static void       strobe()    { toggle();     toggle();                } // BUGBUG -- Is this used by FastLED?  Without knowing (for example) SPI Speed?
  FASTLED_NRF52_INLINE_ATTRIBUTE static port_t     hival()     { return _PORT::r()->OUT | _MASK;        } // sets all _MASK bit(s) in the OUTPUT GPIO port to 1
  FASTLED_NRF52_INLINE_ATTRIBUTE static port_t     loval()     { return _PORT::r()->OUT & ~_MASK;       } // sets all _MASK bit(s) in the OUTPUT GPIO port to 0
  FASTLED_NRF52_INLINE_ATTRIBUTE static port_ptr_t port()      { return &(_PORT::r()->OUT);             } // gets raw pointer to OUTPUT          GPIO port
  FASTLED_NRF52_INLINE_ATTRIBUTE static port_ptr_t cport()     { return &(_PORT::r()->OUTCLR);          } // gets raw pointer to SET   DIRECTION GPIO port
  FASTLED_NRF52_INLINE_ATTRIBUTE static port_ptr_t sport()     { return &(_PORT::r()->OUTSET);          } // gets raw pointer to CLEAR DIRECTION GPIO port
  FASTLED_NRF52_INLINE_ATTRIBUTE static port_t     mask()      { return _MASK;                          } // gets the value of _MASK
  FASTLED_NRF52_INLINE_ATTRIBUTE static void hi(register port_ptr_t port) { hi();  } // sets _MASK in the SET   OUTPUT register (output set high)
  FASTLED_NRF52_INLINE_ATTRIBUTE static void lo(register port_ptr_t port) { lo();  } // sets _MASK in the CLEAR OUTPUT register (output set low)
  FASTLED_NRF52_INLINE_ATTRIBUTE static void set(register port_t val    ) { _PORT::r()->OUT = val; }
  FASTLED_NRF52_INLINE_ATTRIBUTE static void fastset(register port_ptr_t port, register port_t val) { *port = val; }
};

//
// BOARD_PIN can be either the pin portion of a port.pin, or the combined NRF_GPIO_PIN_MAP() number.
// For example both the following two defines refer to P1.15 (pin 47) as Arduino pin 3:
//     _DEFPIN_ARM(3, 1, 15);
//     _DEFPIN_ARM(3, 1, 47);
//
// Similarly, the following defines are all equivalent:
//     _DEFPIN_ARM_IDENTITY_P1(47);
//     _DEFPIN_ARM(47, 1, 15);
//     _DEFPIN_ARM(47, 1, 47);
//

#define _DEFPIN_ARM_IDENTITY_P0(ARDUINO_PIN) \
    template<> class FastPin<ARDUINO_PIN> :  \
    public _ARMPIN<                          \
        1u << (ARDUINO_PIN & 31u),           \
        __generated_struct_NRF_P0            \
        >                                    \
    {}

#define _DEFPIN_ARM_IDENTITY_P1(ARDUINO_PIN) \
    template<> class FastPin<ARDUINO_PIN> :  \
    public _ARMPIN<                          \
        1u << (ARDUINO_PIN & 31u),           \
        __generated_struct_NRF_P1            \
        >                                    \
    {}

#define _DEFPIN_ARM(ARDUINO_PIN, BOARD_PORT, BOARD_PIN)  \
    template<> class FastPin<ARDUINO_PIN> :              \
    public _ARMPIN<                                      \
        1u << (BOARD_PIN & 31u),                         \
        __generated_struct_NRF_P ## BOARD_PORT           \
        >                                                \
    {}

// The actual pin definitions are in a separate header file...
#include "fastpin_arm_nrf52_variants.h"

#define HAS_HARDWARE_PIN_SUPPORT

#endif // #ifndef __FASTPIN_ARM_NRF52_H
