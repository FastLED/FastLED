#pragma once

// IWYU pragma: private
#include <pico/platform.h> // IWYU pragma: keep
#include "platforms/is_platform.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/system/fastpin.h"
#include "platforms/shared/my9221_typed_emitter.h"

namespace fl {
namespace detail {

struct My9221RpPinFunctions {
    void (*hi)();
    void (*lo)();
    void (*toggle)();
};
template <int Pin> struct My9221RpPin;

// Use the existing FastPin logical pin set and operations, not a second SIO
// register mapping. Each ordinary function has its own SDK SRAM section so
// unused callbacks can be garbage-collected. Forced software pins retain
// their selected operations; their callee placement needs separate evidence.
static void __no_inline_not_in_flash_func(my9221RpPin0_hi)() FL_NO_EXCEPT { fl::FastPin<0>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin0_lo)() FL_NO_EXCEPT { fl::FastPin<0>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin0_toggle)() FL_NO_EXCEPT { fl::FastPin<0>::toggle(); }
template <> struct My9221RpPin<0> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin0_hi, my9221RpPin0_lo, my9221RpPin0_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin1_hi)() FL_NO_EXCEPT { fl::FastPin<1>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin1_lo)() FL_NO_EXCEPT { fl::FastPin<1>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin1_toggle)() FL_NO_EXCEPT { fl::FastPin<1>::toggle(); }
template <> struct My9221RpPin<1> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin1_hi, my9221RpPin1_lo, my9221RpPin1_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin2_hi)() FL_NO_EXCEPT { fl::FastPin<2>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin2_lo)() FL_NO_EXCEPT { fl::FastPin<2>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin2_toggle)() FL_NO_EXCEPT { fl::FastPin<2>::toggle(); }
template <> struct My9221RpPin<2> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin2_hi, my9221RpPin2_lo, my9221RpPin2_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin3_hi)() FL_NO_EXCEPT { fl::FastPin<3>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin3_lo)() FL_NO_EXCEPT { fl::FastPin<3>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin3_toggle)() FL_NO_EXCEPT { fl::FastPin<3>::toggle(); }
template <> struct My9221RpPin<3> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin3_hi, my9221RpPin3_lo, my9221RpPin3_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin4_hi)() FL_NO_EXCEPT { fl::FastPin<4>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin4_lo)() FL_NO_EXCEPT { fl::FastPin<4>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin4_toggle)() FL_NO_EXCEPT { fl::FastPin<4>::toggle(); }
template <> struct My9221RpPin<4> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin4_hi, my9221RpPin4_lo, my9221RpPin4_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin5_hi)() FL_NO_EXCEPT { fl::FastPin<5>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin5_lo)() FL_NO_EXCEPT { fl::FastPin<5>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin5_toggle)() FL_NO_EXCEPT { fl::FastPin<5>::toggle(); }
template <> struct My9221RpPin<5> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin5_hi, my9221RpPin5_lo, my9221RpPin5_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin6_hi)() FL_NO_EXCEPT { fl::FastPin<6>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin6_lo)() FL_NO_EXCEPT { fl::FastPin<6>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin6_toggle)() FL_NO_EXCEPT { fl::FastPin<6>::toggle(); }
template <> struct My9221RpPin<6> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin6_hi, my9221RpPin6_lo, my9221RpPin6_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin7_hi)() FL_NO_EXCEPT { fl::FastPin<7>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin7_lo)() FL_NO_EXCEPT { fl::FastPin<7>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin7_toggle)() FL_NO_EXCEPT { fl::FastPin<7>::toggle(); }
template <> struct My9221RpPin<7> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin7_hi, my9221RpPin7_lo, my9221RpPin7_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin8_hi)() FL_NO_EXCEPT { fl::FastPin<8>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin8_lo)() FL_NO_EXCEPT { fl::FastPin<8>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin8_toggle)() FL_NO_EXCEPT { fl::FastPin<8>::toggle(); }
template <> struct My9221RpPin<8> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin8_hi, my9221RpPin8_lo, my9221RpPin8_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin9_hi)() FL_NO_EXCEPT { fl::FastPin<9>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin9_lo)() FL_NO_EXCEPT { fl::FastPin<9>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin9_toggle)() FL_NO_EXCEPT { fl::FastPin<9>::toggle(); }
template <> struct My9221RpPin<9> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin9_hi, my9221RpPin9_lo, my9221RpPin9_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin10_hi)() FL_NO_EXCEPT { fl::FastPin<10>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin10_lo)() FL_NO_EXCEPT { fl::FastPin<10>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin10_toggle)() FL_NO_EXCEPT { fl::FastPin<10>::toggle(); }
template <> struct My9221RpPin<10> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin10_hi, my9221RpPin10_lo, my9221RpPin10_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin11_hi)() FL_NO_EXCEPT { fl::FastPin<11>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin11_lo)() FL_NO_EXCEPT { fl::FastPin<11>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin11_toggle)() FL_NO_EXCEPT { fl::FastPin<11>::toggle(); }
template <> struct My9221RpPin<11> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin11_hi, my9221RpPin11_lo, my9221RpPin11_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin12_hi)() FL_NO_EXCEPT { fl::FastPin<12>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin12_lo)() FL_NO_EXCEPT { fl::FastPin<12>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin12_toggle)() FL_NO_EXCEPT { fl::FastPin<12>::toggle(); }
template <> struct My9221RpPin<12> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin12_hi, my9221RpPin12_lo, my9221RpPin12_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin13_hi)() FL_NO_EXCEPT { fl::FastPin<13>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin13_lo)() FL_NO_EXCEPT { fl::FastPin<13>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin13_toggle)() FL_NO_EXCEPT { fl::FastPin<13>::toggle(); }
template <> struct My9221RpPin<13> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin13_hi, my9221RpPin13_lo, my9221RpPin13_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin14_hi)() FL_NO_EXCEPT { fl::FastPin<14>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin14_lo)() FL_NO_EXCEPT { fl::FastPin<14>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin14_toggle)() FL_NO_EXCEPT { fl::FastPin<14>::toggle(); }
template <> struct My9221RpPin<14> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin14_hi, my9221RpPin14_lo, my9221RpPin14_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin15_hi)() FL_NO_EXCEPT { fl::FastPin<15>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin15_lo)() FL_NO_EXCEPT { fl::FastPin<15>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin15_toggle)() FL_NO_EXCEPT { fl::FastPin<15>::toggle(); }
template <> struct My9221RpPin<15> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin15_hi, my9221RpPin15_lo, my9221RpPin15_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin16_hi)() FL_NO_EXCEPT { fl::FastPin<16>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin16_lo)() FL_NO_EXCEPT { fl::FastPin<16>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin16_toggle)() FL_NO_EXCEPT { fl::FastPin<16>::toggle(); }
template <> struct My9221RpPin<16> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin16_hi, my9221RpPin16_lo, my9221RpPin16_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin17_hi)() FL_NO_EXCEPT { fl::FastPin<17>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin17_lo)() FL_NO_EXCEPT { fl::FastPin<17>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin17_toggle)() FL_NO_EXCEPT { fl::FastPin<17>::toggle(); }
template <> struct My9221RpPin<17> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin17_hi, my9221RpPin17_lo, my9221RpPin17_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin18_hi)() FL_NO_EXCEPT { fl::FastPin<18>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin18_lo)() FL_NO_EXCEPT { fl::FastPin<18>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin18_toggle)() FL_NO_EXCEPT { fl::FastPin<18>::toggle(); }
template <> struct My9221RpPin<18> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin18_hi, my9221RpPin18_lo, my9221RpPin18_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin19_hi)() FL_NO_EXCEPT { fl::FastPin<19>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin19_lo)() FL_NO_EXCEPT { fl::FastPin<19>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin19_toggle)() FL_NO_EXCEPT { fl::FastPin<19>::toggle(); }
template <> struct My9221RpPin<19> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin19_hi, my9221RpPin19_lo, my9221RpPin19_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin20_hi)() FL_NO_EXCEPT { fl::FastPin<20>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin20_lo)() FL_NO_EXCEPT { fl::FastPin<20>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin20_toggle)() FL_NO_EXCEPT { fl::FastPin<20>::toggle(); }
template <> struct My9221RpPin<20> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin20_hi, my9221RpPin20_lo, my9221RpPin20_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin21_hi)() FL_NO_EXCEPT { fl::FastPin<21>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin21_lo)() FL_NO_EXCEPT { fl::FastPin<21>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin21_toggle)() FL_NO_EXCEPT { fl::FastPin<21>::toggle(); }
template <> struct My9221RpPin<21> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin21_hi, my9221RpPin21_lo, my9221RpPin21_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin22_hi)() FL_NO_EXCEPT { fl::FastPin<22>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin22_lo)() FL_NO_EXCEPT { fl::FastPin<22>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin22_toggle)() FL_NO_EXCEPT { fl::FastPin<22>::toggle(); }
template <> struct My9221RpPin<22> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin22_hi, my9221RpPin22_lo, my9221RpPin22_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin23_hi)() FL_NO_EXCEPT { fl::FastPin<23>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin23_lo)() FL_NO_EXCEPT { fl::FastPin<23>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin23_toggle)() FL_NO_EXCEPT { fl::FastPin<23>::toggle(); }
template <> struct My9221RpPin<23> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin23_hi, my9221RpPin23_lo, my9221RpPin23_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin24_hi)() FL_NO_EXCEPT { fl::FastPin<24>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin24_lo)() FL_NO_EXCEPT { fl::FastPin<24>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin24_toggle)() FL_NO_EXCEPT { fl::FastPin<24>::toggle(); }
template <> struct My9221RpPin<24> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin24_hi, my9221RpPin24_lo, my9221RpPin24_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin25_hi)() FL_NO_EXCEPT { fl::FastPin<25>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin25_lo)() FL_NO_EXCEPT { fl::FastPin<25>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin25_toggle)() FL_NO_EXCEPT { fl::FastPin<25>::toggle(); }
template <> struct My9221RpPin<25> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin25_hi, my9221RpPin25_lo, my9221RpPin25_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin26_hi)() FL_NO_EXCEPT { fl::FastPin<26>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin26_lo)() FL_NO_EXCEPT { fl::FastPin<26>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin26_toggle)() FL_NO_EXCEPT { fl::FastPin<26>::toggle(); }
template <> struct My9221RpPin<26> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin26_hi, my9221RpPin26_lo, my9221RpPin26_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin27_hi)() FL_NO_EXCEPT { fl::FastPin<27>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin27_lo)() FL_NO_EXCEPT { fl::FastPin<27>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin27_toggle)() FL_NO_EXCEPT { fl::FastPin<27>::toggle(); }
template <> struct My9221RpPin<27> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin27_hi, my9221RpPin27_lo, my9221RpPin27_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin28_hi)() FL_NO_EXCEPT { fl::FastPin<28>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin28_lo)() FL_NO_EXCEPT { fl::FastPin<28>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin28_toggle)() FL_NO_EXCEPT { fl::FastPin<28>::toggle(); }
template <> struct My9221RpPin<28> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin28_hi, my9221RpPin28_lo, my9221RpPin28_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin29_hi)() FL_NO_EXCEPT { fl::FastPin<29>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin29_lo)() FL_NO_EXCEPT { fl::FastPin<29>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin29_toggle)() FL_NO_EXCEPT { fl::FastPin<29>::toggle(); }
template <> struct My9221RpPin<29> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin29_hi, my9221RpPin29_lo, my9221RpPin29_toggle};
    }
};

