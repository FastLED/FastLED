#pragma once
#include <stdint.h>

#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN


#define _R(T) struct __gen_struct_ ## T
#define _FL_DEFPIN(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1 << BIT, _R(GPIO ## L)> {};

/// Template definition for STM32 style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
/// The registers are data output, set output, clear output, toggle output, input, and direction

template<uint8_t PIN, uint8_t _BIT, uint32_t _MASK, typename _GPIO> class _ARMPIN {

public:
    typedef volatile uint32_t * port_ptr_t;
    typedef uint32_t port_t;

    #if 0
    inline static void setOutput() {
        if(_BIT<8) {
            _CRL::r() = (_CRL::r() & (0xF << (_BIT*4)) | (0x1 << (_BIT*4)));
        } else {
            _CRH::r() = (_CRH::r() & (0xF << ((_BIT-8)*4))) | (0x1 << ((_BIT-8)*4));
        }
    }
    inline static void setInput() { /* TODO */ } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }
    #endif

    inline static void setOutput() { pinMode(PIN, OUTPUT); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
    inline static void setInput() { pinMode(PIN, INPUT); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

#if defined(STM32F2XX)
    inline static void hi() __attribute__ ((always_inline)) { _GPIO::r()->BSRRL = _MASK; }
    inline static void lo() __attribute__ ((always_inline)) { _GPIO::r()->BSRRH = _MASK; }
#else
    inline static void hi() __attribute__ ((always_inline)) { _GPIO::r()->BSRR = _MASK; }
    inline static void lo() __attribute__ ((always_inline)) { _GPIO::r()->BRR = _MASK; }
    // inline static void lo() __attribute__ ((always_inline)) { _GPIO::r()->BSRR = (_MASK<<16); }
#endif
    inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { _GPIO::r()->ODR = val; }

    inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

    inline static void toggle() __attribute__ ((always_inline)) { if(_GPIO::r()->ODR & _MASK) { lo(); } else { hi(); } }

    inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

    inline static port_t hival() __attribute__ ((always_inline)) { return _GPIO::r()->ODR | _MASK; }
    inline static port_t loval() __attribute__ ((always_inline)) { return _GPIO::r()->ODR & ~_MASK; }
    inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_GPIO::r()->ODR; }

#if defined(STM32F2XX)
    inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPIO::r()->BSRRL; }
    inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPIO::r()->BSRRH; }
#else
    inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPIO::r()->BSRR; }
    inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPIO::r()->BRR; }
#endif

    inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};



FASTLED_NAMESPACE_END
