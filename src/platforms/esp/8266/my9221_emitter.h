#pragma once

// IWYU pragma: private
#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include <c_types.h> // IWYU pragma: keep

namespace fl {
namespace detail {

struct My9221EspPinFunctions {
    void (*hi)();
    void (*lo)();
    void (*toggle)();
};
template <int Pin> struct My9221EspPin;

// Explicit forwarding signatures for logical pins in fastpin_esp8266.h.
// No GPIO interpretation: all callbacks invoke the actual selected FastPin.
// RAW excludes flash pins6..11; D1 has0..15; NodeMCU has0..10.
// Forced software pins retain the Arduino GPIO-numbered0..16 route; its
// Arduino callees/cache placement need separate proof from native FastPin.
static FL_NO_INLINE void IRAM_ATTR my9221Pin0_hi() FL_NO_EXCEPT { fl::FastPin<0>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin0_lo() FL_NO_EXCEPT { fl::FastPin<0>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin0_toggle() FL_NO_EXCEPT { fl::FastPin<0>::toggle(); }
template <> struct My9221EspPin<0> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin0_hi, my9221Pin0_lo, my9221Pin0_toggle};
    }
};

static FL_NO_INLINE void IRAM_ATTR my9221Pin1_hi() FL_NO_EXCEPT { fl::FastPin<1>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin1_lo() FL_NO_EXCEPT { fl::FastPin<1>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin1_toggle() FL_NO_EXCEPT { fl::FastPin<1>::toggle(); }
template <> struct My9221EspPin<1> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin1_hi, my9221Pin1_lo, my9221Pin1_toggle};
    }
};

static FL_NO_INLINE void IRAM_ATTR my9221Pin2_hi() FL_NO_EXCEPT { fl::FastPin<2>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin2_lo() FL_NO_EXCEPT { fl::FastPin<2>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin2_toggle() FL_NO_EXCEPT { fl::FastPin<2>::toggle(); }
template <> struct My9221EspPin<2> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin2_hi, my9221Pin2_lo, my9221Pin2_toggle};
    }
};

static FL_NO_INLINE void IRAM_ATTR my9221Pin3_hi() FL_NO_EXCEPT { fl::FastPin<3>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin3_lo() FL_NO_EXCEPT { fl::FastPin<3>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin3_toggle() FL_NO_EXCEPT { fl::FastPin<3>::toggle(); }
template <> struct My9221EspPin<3> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin3_hi, my9221Pin3_lo, my9221Pin3_toggle};
    }
};

static FL_NO_INLINE void IRAM_ATTR my9221Pin4_hi() FL_NO_EXCEPT { fl::FastPin<4>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin4_lo() FL_NO_EXCEPT { fl::FastPin<4>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin4_toggle() FL_NO_EXCEPT { fl::FastPin<4>::toggle(); }
template <> struct My9221EspPin<4> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin4_hi, my9221Pin4_lo, my9221Pin4_toggle};
    }
};

static FL_NO_INLINE void IRAM_ATTR my9221Pin5_hi() FL_NO_EXCEPT { fl::FastPin<5>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin5_lo() FL_NO_EXCEPT { fl::FastPin<5>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin5_toggle() FL_NO_EXCEPT { fl::FastPin<5>::toggle(); }
template <> struct My9221EspPin<5> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin5_hi, my9221Pin5_lo, my9221Pin5_toggle};
    }
};

#if defined(FASTLED_FORCE_SOFTWARE_PINS) || !defined(FASTLED_ESP8266_RAW_PIN_ORDER)
static FL_NO_INLINE void IRAM_ATTR my9221Pin6_hi() FL_NO_EXCEPT { fl::FastPin<6>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin6_lo() FL_NO_EXCEPT { fl::FastPin<6>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin6_toggle() FL_NO_EXCEPT { fl::FastPin<6>::toggle(); }
template <> struct My9221EspPin<6> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin6_hi, my9221Pin6_lo, my9221Pin6_toggle};
    }
};
#endif