#if defined(PICO_RP2350) || defined(FL_IS_RP2350)
static void __no_inline_not_in_flash_func(my9221RpPin30_hi)() FL_NO_EXCEPT { fl::FastPin<30>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin30_lo)() FL_NO_EXCEPT { fl::FastPin<30>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin30_toggle)() FL_NO_EXCEPT { fl::FastPin<30>::toggle(); }
template <> struct My9221RpPin<30> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin30_hi, my9221RpPin30_lo, my9221RpPin30_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin31_hi)() FL_NO_EXCEPT { fl::FastPin<31>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin31_lo)() FL_NO_EXCEPT { fl::FastPin<31>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin31_toggle)() FL_NO_EXCEPT { fl::FastPin<31>::toggle(); }
template <> struct My9221RpPin<31> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin31_hi, my9221RpPin31_lo, my9221RpPin31_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin32_hi)() FL_NO_EXCEPT { fl::FastPin<32>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin32_lo)() FL_NO_EXCEPT { fl::FastPin<32>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin32_toggle)() FL_NO_EXCEPT { fl::FastPin<32>::toggle(); }
template <> struct My9221RpPin<32> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin32_hi, my9221RpPin32_lo, my9221RpPin32_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin33_hi)() FL_NO_EXCEPT { fl::FastPin<33>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin33_lo)() FL_NO_EXCEPT { fl::FastPin<33>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin33_toggle)() FL_NO_EXCEPT { fl::FastPin<33>::toggle(); }
template <> struct My9221RpPin<33> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin33_hi, my9221RpPin33_lo, my9221RpPin33_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin34_hi)() FL_NO_EXCEPT { fl::FastPin<34>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin34_lo)() FL_NO_EXCEPT { fl::FastPin<34>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin34_toggle)() FL_NO_EXCEPT { fl::FastPin<34>::toggle(); }
template <> struct My9221RpPin<34> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin34_hi, my9221RpPin34_lo, my9221RpPin34_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin35_hi)() FL_NO_EXCEPT { fl::FastPin<35>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin35_lo)() FL_NO_EXCEPT { fl::FastPin<35>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin35_toggle)() FL_NO_EXCEPT { fl::FastPin<35>::toggle(); }
template <> struct My9221RpPin<35> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin35_hi, my9221RpPin35_lo, my9221RpPin35_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin36_hi)() FL_NO_EXCEPT { fl::FastPin<36>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin36_lo)() FL_NO_EXCEPT { fl::FastPin<36>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin36_toggle)() FL_NO_EXCEPT { fl::FastPin<36>::toggle(); }
template <> struct My9221RpPin<36> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin36_hi, my9221RpPin36_lo, my9221RpPin36_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin37_hi)() FL_NO_EXCEPT { fl::FastPin<37>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin37_lo)() FL_NO_EXCEPT { fl::FastPin<37>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin37_toggle)() FL_NO_EXCEPT { fl::FastPin<37>::toggle(); }
template <> struct My9221RpPin<37> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin37_hi, my9221RpPin37_lo, my9221RpPin37_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin38_hi)() FL_NO_EXCEPT { fl::FastPin<38>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin38_lo)() FL_NO_EXCEPT { fl::FastPin<38>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin38_toggle)() FL_NO_EXCEPT { fl::FastPin<38>::toggle(); }
template <> struct My9221RpPin<38> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin38_hi, my9221RpPin38_lo, my9221RpPin38_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin39_hi)() FL_NO_EXCEPT { fl::FastPin<39>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin39_lo)() FL_NO_EXCEPT { fl::FastPin<39>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin39_toggle)() FL_NO_EXCEPT { fl::FastPin<39>::toggle(); }
template <> struct My9221RpPin<39> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin39_hi, my9221RpPin39_lo, my9221RpPin39_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin40_hi)() FL_NO_EXCEPT { fl::FastPin<40>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin40_lo)() FL_NO_EXCEPT { fl::FastPin<40>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin40_toggle)() FL_NO_EXCEPT { fl::FastPin<40>::toggle(); }
template <> struct My9221RpPin<40> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin40_hi, my9221RpPin40_lo, my9221RpPin40_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin41_hi)() FL_NO_EXCEPT { fl::FastPin<41>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin41_lo)() FL_NO_EXCEPT { fl::FastPin<41>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin41_toggle)() FL_NO_EXCEPT { fl::FastPin<41>::toggle(); }
template <> struct My9221RpPin<41> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin41_hi, my9221RpPin41_lo, my9221RpPin41_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin42_hi)() FL_NO_EXCEPT { fl::FastPin<42>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin42_lo)() FL_NO_EXCEPT { fl::FastPin<42>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin42_toggle)() FL_NO_EXCEPT { fl::FastPin<42>::toggle(); }
template <> struct My9221RpPin<42> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin42_hi, my9221RpPin42_lo, my9221RpPin42_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin43_hi)() FL_NO_EXCEPT { fl::FastPin<43>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin43_lo)() FL_NO_EXCEPT { fl::FastPin<43>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin43_toggle)() FL_NO_EXCEPT { fl::FastPin<43>::toggle(); }
template <> struct My9221RpPin<43> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin43_hi, my9221RpPin43_lo, my9221RpPin43_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin44_hi)() FL_NO_EXCEPT { fl::FastPin<44>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin44_lo)() FL_NO_EXCEPT { fl::FastPin<44>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin44_toggle)() FL_NO_EXCEPT { fl::FastPin<44>::toggle(); }
template <> struct My9221RpPin<44> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin44_hi, my9221RpPin44_lo, my9221RpPin44_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin45_hi)() FL_NO_EXCEPT { fl::FastPin<45>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin45_lo)() FL_NO_EXCEPT { fl::FastPin<45>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin45_toggle)() FL_NO_EXCEPT { fl::FastPin<45>::toggle(); }
template <> struct My9221RpPin<45> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin45_hi, my9221RpPin45_lo, my9221RpPin45_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin46_hi)() FL_NO_EXCEPT { fl::FastPin<46>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin46_lo)() FL_NO_EXCEPT { fl::FastPin<46>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin46_toggle)() FL_NO_EXCEPT { fl::FastPin<46>::toggle(); }
template <> struct My9221RpPin<46> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin46_hi, my9221RpPin46_lo, my9221RpPin46_toggle};
    }
};

