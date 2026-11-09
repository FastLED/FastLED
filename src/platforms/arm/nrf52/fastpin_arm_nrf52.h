// IWYU pragma: private

#ifndef __FASTPIN_ARM_NRF52_H
#define __FASTPIN_ARM_NRF52_H

#include "fl/stl/compiler_control.h"
#include "fl/stl/static_assert.h"
// Include fastpin_base.h to get FastPin base template
// This reopens namespace fl but typedefs will still be in scope
#include "fl/system/fastpin_base.h"
#include "platforms/arm/nrf52/nrf52_pin.h"


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
    FASTLED_NRF52_INLINE_ATTRIBUTE constexpr static uintptr_t r() FL_NO_EXCEPT {
        return NRF_P0_BASE;
    }
};
// Not all NRF52 chips have two ports.  Only define if P1 is present.
#if defined(NRF_P1_BASE)
struct __generated_struct_NRF_P1 {
    FASTLED_NRF52_INLINE_ATTRIBUTE constexpr static uintptr_t r() FL_NO_EXCEPT {
        return NRF_P1_BASE;
    }
};
#endif

namespace fl {

template <u32 _MASK, typename _PORT, u8 _PORT_NUMBER, u8 _PIN_NUMBER>
class _INVALID_ARMPIN: public _ARMPIN<_MASK, _PORT, _PORT_NUMBER, _PIN_NUMBER> {
public:
    _INVALID_ARMPIN() {
        Serial.print("For whatever reason pin "); Serial.print(_PIN_NUMBER);
        Serial.println(" has been marked as invalid. Please use a different pin.");
        Serial.println("Pausing execution for 2 seconds.");
        delay(2000);  // Give time for the message to be printed.
    }

};

/// Native register pin for an Arduino pin, specialized for every pin in the
/// variant table. Peripheral drivers (SPIM, PWM) use it so they keep native
/// GPIO and PSEL routing when FASTLED_FORCE_SOFTWARE_PINS replaces FastPin.
template <u8 PIN> class Nrf52HwPin {
    // Instantiated only for pins missing from the variant table.
    FL_STATIC_ASSERT(PIN == 255 && PIN != 255,
                     "This pin is not in the nRF52 variant pin table, so the PWM and SPIM drivers cannot route it. Use a pin listed in fastpin_arm_nrf52_variants.h for your board.");
};

/// True when Nrf52HwPin<PIN> names a real pad the PWM and SPIM drivers may
/// route. Invalid pins derive from _INVALID_ARMPIN<0, NRF_P0, 0, 0>, so routing
/// them would drive P0.00 (#5190). The derived-to-base pointer conversion is
/// preferred over the conversion to const void*.
template <u32 _MASK, typename _PORT, u8 _PORT_NUMBER, u8 _PIN_NUMBER>
constexpr bool nrf52_is_invalid_pin(const _INVALID_ARMPIN<_MASK, _PORT, _PORT_NUMBER, _PIN_NUMBER>*) FL_NO_EXCEPT { return true; }
constexpr bool nrf52_is_invalid_pin(const void*) FL_NO_EXCEPT { return false; }
template <u8 PIN> constexpr bool nrf52_pin_routable() FL_NO_EXCEPT {
    return !nrf52_is_invalid_pin(static_cast<const Nrf52HwPin<PIN>*>(nullptr));
}

}  // namespace fl


#if defined(FASTLED_FORCE_SOFTWARE_PINS)
// The variant table then emits only fl::Nrf52HwPin, so fl::FastPin uses
// the forced software primary template in fastpin_base.h (#5070).
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT
#else
#define HAS_HARDWARE_PIN_SUPPORT
#endif

// Include variant-specific pin definitions
#include "platforms/arm/nrf52/fastpin_arm_nrf52_variants.h" // allow-include-after-namespace
#include "fl/stl/noexcept.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER


FL_DISABLE_WARNING_POP

#endif // #ifndef __FASTPIN_ARM_NRF52_H