#if defined(FASTLED_FORCE_SOFTWARE_PINS) || !defined(FASTLED_ESP8266_RAW_PIN_ORDER)
static FL_NO_INLINE void IRAM_ATTR my9221Pin7_hi() FL_NO_EXCEPT { fl::FastPin<7>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin7_lo() FL_NO_EXCEPT { fl::FastPin<7>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin7_toggle() FL_NO_EXCEPT { fl::FastPin<7>::toggle(); }
template <> struct My9221EspPin<7> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin7_hi, my9221Pin7_lo, my9221Pin7_toggle};
    }
};
#endif

#if defined(FASTLED_FORCE_SOFTWARE_PINS) || !defined(FASTLED_ESP8266_RAW_PIN_ORDER)
static FL_NO_INLINE void IRAM_ATTR my9221Pin8_hi() FL_NO_EXCEPT { fl::FastPin<8>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin8_lo() FL_NO_EXCEPT { fl::FastPin<8>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin8_toggle() FL_NO_EXCEPT { fl::FastPin<8>::toggle(); }
template <> struct My9221EspPin<8> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin8_hi, my9221Pin8_lo, my9221Pin8_toggle};
    }
};
#endif

#if defined(FASTLED_FORCE_SOFTWARE_PINS) || !defined(FASTLED_ESP8266_RAW_PIN_ORDER)
static FL_NO_INLINE void IRAM_ATTR my9221Pin9_hi() FL_NO_EXCEPT { fl::FastPin<9>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin9_lo() FL_NO_EXCEPT { fl::FastPin<9>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin9_toggle() FL_NO_EXCEPT { fl::FastPin<9>::toggle(); }
template <> struct My9221EspPin<9> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin9_hi, my9221Pin9_lo, my9221Pin9_toggle};
    }
};
#endif

#if defined(FASTLED_FORCE_SOFTWARE_PINS) || !defined(FASTLED_ESP8266_RAW_PIN_ORDER)
static FL_NO_INLINE void IRAM_ATTR my9221Pin10_hi() FL_NO_EXCEPT { fl::FastPin<10>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin10_lo() FL_NO_EXCEPT { fl::FastPin<10>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin10_toggle() FL_NO_EXCEPT { fl::FastPin<10>::toggle(); }
template <> struct My9221EspPin<10> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin10_hi, my9221Pin10_lo, my9221Pin10_toggle};
    }
};
#endif

#if defined(FASTLED_FORCE_SOFTWARE_PINS) || (!defined(FASTLED_ESP8266_RAW_PIN_ORDER) && defined(FASTLED_ESP8266_D1_PIN_ORDER))
static FL_NO_INLINE void IRAM_ATTR my9221Pin11_hi() FL_NO_EXCEPT { fl::FastPin<11>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin11_lo() FL_NO_EXCEPT { fl::FastPin<11>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin11_toggle() FL_NO_EXCEPT { fl::FastPin<11>::toggle(); }
template <> struct My9221EspPin<11> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin11_hi, my9221Pin11_lo, my9221Pin11_toggle};
    }
};
#endif

#if defined(FASTLED_FORCE_SOFTWARE_PINS) || defined(FASTLED_ESP8266_RAW_PIN_ORDER) || defined(FASTLED_ESP8266_D1_PIN_ORDER)
static FL_NO_INLINE void IRAM_ATTR my9221Pin12_hi() FL_NO_EXCEPT { fl::FastPin<12>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin12_lo() FL_NO_EXCEPT { fl::FastPin<12>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin12_toggle() FL_NO_EXCEPT { fl::FastPin<12>::toggle(); }
template <> struct My9221EspPin<12> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin12_hi, my9221Pin12_lo, my9221Pin12_toggle};
    }
};
#endif

#if defined(FASTLED_FORCE_SOFTWARE_PINS) || defined(FASTLED_ESP8266_RAW_PIN_ORDER) || defined(FASTLED_ESP8266_D1_PIN_ORDER)
static FL_NO_INLINE void IRAM_ATTR my9221Pin13_hi() FL_NO_EXCEPT { fl::FastPin<13>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin13_lo() FL_NO_EXCEPT { fl::FastPin<13>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin13_toggle() FL_NO_EXCEPT { fl::FastPin<13>::toggle(); }
template <> struct My9221EspPin<13> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin13_hi, my9221Pin13_lo, my9221Pin13_toggle};
    }
};
#endif

