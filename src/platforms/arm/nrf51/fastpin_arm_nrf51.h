// IWYU pragma: private

#ifndef __FASTPIN_ARM_NRF51_H
#define __FASTPIN_ARM_NRF51_H

#include "fl/stl/compiler_control.h"
#include "fl/system/fastpin_base.h"
#include "platforms/arm/nrf51/is_nrf51.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

#if defined(FL_IS_NRF51)
// IWYU pragma: begin_keep
#include <nrf51.h>  // vendor CMSIS: NRF_GPIO / NRF_GPIO_Type
// IWYU pragma: end_keep

namespace fl {

/// nRF51 GPIO pin, driven through the vendor CMSIS `NRF_GPIO` register block
/// (OUT/OUTSET/OUTCLR/IN/DIRSET/DIRCLR).
template<fl::u8 PIN, fl::u32 _MASK> class _ARMPIN : public ValidPinBase {
public:
    typedef volatile fl::u32 * port_ptr_t;
    typedef fl::u32 port_t;

    inline static void setOutput() { NRF_GPIO->DIRSET = _MASK; }
    inline static void setInput() { NRF_GPIO->DIRCLR = _MASK; }

    inline static void hi() __attribute__ ((always_inline)) { NRF_GPIO->OUTSET = _MASK; }
    inline static void lo() __attribute__ ((always_inline)) { NRF_GPIO->OUTCLR = _MASK; }
    inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { NRF_GPIO->OUT = val; }

    inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

    inline static void toggle() __attribute__ ((always_inline)) { NRF_GPIO->OUT ^= _MASK; }

    inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

    inline static port_t hival() __attribute__ ((always_inline)) { return NRF_GPIO->OUT | _MASK; }
    inline static port_t loval() __attribute__ ((always_inline)) { return NRF_GPIO->OUT & ~_MASK; }
    inline static port_ptr_t port() __attribute__ ((always_inline)) { return &NRF_GPIO->OUT; }
    inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }

    inline static bool isset() __attribute__ ((always_inline)) { return (NRF_GPIO->IN & _MASK) != 0; }
};

#define _FL_DEFPIN(PIN) template<> class FastPin<PIN> : public _ARMPIN<PIN, (fl::u32)1 << PIN> {};

// Actual pin definitions
#define MAX_PIN 31
_FL_DEFPIN(0); _FL_DEFPIN(1); _FL_DEFPIN(2); _FL_DEFPIN(3);
_FL_DEFPIN(4); _FL_DEFPIN(5); _FL_DEFPIN(6); _FL_DEFPIN(7);
_FL_DEFPIN(8); _FL_DEFPIN(9); _FL_DEFPIN(10); _FL_DEFPIN(11);
_FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14); _FL_DEFPIN(15);
_FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23);
_FL_DEFPIN(24); _FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27);
_FL_DEFPIN(28); _FL_DEFPIN(29); _FL_DEFPIN(30); _FL_DEFPIN(31);

#define HAS_HARDWARE_PIN_SUPPORT

}  // namespace fl

#endif  // FL_IS_NRF51


FL_DISABLE_WARNING_POP

#endif
