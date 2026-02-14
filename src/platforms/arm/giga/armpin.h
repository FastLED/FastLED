#pragma once

// IWYU pragma: private
#include "fl/stl/stdint.h"
#include "fl/fastpin_base.h"
#include "fl/pin.h"  // For PinMode, PinValue enums
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
namespace fl {
#define _R(T) struct __gen_struct_ ## T
#define _FL_DEFPIN(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1 << BIT, _R(GPIO ## L)> {};

/// Template definition for STM32 style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
/// The registers are data output, set output, clear output, toggle output, input, and direction

template<u8 PIN, u8 _BIT, u32 _MASK, typename _GPIO> class _ARMPIN : public ValidPinBase {

public:
    typedef volatile u32 * port_ptr_t;
    typedef u32 port_t;

    inline static void setOutput() { pinMode(PIN, PinMode::Output); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
    inline static void setInput() { pinMode(PIN, PinMode::Input); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }
    
    inline static void hi() __attribute__ ((always_inline)) { _GPIO::r()->BSRR = _MASK; }
    inline static void lo() __attribute__ ((always_inline)) { _GPIO::r()->BSRR = (_MASK<<16); }

    inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { _GPIO::r()->ODR = val; }

    inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

    inline static void toggle() __attribute__ ((always_inline)) { if(_GPIO::r()->ODR & _MASK) { lo(); } else { hi(); } }

    inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

    inline static port_t hival() __attribute__ ((always_inline)) { return _GPIO::r()->ODR | _MASK; }
    inline static port_t loval() __attribute__ ((always_inline)) { return _GPIO::r()->ODR & ~_MASK; }
    inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_GPIO::r()->ODR; }

    inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPIO::r()->BSRR = (_MASK<<16); }
    inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPIO::r()->BSRR = _MASK; }

    inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};
}  // namespace fl

FL_DISABLE_WARNING_POP