#if defined(FASTLED_FORCE_SOFTWARE_PINS) || defined(FASTLED_ESP8266_RAW_PIN_ORDER) || defined(FASTLED_ESP8266_D1_PIN_ORDER)
static FL_NO_INLINE void IRAM_ATTR my9221Pin14_hi() FL_NO_EXCEPT { fl::FastPin<14>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin14_lo() FL_NO_EXCEPT { fl::FastPin<14>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin14_toggle() FL_NO_EXCEPT { fl::FastPin<14>::toggle(); }
template <> struct My9221EspPin<14> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin14_hi, my9221Pin14_lo, my9221Pin14_toggle};
    }
};
#endif

#if defined(FASTLED_FORCE_SOFTWARE_PINS) || defined(FASTLED_ESP8266_RAW_PIN_ORDER) || defined(FASTLED_ESP8266_D1_PIN_ORDER)
static FL_NO_INLINE void IRAM_ATTR my9221Pin15_hi() FL_NO_EXCEPT { fl::FastPin<15>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin15_lo() FL_NO_EXCEPT { fl::FastPin<15>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin15_toggle() FL_NO_EXCEPT { fl::FastPin<15>::toggle(); }
template <> struct My9221EspPin<15> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin15_hi, my9221Pin15_lo, my9221Pin15_toggle};
    }
};
#endif

#if defined(FASTLED_FORCE_SOFTWARE_PINS) || defined(FASTLED_ESP8266_RAW_PIN_ORDER)
static FL_NO_INLINE void IRAM_ATTR my9221Pin16_hi() FL_NO_EXCEPT { fl::FastPin<16>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin16_lo() FL_NO_EXCEPT { fl::FastPin<16>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Pin16_toggle() FL_NO_EXCEPT { fl::FastPin<16>::toggle(); }
template <> struct My9221EspPin<16> {
    static FASTLED_FORCE_INLINE My9221EspPinFunctions get() FL_NO_EXCEPT {
        return {my9221Pin16_hi, my9221Pin16_lo, my9221Pin16_toggle};
    }
};
#endif

struct My9221EspPins {
    void (*data_hi)();
    void (*data_lo)();
    void (*clock_toggle)();
    void (*clock_lo)();
    FASTLED_FORCE_INLINE void dataHi() const FL_NO_EXCEPT { data_hi(); }
    FASTLED_FORCE_INLINE void dataLo() const FL_NO_EXCEPT { data_lo(); }
    FASTLED_FORCE_INLINE void clockToggle() const FL_NO_EXCEPT { clock_toggle(); }
    FASTLED_FORCE_INLINE void clockLo() const FL_NO_EXCEPT { clock_lo(); }
};

// This is an ordinary non-template function: GCC then places BOTH the function
// and Xtensa literal pools in vendor IRAM sections. All serializer helpers inline
// here; GPIO callbacks also have ordinary IRAM bodies. noipa prevents constprop
// clones from defeating this boundary. Caller stack must be internal memory.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 9
static FL_NO_INLINE __attribute__((noipa)) void IRAM_ATTR my9221EmitEsp8266(
    const fl::u16* frame, fl::size words, My9221EspPins pins) FL_NO_EXCEPT {
    My9221Serializer<My9221EspPins, My9221Timing<0, 0>>::run(frame, words, pins);
}
#else
static FL_NO_INLINE __attribute__((noclone)) void IRAM_ATTR my9221EmitEsp8266(
    const fl::u16* frame, fl::size words, My9221EspPins pins) FL_NO_EXCEPT {
    My9221Serializer<My9221EspPins, My9221Timing<0, 0>>::run(frame, words, pins);
}
#endif

template <int Data, fl::u8 Clock> struct My9221EmitterDispatch {
    static FASTLED_FORCE_INLINE void run(const fl::u16* frame, fl::size words) FL_NO_EXCEPT {
        const My9221EspPinFunctions data = My9221EspPin<Data>::get();
        const My9221EspPinFunctions clock = My9221EspPin<Clock>::get();
        // All addresses are copied before the kernel masks interrupts; there is
        // no flash dispatch table, controller or vector access under the guard.
        const My9221EspPins pins = {data.hi, data.lo, clock.toggle, clock.lo};
        my9221EmitEsp8266(frame, words, pins);
    }
};

} // namespace detail
} // namespace fl