static void __no_inline_not_in_flash_func(my9221RpPin47_hi)() FL_NO_EXCEPT { fl::FastPin<47>::hi(); }
static void __no_inline_not_in_flash_func(my9221RpPin47_lo)() FL_NO_EXCEPT { fl::FastPin<47>::lo(); }
static void __no_inline_not_in_flash_func(my9221RpPin47_toggle)() FL_NO_EXCEPT { fl::FastPin<47>::toggle(); }
template <> struct My9221RpPin<47> {
    static FASTLED_FORCE_INLINE My9221RpPinFunctions get() FL_NO_EXCEPT {
        return {my9221RpPin47_hi, my9221RpPin47_lo, my9221RpPin47_toggle};
    }
};

#endif

struct My9221RpPins {
    void (*data_hi)();
    void (*data_lo)();
    void (*clock_toggle)();
    void (*clock_lo)();
    FASTLED_FORCE_INLINE void dataHi() const FL_NO_EXCEPT { data_hi(); }
    FASTLED_FORCE_INLINE void dataLo() const FL_NO_EXCEPT { data_lo(); }
    FASTLED_FORCE_INLINE void clockToggle() const FL_NO_EXCEPT { clock_toggle(); }
    FASTLED_FORCE_INLINE void clockLo() const FL_NO_EXCEPT { clock_lo(); }
};

