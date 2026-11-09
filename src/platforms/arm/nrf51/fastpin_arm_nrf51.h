// IWYU pragma: private

#ifndef __FASTPIN_ARM_NRF51_H
#define __FASTPIN_ARM_NRF51_H

#include "fl/stl/compiler_control.h"
#include "fl/stl/static_assert.h"
#include "platforms/arm/nrf51/is_nrf51.h"

// nRF51 has no Arduino port-register map. Define this before fastpin_base.h
// so a forced FastPin is NoPinMapPin whichever nRF51 header comes first.
#if defined(FL_IS_NRF51) && !defined(FASTLED_NO_PINMAP)
#define FASTLED_NO_PINMAP
#endif
#include "fl/system/fastpin_base.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

#if defined(FL_IS_NRF51)
// IWYU pragma: begin_keep
#include <nrf51.h>  // vendor CMSIS: NRF_GPIO / NRF_GPIO_Type
#include <nrf51_bitfields.h>  // GPIO_PIN_CNF_INPUT_Msk
// IWYU pragma: end_keep

namespace fl {

/// nRF51 GPIO pin, driven through the vendor CMSIS `NRF_GPIO` register block
/// (OUT/OUTSET/OUTCLR/IN/DIRSET/DIRCLR).
template<fl::u8 PIN, fl::u32 _MASK> class _ARMPIN : public ValidPinBase {
public:
    typedef volatile fl::u32 * port_ptr_t;
    typedef fl::u32 port_t;

    inline static void setOutput() { NRF_GPIO->DIRSET = _MASK; }
    inline static void setInput() {
        NRF_GPIO->DIRCLR = _MASK;
        // Reconnect the input buffer (PIN_CNF.INPUT = Connect = 0) in case an
        // earlier analogWrite()/pinMode() disconnected it, so isset() reads.
        // PIN_CNF is read-modify-written: mask IRQs so an ISR (e.g. enabling
        // SENSE for wake) cannot be undone by a stale write (#5188). Restores
        // the prior PRIMASK, so it is safe with IRQs already off.
        const fl::u32 primask = __get_PRIMASK();
        __disable_irq();
        NRF_GPIO->PIN_CNF[PIN] &= ~GPIO_PIN_CNF_INPUT_Msk;
        __set_PRIMASK(primask);
    }

    inline static void hi() __attribute__ ((always_inline)) { NRF_GPIO->OUTSET = _MASK; }
    inline static void lo() __attribute__ ((always_inline)) { NRF_GPIO->OUTCLR = _MASK; }
    inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { NRF_GPIO->OUT = val; }

    inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

    // Read OUT, then write only this pin through OUTCLR/OUTSET. A plain
    // `OUT ^= mask` writes the whole port back and reverts any other pin an
    // interrupt changed between the load and the store (#5047).
    inline static void toggle() __attribute__ ((always_inline)) {
        if (NRF_GPIO->OUT & _MASK) { NRF_GPIO->OUTCLR = _MASK; } else { NRF_GPIO->OUTSET = _MASK; }
    }

    inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

    inline static port_t hival() __attribute__ ((always_inline)) { return NRF_GPIO->OUT | _MASK; }
    inline static port_t loval() __attribute__ ((always_inline)) { return NRF_GPIO->OUT & ~_MASK; }
    inline static port_ptr_t port() __attribute__ ((always_inline)) { return &NRF_GPIO->OUT; }
    inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }

    inline static bool isset() __attribute__ ((always_inline)) { return (NRF_GPIO->IN & _MASK) != 0; }
};

/// Native register pin. The clockless and SPI0 drivers use it so they keep
/// native GPIO when FASTLED_FORCE_SOFTWARE_PINS replaces FastPin (#5069).
template <fl::u8 PIN> class Nrf51HwPin {
public:
    // Instantiated only for pins outside 0..31. Stubs keep the static
    // assertion the only diagnostic.
    typedef volatile fl::u32* port_ptr_t;
    typedef fl::u32 port_t;
    static void setOutput() {}
    static void hi() {}
    static void lo() {}
    static void toggle() {}
    static port_t mask() { return 0; }
    static port_ptr_t port() { return nullptr; }
    FL_STATIC_ASSERT(PIN == 255 && PIN != 255,
                     "nRF51 has GPIO 0..31 only; this pin cannot be driven natively.");
};

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
// No FastPin specialization: fl::FastPin is the forced software primary
// template from fastpin_base.h (NoPinMapPin, since FASTLED_NO_PINMAP is set).
#define _FL_NRF51_FASTPIN(PIN)
#else
#define _FL_NRF51_FASTPIN(PIN) template<> class FastPin<PIN> : public Nrf51HwPin<PIN> {};
#endif

#define _FL_DEFPIN(PIN) \
    template<> class Nrf51HwPin<PIN> : public _ARMPIN<PIN, (fl::u32)1 << PIN> {}; \
    _FL_NRF51_FASTPIN(PIN)

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

}  // namespace fl

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT
#else
#define HAS_HARDWARE_PIN_SUPPORT
#endif

#endif  // FL_IS_NRF51


FL_DISABLE_WARNING_POP

#endif
