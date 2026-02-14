

// ok no namespace fl
#pragma once

#include "fl/stl/stdint.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

template<fl::u8 PIN, fl::u8 _MASK, typename _PORT, typename _DDR, typename _PIN>
class _AVRPIN {
public:
    typedef volatile fl::u8* port_ptr_t;
    typedef fl::u8 port_t;

    inline static void setOutput() { _DDR::r() |= _MASK; }
    inline static void setInput() { _DDR::r() &= ~_MASK; }

    inline static void hi() __attribute__((always_inline)) { _PORT::r() |= _MASK; }
    inline static void lo() __attribute__((always_inline)) { _PORT::r() &= ~_MASK; }
    inline static void set(FASTLED_REGISTER fl::u8 val) __attribute__((always_inline)) { _PORT::r() = val; }

    inline static void strobe() __attribute__((always_inline)) { toggle(); toggle(); }

    inline static void toggle() __attribute__((always_inline)) { _PIN::r() = _MASK; }

    inline static void hi(FASTLED_REGISTER port_ptr_t /*port*/) __attribute__((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t /*port*/) __attribute__((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t /*port*/, FASTLED_REGISTER fl::u8 val) __attribute__((always_inline)) { set(val); }

    inline static port_t hival() __attribute__((always_inline)) { return _PORT::r() | _MASK; }
    inline static port_t loval() __attribute__((always_inline)) { return _PORT::r() & ~_MASK; }
    inline static port_ptr_t port() __attribute__((always_inline)) { return &_PORT::r(); }

    inline static port_t mask() __attribute__((always_inline)) { return _MASK; }

    static constexpr bool validpin() { return true; }
};

FL_DISABLE_WARNING_POP