// Pico SDK __in_ram(group)/__not_in_flash_func(name) use .time_critical.group.
// This ordinary function avoids template section placement. flatten requests
// inlining of the SDK's plain static-inline busy_wait_at_least_cycles through
// SpiCycleWait; callbacks remain noinline in their own SRAM sections. Actual
// ELF inspection must verify that the wait and every masked callee/literal
// stay in SRAM. This annotation alone proves neither placement nor phase WCET.
// Frame and caller stack must be internal memory; other-core/XIP ownership and
// high-priority exceptions remain independent caller/target obligations.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 9
static __attribute__((noipa, flatten))
void __no_inline_not_in_flash_func(my9221EmitRp)(const fl::u16* frame, fl::size words, My9221RpPins pins) FL_NO_EXCEPT {
    My9221Serializer<My9221RpPins, My9221Timing<0, 0>>::run(frame, words, pins);
}
#else
static __attribute__((noclone, flatten))
void __no_inline_not_in_flash_func(my9221EmitRp)(const fl::u16* frame, fl::size words, My9221RpPins pins) FL_NO_EXCEPT {
    My9221Serializer<My9221RpPins, My9221Timing<0, 0>>::run(frame, words, pins);
}
#endif

template <int Data, fl::u8 Clock> struct My9221EmitterDispatch {
    static FASTLED_FORCE_INLINE void run(const fl::u16* frame, fl::size words) FL_NO_EXCEPT {
        const My9221RpPinFunctions data = My9221RpPin<Data>::get();
        const My9221RpPinFunctions clock = My9221RpPin<Clock>::get();
        const My9221RpPins pins = {data.hi, data.lo, clock.toggle, clock.lo};
        my9221EmitRp(frame, words, pins);
    }
};

} // namespace detail
} // namespace fl
