#ifndef __FASTPIN_ARM_NRF52_H
#define __FASTPIN_ARM_NRF52_H

#include "fl/force_inline.h"
// Include fastpin_base.h to get FastPin base template
// This reopens namespace fl but typedefs will still be in scope
#include "fl/fastpin_base.h"


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
// Therefore, ideally, the _FL_DEFPIN() macro would simply
// conditionally pass either NRF_P0 or NRF_P1 to the underlying
// FastPin<> template class class.
//
// The "pin" provided to the FastLED<> template (and which
// the _FL_DEFPIN() macro specializes for valid pins) is NOT
// the microcontroller port.pin, but the Arduino digital pin.
// Some boards have an identity mapping (e.g., nRF52832 Feather)
// but most do not.  Therefore, the _FL_DEFPIN() macro
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
// These are defined at global scope so they can be used by macros outside the namespace
struct __generated_struct_NRF_P0 {
    FASTLED_NRF52_INLINE_ATTRIBUTE constexpr static uintptr_t r() {
        return NRF_P0_BASE;
    }
};
// Not all NRF52 chips have two ports.  Only define if P1 is present.
#if defined(NRF_P1_BASE)
struct __generated_struct_NRF_P1 {
    FASTLED_NRF52_INLINE_ATTRIBUTE constexpr static uintptr_t r() {
        return NRF_P1_BASE;
    }
};
#endif

namespace fl {

// The actual class template can then use a typename, for what is essentially a constexpr NRF_GPIO_Type*
template <uint32_t _MASK, typename _PORT, uint8_t _PORT_NUMBER, uint8_t _PIN_NUMBER> class _ARMPIN : public ValidPinBase {
public:
    typedef volatile uint32_t * port_ptr_t;
    typedef uint32_t port_t;

    FASTLED_NRF52_INLINE_ATTRIBUTE static void       setOutput() {
        // OK for this to be more than one instruction, as unusual to quickly switch input/output modes
        nrf_gpio_cfg(
            nrf_pin(),
            NRF_GPIO_PIN_DIR_OUTPUT,        // set pin as output
            NRF_GPIO_PIN_INPUT_DISCONNECT,  // disconnect the input buffering
            NRF_GPIO_PIN_NOPULL,            // neither pull-up nor pull-down resistors enabled
            NRF_GPIO_PIN_H0H1,              // high drive mode required for faster speeds
            NRF_GPIO_PIN_NOSENSE            // pin sense level disabled
            );
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void       setInput()  {
        // OK for this to be more than one instruction, as unusual to quickly switch input/output modes
        nrf_gpio_cfg(
            nrf_pin(),
            NRF_GPIO_PIN_DIR_INPUT,         // set pin as input
            NRF_GPIO_PIN_INPUT_DISCONNECT,  // disconnect the input buffering
            NRF_GPIO_PIN_NOPULL,            // neither pull-up nor pull-down resistors enabled
            NRF_GPIO_PIN_H0H1,              // high drive mode required for faster speeds
            NRF_GPIO_PIN_NOSENSE            // pin sense level disabled
            );
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void       hi()        { (reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUTSET = _MASK;            } // sets _MASK in the SET   OUTPUT register (output set high) // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static void       lo()        { (reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUTCLR = _MASK;            } // sets _MASK in the CLEAR OUTPUT register (output set low) // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static void       toggle()    { (reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUT ^= _MASK;              } // toggles _MASK bits in the OUTPUT GPIO port directly // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static void       strobe()    { toggle();     toggle();                } // BUGBUG -- Is this used by FastLED?  Without knowing (for example) SPI Speed?
    FASTLED_NRF52_INLINE_ATTRIBUTE static port_t     hival()     { return (reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUT | _MASK;        } // sets all _MASK bit(s) in the OUTPUT GPIO port to 1 // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static port_t     loval()     { return (reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUT & ~_MASK;       } // sets all _MASK bit(s) in the OUTPUT GPIO port to 0 // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static port_ptr_t port()      { return &((reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUT);             } // gets raw pointer to OUTPUT          GPIO port // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static port_ptr_t cport()     { return &((reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUTCLR);          } // gets raw pointer to SET   DIRECTION GPIO port // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static port_ptr_t sport()     { return &((reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUTSET);          } // gets raw pointer to CLEAR DIRECTION GPIO port // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static port_t     mask()      { return _MASK;                          } // gets the value of _MASK
    FASTLED_NRF52_INLINE_ATTRIBUTE static void hi (FASTLED_REGISTER port_ptr_t port) { hi();                      } // sets _MASK in the SET   OUTPUT register (output set high)
    FASTLED_NRF52_INLINE_ATTRIBUTE static void lo (FASTLED_REGISTER port_ptr_t port) { lo();                      } // sets _MASK in the CLEAR OUTPUT register (output set low)
    FASTLED_NRF52_INLINE_ATTRIBUTE static void set(FASTLED_REGISTER port_t     val ) { (reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUT = val;     } // sets entire port's value (optimization used by FastLED) // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) { *port = val; }
    constexpr                      static uint32_t   nrf_pin2() { return NRF_GPIO_PIN_MAP(_PORT_NUMBER, _PIN_NUMBER); }
    constexpr                      static bool       LowSpeedOnlyRecommended() {
        // Caller must always determine if high speed use if allowed on a given pin,
        // because it depends on more than just the chip packaging ... it depends on entire board (and even system) design.
        return false; // choosing default to be FALSE, to allow users to ATTEMPT to use high-speed on pins where support is not known
    }
    // Expose the nrf pin (port/pin combined), port, and pin as properties (e.g., for setting up SPI)

    FASTLED_NRF52_INLINE_ATTRIBUTE static uint32_t   nrf_pin()  { return NRF_GPIO_PIN_MAP(_PORT_NUMBER, _PIN_NUMBER); }
};


template <uint32_t _MASK, typename _PORT, uint8_t _PORT_NUMBER, uint8_t _PIN_NUMBER>
class _INVALID_ARMPIN: public _ARMPIN<_MASK, _PORT, _PORT_NUMBER, _PIN_NUMBER> {
public:
    _INVALID_ARMPIN() {
        Serial.print("For whatever reason pin "); Serial.print(_PIN_NUMBER);
        Serial.println(" has been marked as invalid. Please use a different pin.");
        Serial.println("Pausing execution for 2 seconds.");
        delay(2000);  // Give time for the message to be printed.
    }

};

}  // namespace fl


#define HAS_HARDWARE_PIN_SUPPORT

// Include variant-specific pin definitions
#include "fastpin_arm_nrf52_variants.h" // allow-include-after-namespace

#endif // #ifndef __FASTPIN_ARM_NRF52_H
