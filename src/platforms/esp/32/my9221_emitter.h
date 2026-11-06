#pragma once

// IWYU pragma: private
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/system/fastpin.h"
#include "platforms/shared/my9221_typed_emitter.h"

namespace fl {
namespace detail {

#if !defined(FASTLED_FORCE_SOFTWARE_PINS)

struct My9221Esp32PinFunctions {
    void (*hi)();
    void (*lo)();
    void (*toggle)();
};
template <int Pin> struct My9221Esp32Pin;

// User exclusions are a C++ constant-expression contract (including typed casts
// and named constexpr values), not a preprocessor expression. Only the SDK
// hardware mask is tested below. Invalid operations are never instantiated;
// acquiring callbacks for an excluded pin fails the selected dispatch assertions.
template <int Pin, bool Valid = fl::FastPin<Pin>::validpin()>
struct My9221Esp32PinOps;
template <int Pin> struct My9221Esp32PinOps<Pin, true> {
    static FASTLED_FORCE_INLINE void hi() FL_NO_EXCEPT { fl::FastPin<Pin>::hi(); }
    static FASTLED_FORCE_INLINE void lo() FL_NO_EXCEPT { fl::FastPin<Pin>::lo(); }
    static FASTLED_FORCE_INLINE void toggle() FL_NO_EXCEPT { fl::FastPin<Pin>::toggle(); }
};
template <int Pin> struct My9221Esp32PinOps<Pin, false> {
    // These private bodies only keep unused ordinary function declarations
    // well-formed. dispatch rejects the pin before any address can be acquired;
    // trap rather than silently emit nothing if this invariant is violated.
    static FASTLED_FORCE_INLINE void hi() FL_NO_EXCEPT { __builtin_trap(); }
    static FASTLED_FORCE_INLINE void lo() FL_NO_EXCEPT { __builtin_trap(); }
    static FASTLED_FORCE_INLINE void toggle() FL_NO_EXCEPT { __builtin_trap(); }
};

// Ordinary functions avoid template/COMDAT section placement while retaining
// selected FastPin semantics. No GPIO mapping or register layout is copied.
#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 0))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin0_hi() FL_NO_EXCEPT { My9221Esp32PinOps<0>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin0_lo() FL_NO_EXCEPT { My9221Esp32PinOps<0>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin0_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<0>::toggle(); }
template <> struct My9221Esp32Pin<0> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin0_hi, my9221Esp32Pin0_lo, my9221Esp32Pin0_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 1))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin1_hi() FL_NO_EXCEPT { My9221Esp32PinOps<1>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin1_lo() FL_NO_EXCEPT { My9221Esp32PinOps<1>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin1_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<1>::toggle(); }
template <> struct My9221Esp32Pin<1> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin1_hi, my9221Esp32Pin1_lo, my9221Esp32Pin1_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 2))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin2_hi() FL_NO_EXCEPT { My9221Esp32PinOps<2>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin2_lo() FL_NO_EXCEPT { My9221Esp32PinOps<2>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin2_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<2>::toggle(); }
template <> struct My9221Esp32Pin<2> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin2_hi, my9221Esp32Pin2_lo, my9221Esp32Pin2_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 3))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin3_hi() FL_NO_EXCEPT { My9221Esp32PinOps<3>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin3_lo() FL_NO_EXCEPT { My9221Esp32PinOps<3>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin3_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<3>::toggle(); }
template <> struct My9221Esp32Pin<3> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin3_hi, my9221Esp32Pin3_lo, my9221Esp32Pin3_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 4))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin4_hi() FL_NO_EXCEPT { My9221Esp32PinOps<4>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin4_lo() FL_NO_EXCEPT { My9221Esp32PinOps<4>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin4_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<4>::toggle(); }
template <> struct My9221Esp32Pin<4> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin4_hi, my9221Esp32Pin4_lo, my9221Esp32Pin4_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 5))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin5_hi() FL_NO_EXCEPT { My9221Esp32PinOps<5>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin5_lo() FL_NO_EXCEPT { My9221Esp32PinOps<5>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin5_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<5>::toggle(); }
template <> struct My9221Esp32Pin<5> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin5_hi, my9221Esp32Pin5_lo, my9221Esp32Pin5_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 6))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin6_hi() FL_NO_EXCEPT { My9221Esp32PinOps<6>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin6_lo() FL_NO_EXCEPT { My9221Esp32PinOps<6>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin6_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<6>::toggle(); }
template <> struct My9221Esp32Pin<6> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin6_hi, my9221Esp32Pin6_lo, my9221Esp32Pin6_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 7))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin7_hi() FL_NO_EXCEPT { My9221Esp32PinOps<7>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin7_lo() FL_NO_EXCEPT { My9221Esp32PinOps<7>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin7_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<7>::toggle(); }
template <> struct My9221Esp32Pin<7> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin7_hi, my9221Esp32Pin7_lo, my9221Esp32Pin7_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 8))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin8_hi() FL_NO_EXCEPT { My9221Esp32PinOps<8>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin8_lo() FL_NO_EXCEPT { My9221Esp32PinOps<8>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin8_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<8>::toggle(); }
template <> struct My9221Esp32Pin<8> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin8_hi, my9221Esp32Pin8_lo, my9221Esp32Pin8_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 9))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin9_hi() FL_NO_EXCEPT { My9221Esp32PinOps<9>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin9_lo() FL_NO_EXCEPT { My9221Esp32PinOps<9>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin9_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<9>::toggle(); }
template <> struct My9221Esp32Pin<9> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin9_hi, my9221Esp32Pin9_lo, my9221Esp32Pin9_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 10))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin10_hi() FL_NO_EXCEPT { My9221Esp32PinOps<10>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin10_lo() FL_NO_EXCEPT { My9221Esp32PinOps<10>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin10_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<10>::toggle(); }
template <> struct My9221Esp32Pin<10> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin10_hi, my9221Esp32Pin10_lo, my9221Esp32Pin10_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 11))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin11_hi() FL_NO_EXCEPT { My9221Esp32PinOps<11>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin11_lo() FL_NO_EXCEPT { My9221Esp32PinOps<11>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin11_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<11>::toggle(); }
template <> struct My9221Esp32Pin<11> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin11_hi, my9221Esp32Pin11_lo, my9221Esp32Pin11_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 12))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin12_hi() FL_NO_EXCEPT { My9221Esp32PinOps<12>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin12_lo() FL_NO_EXCEPT { My9221Esp32PinOps<12>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin12_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<12>::toggle(); }
template <> struct My9221Esp32Pin<12> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin12_hi, my9221Esp32Pin12_lo, my9221Esp32Pin12_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 13))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin13_hi() FL_NO_EXCEPT { My9221Esp32PinOps<13>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin13_lo() FL_NO_EXCEPT { My9221Esp32PinOps<13>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin13_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<13>::toggle(); }
template <> struct My9221Esp32Pin<13> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin13_hi, my9221Esp32Pin13_lo, my9221Esp32Pin13_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 14))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin14_hi() FL_NO_EXCEPT { My9221Esp32PinOps<14>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin14_lo() FL_NO_EXCEPT { My9221Esp32PinOps<14>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin14_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<14>::toggle(); }
template <> struct My9221Esp32Pin<14> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin14_hi, my9221Esp32Pin14_lo, my9221Esp32Pin14_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 15))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin15_hi() FL_NO_EXCEPT { My9221Esp32PinOps<15>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin15_lo() FL_NO_EXCEPT { My9221Esp32PinOps<15>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin15_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<15>::toggle(); }
template <> struct My9221Esp32Pin<15> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin15_hi, my9221Esp32Pin15_lo, my9221Esp32Pin15_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 16))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin16_hi() FL_NO_EXCEPT { My9221Esp32PinOps<16>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin16_lo() FL_NO_EXCEPT { My9221Esp32PinOps<16>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin16_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<16>::toggle(); }
template <> struct My9221Esp32Pin<16> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin16_hi, my9221Esp32Pin16_lo, my9221Esp32Pin16_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 17))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin17_hi() FL_NO_EXCEPT { My9221Esp32PinOps<17>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin17_lo() FL_NO_EXCEPT { My9221Esp32PinOps<17>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin17_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<17>::toggle(); }
template <> struct My9221Esp32Pin<17> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin17_hi, my9221Esp32Pin17_lo, my9221Esp32Pin17_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 18))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin18_hi() FL_NO_EXCEPT { My9221Esp32PinOps<18>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin18_lo() FL_NO_EXCEPT { My9221Esp32PinOps<18>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin18_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<18>::toggle(); }
template <> struct My9221Esp32Pin<18> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin18_hi, my9221Esp32Pin18_lo, my9221Esp32Pin18_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 19))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin19_hi() FL_NO_EXCEPT { My9221Esp32PinOps<19>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin19_lo() FL_NO_EXCEPT { My9221Esp32PinOps<19>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin19_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<19>::toggle(); }
template <> struct My9221Esp32Pin<19> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin19_hi, my9221Esp32Pin19_lo, my9221Esp32Pin19_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 20))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin20_hi() FL_NO_EXCEPT { My9221Esp32PinOps<20>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin20_lo() FL_NO_EXCEPT { My9221Esp32PinOps<20>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin20_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<20>::toggle(); }
template <> struct My9221Esp32Pin<20> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin20_hi, my9221Esp32Pin20_lo, my9221Esp32Pin20_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 21))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin21_hi() FL_NO_EXCEPT { My9221Esp32PinOps<21>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin21_lo() FL_NO_EXCEPT { My9221Esp32PinOps<21>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin21_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<21>::toggle(); }
template <> struct My9221Esp32Pin<21> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin21_hi, my9221Esp32Pin21_lo, my9221Esp32Pin21_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 22))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin22_hi() FL_NO_EXCEPT { My9221Esp32PinOps<22>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin22_lo() FL_NO_EXCEPT { My9221Esp32PinOps<22>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin22_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<22>::toggle(); }
template <> struct My9221Esp32Pin<22> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin22_hi, my9221Esp32Pin22_lo, my9221Esp32Pin22_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 23))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin23_hi() FL_NO_EXCEPT { My9221Esp32PinOps<23>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin23_lo() FL_NO_EXCEPT { My9221Esp32PinOps<23>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin23_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<23>::toggle(); }
template <> struct My9221Esp32Pin<23> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin23_hi, my9221Esp32Pin23_lo, my9221Esp32Pin23_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 24))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin24_hi() FL_NO_EXCEPT { My9221Esp32PinOps<24>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin24_lo() FL_NO_EXCEPT { My9221Esp32PinOps<24>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin24_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<24>::toggle(); }
template <> struct My9221Esp32Pin<24> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin24_hi, my9221Esp32Pin24_lo, my9221Esp32Pin24_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 25))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin25_hi() FL_NO_EXCEPT { My9221Esp32PinOps<25>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin25_lo() FL_NO_EXCEPT { My9221Esp32PinOps<25>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin25_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<25>::toggle(); }
template <> struct My9221Esp32Pin<25> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin25_hi, my9221Esp32Pin25_lo, my9221Esp32Pin25_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 26))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin26_hi() FL_NO_EXCEPT { My9221Esp32PinOps<26>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin26_lo() FL_NO_EXCEPT { My9221Esp32PinOps<26>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin26_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<26>::toggle(); }
template <> struct My9221Esp32Pin<26> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin26_hi, my9221Esp32Pin26_lo, my9221Esp32Pin26_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 27))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin27_hi() FL_NO_EXCEPT { My9221Esp32PinOps<27>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin27_lo() FL_NO_EXCEPT { My9221Esp32PinOps<27>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin27_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<27>::toggle(); }
template <> struct My9221Esp32Pin<27> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin27_hi, my9221Esp32Pin27_lo, my9221Esp32Pin27_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 28))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin28_hi() FL_NO_EXCEPT { My9221Esp32PinOps<28>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin28_lo() FL_NO_EXCEPT { My9221Esp32PinOps<28>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin28_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<28>::toggle(); }
template <> struct My9221Esp32Pin<28> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin28_hi, my9221Esp32Pin28_lo, my9221Esp32Pin28_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 29))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin29_hi() FL_NO_EXCEPT { My9221Esp32PinOps<29>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin29_lo() FL_NO_EXCEPT { My9221Esp32PinOps<29>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin29_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<29>::toggle(); }
template <> struct My9221Esp32Pin<29> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin29_hi, my9221Esp32Pin29_lo, my9221Esp32Pin29_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 30))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin30_hi() FL_NO_EXCEPT { My9221Esp32PinOps<30>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin30_lo() FL_NO_EXCEPT { My9221Esp32PinOps<30>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin30_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<30>::toggle(); }
template <> struct My9221Esp32Pin<30> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin30_hi, my9221Esp32Pin30_lo, my9221Esp32Pin30_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 31))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin31_hi() FL_NO_EXCEPT { My9221Esp32PinOps<31>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin31_lo() FL_NO_EXCEPT { My9221Esp32PinOps<31>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin31_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<31>::toggle(); }
template <> struct My9221Esp32Pin<31> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin31_hi, my9221Esp32Pin31_lo, my9221Esp32Pin31_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 32))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin32_hi() FL_NO_EXCEPT { My9221Esp32PinOps<32>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin32_lo() FL_NO_EXCEPT { My9221Esp32PinOps<32>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin32_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<32>::toggle(); }
template <> struct My9221Esp32Pin<32> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin32_hi, my9221Esp32Pin32_lo, my9221Esp32Pin32_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 33))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin33_hi() FL_NO_EXCEPT { My9221Esp32PinOps<33>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin33_lo() FL_NO_EXCEPT { My9221Esp32PinOps<33>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin33_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<33>::toggle(); }
template <> struct My9221Esp32Pin<33> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin33_hi, my9221Esp32Pin33_lo, my9221Esp32Pin33_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 34))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin34_hi() FL_NO_EXCEPT { My9221Esp32PinOps<34>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin34_lo() FL_NO_EXCEPT { My9221Esp32PinOps<34>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin34_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<34>::toggle(); }
template <> struct My9221Esp32Pin<34> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin34_hi, my9221Esp32Pin34_lo, my9221Esp32Pin34_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 35))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin35_hi() FL_NO_EXCEPT { My9221Esp32PinOps<35>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin35_lo() FL_NO_EXCEPT { My9221Esp32PinOps<35>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin35_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<35>::toggle(); }
template <> struct My9221Esp32Pin<35> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin35_hi, my9221Esp32Pin35_lo, my9221Esp32Pin35_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 36))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin36_hi() FL_NO_EXCEPT { My9221Esp32PinOps<36>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin36_lo() FL_NO_EXCEPT { My9221Esp32PinOps<36>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin36_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<36>::toggle(); }
template <> struct My9221Esp32Pin<36> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin36_hi, my9221Esp32Pin36_lo, my9221Esp32Pin36_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 37))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin37_hi() FL_NO_EXCEPT { My9221Esp32PinOps<37>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin37_lo() FL_NO_EXCEPT { My9221Esp32PinOps<37>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin37_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<37>::toggle(); }
template <> struct My9221Esp32Pin<37> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin37_hi, my9221Esp32Pin37_lo, my9221Esp32Pin37_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 38))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin38_hi() FL_NO_EXCEPT { My9221Esp32PinOps<38>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin38_lo() FL_NO_EXCEPT { My9221Esp32PinOps<38>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin38_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<38>::toggle(); }
template <> struct My9221Esp32Pin<38> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin38_hi, my9221Esp32Pin38_lo, my9221Esp32Pin38_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 39))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin39_hi() FL_NO_EXCEPT { My9221Esp32PinOps<39>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin39_lo() FL_NO_EXCEPT { My9221Esp32PinOps<39>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin39_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<39>::toggle(); }
template <> struct My9221Esp32Pin<39> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin39_hi, my9221Esp32Pin39_lo, my9221Esp32Pin39_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 40))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin40_hi() FL_NO_EXCEPT { My9221Esp32PinOps<40>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin40_lo() FL_NO_EXCEPT { My9221Esp32PinOps<40>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin40_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<40>::toggle(); }
template <> struct My9221Esp32Pin<40> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin40_hi, my9221Esp32Pin40_lo, my9221Esp32Pin40_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 41))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin41_hi() FL_NO_EXCEPT { My9221Esp32PinOps<41>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin41_lo() FL_NO_EXCEPT { My9221Esp32PinOps<41>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin41_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<41>::toggle(); }
template <> struct My9221Esp32Pin<41> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin41_hi, my9221Esp32Pin41_lo, my9221Esp32Pin41_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 42))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin42_hi() FL_NO_EXCEPT { My9221Esp32PinOps<42>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin42_lo() FL_NO_EXCEPT { My9221Esp32PinOps<42>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin42_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<42>::toggle(); }
template <> struct My9221Esp32Pin<42> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin42_hi, my9221Esp32Pin42_lo, my9221Esp32Pin42_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 43))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin43_hi() FL_NO_EXCEPT { My9221Esp32PinOps<43>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin43_lo() FL_NO_EXCEPT { My9221Esp32PinOps<43>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin43_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<43>::toggle(); }
template <> struct My9221Esp32Pin<43> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin43_hi, my9221Esp32Pin43_lo, my9221Esp32Pin43_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 44))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin44_hi() FL_NO_EXCEPT { My9221Esp32PinOps<44>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin44_lo() FL_NO_EXCEPT { My9221Esp32PinOps<44>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin44_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<44>::toggle(); }
template <> struct My9221Esp32Pin<44> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin44_hi, my9221Esp32Pin44_lo, my9221Esp32Pin44_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 45))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin45_hi() FL_NO_EXCEPT { My9221Esp32PinOps<45>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin45_lo() FL_NO_EXCEPT { My9221Esp32PinOps<45>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin45_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<45>::toggle(); }
template <> struct My9221Esp32Pin<45> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin45_hi, my9221Esp32Pin45_lo, my9221Esp32Pin45_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 46))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin46_hi() FL_NO_EXCEPT { My9221Esp32PinOps<46>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin46_lo() FL_NO_EXCEPT { My9221Esp32PinOps<46>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin46_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<46>::toggle(); }
template <> struct My9221Esp32Pin<46> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin46_hi, my9221Esp32Pin46_lo, my9221Esp32Pin46_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 47))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin47_hi() FL_NO_EXCEPT { My9221Esp32PinOps<47>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin47_lo() FL_NO_EXCEPT { My9221Esp32PinOps<47>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin47_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<47>::toggle(); }
template <> struct My9221Esp32Pin<47> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin47_hi, my9221Esp32Pin47_lo, my9221Esp32Pin47_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 48))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin48_hi() FL_NO_EXCEPT { My9221Esp32PinOps<48>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin48_lo() FL_NO_EXCEPT { My9221Esp32PinOps<48>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin48_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<48>::toggle(); }
template <> struct My9221Esp32Pin<48> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin48_hi, my9221Esp32Pin48_lo, my9221Esp32Pin48_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 49))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin49_hi() FL_NO_EXCEPT { My9221Esp32PinOps<49>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin49_lo() FL_NO_EXCEPT { My9221Esp32PinOps<49>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin49_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<49>::toggle(); }
template <> struct My9221Esp32Pin<49> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin49_hi, my9221Esp32Pin49_lo, my9221Esp32Pin49_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 50))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin50_hi() FL_NO_EXCEPT { My9221Esp32PinOps<50>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin50_lo() FL_NO_EXCEPT { My9221Esp32PinOps<50>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin50_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<50>::toggle(); }
template <> struct My9221Esp32Pin<50> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin50_hi, my9221Esp32Pin50_lo, my9221Esp32Pin50_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 51))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin51_hi() FL_NO_EXCEPT { My9221Esp32PinOps<51>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin51_lo() FL_NO_EXCEPT { My9221Esp32PinOps<51>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin51_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<51>::toggle(); }
template <> struct My9221Esp32Pin<51> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin51_hi, my9221Esp32Pin51_lo, my9221Esp32Pin51_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 52))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin52_hi() FL_NO_EXCEPT { My9221Esp32PinOps<52>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin52_lo() FL_NO_EXCEPT { My9221Esp32PinOps<52>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin52_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<52>::toggle(); }
template <> struct My9221Esp32Pin<52> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin52_hi, my9221Esp32Pin52_lo, my9221Esp32Pin52_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 53))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin53_hi() FL_NO_EXCEPT { My9221Esp32PinOps<53>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin53_lo() FL_NO_EXCEPT { My9221Esp32PinOps<53>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin53_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<53>::toggle(); }
template <> struct My9221Esp32Pin<53> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin53_hi, my9221Esp32Pin53_lo, my9221Esp32Pin53_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 54))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin54_hi() FL_NO_EXCEPT { My9221Esp32PinOps<54>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin54_lo() FL_NO_EXCEPT { My9221Esp32PinOps<54>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin54_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<54>::toggle(); }
template <> struct My9221Esp32Pin<54> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin54_hi, my9221Esp32Pin54_lo, my9221Esp32Pin54_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 55))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin55_hi() FL_NO_EXCEPT { My9221Esp32PinOps<55>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin55_lo() FL_NO_EXCEPT { My9221Esp32PinOps<55>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin55_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<55>::toggle(); }
template <> struct My9221Esp32Pin<55> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin55_hi, my9221Esp32Pin55_lo, my9221Esp32Pin55_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 56))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin56_hi() FL_NO_EXCEPT { My9221Esp32PinOps<56>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin56_lo() FL_NO_EXCEPT { My9221Esp32PinOps<56>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin56_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<56>::toggle(); }
template <> struct My9221Esp32Pin<56> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin56_hi, my9221Esp32Pin56_lo, my9221Esp32Pin56_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 57))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin57_hi() FL_NO_EXCEPT { My9221Esp32PinOps<57>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin57_lo() FL_NO_EXCEPT { My9221Esp32PinOps<57>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin57_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<57>::toggle(); }
template <> struct My9221Esp32Pin<57> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin57_hi, my9221Esp32Pin57_lo, my9221Esp32Pin57_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 58))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin58_hi() FL_NO_EXCEPT { My9221Esp32PinOps<58>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin58_lo() FL_NO_EXCEPT { My9221Esp32PinOps<58>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin58_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<58>::toggle(); }
template <> struct My9221Esp32Pin<58> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin58_hi, my9221Esp32Pin58_lo, my9221Esp32Pin58_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 59))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin59_hi() FL_NO_EXCEPT { My9221Esp32PinOps<59>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin59_lo() FL_NO_EXCEPT { My9221Esp32PinOps<59>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin59_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<59>::toggle(); }
template <> struct My9221Esp32Pin<59> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin59_hi, my9221Esp32Pin59_lo, my9221Esp32Pin59_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 60))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin60_hi() FL_NO_EXCEPT { My9221Esp32PinOps<60>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin60_lo() FL_NO_EXCEPT { My9221Esp32PinOps<60>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin60_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<60>::toggle(); }
template <> struct My9221Esp32Pin<60> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin60_hi, my9221Esp32Pin60_lo, my9221Esp32Pin60_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 61))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin61_hi() FL_NO_EXCEPT { My9221Esp32PinOps<61>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin61_lo() FL_NO_EXCEPT { My9221Esp32PinOps<61>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin61_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<61>::toggle(); }
template <> struct My9221Esp32Pin<61> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin61_hi, my9221Esp32Pin61_lo, my9221Esp32Pin61_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 62))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin62_hi() FL_NO_EXCEPT { My9221Esp32PinOps<62>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin62_lo() FL_NO_EXCEPT { My9221Esp32PinOps<62>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin62_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<62>::toggle(); }
template <> struct My9221Esp32Pin<62> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin62_hi, my9221Esp32Pin62_lo, my9221Esp32Pin62_toggle};
    }
};
#endif

#if (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ULL << 63))
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin63_hi() FL_NO_EXCEPT { My9221Esp32PinOps<63>::hi(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin63_lo() FL_NO_EXCEPT { My9221Esp32PinOps<63>::lo(); }
static FL_NO_INLINE void IRAM_ATTR my9221Esp32Pin63_toggle() FL_NO_EXCEPT { My9221Esp32PinOps<63>::toggle(); }
template <> struct My9221Esp32Pin<63> {
    static FASTLED_FORCE_INLINE My9221Esp32PinFunctions get() FL_NO_EXCEPT {
        return {my9221Esp32Pin63_hi, my9221Esp32Pin63_lo, my9221Esp32Pin63_toggle};
    }
};
#endif

struct My9221Esp32Pins {
    void (*data_hi)();
    void (*data_lo)();
    void (*clock_toggle)();
    void (*clock_lo)();
    FASTLED_FORCE_INLINE void dataHi() const FL_NO_EXCEPT { data_hi(); }
    FASTLED_FORCE_INLINE void dataLo() const FL_NO_EXCEPT { data_lo(); }
    FASTLED_FORCE_INLINE void clockToggle() const FL_NO_EXCEPT { clock_toggle(); }
    FASTLED_FORCE_INLINE void clockLo() const FL_NO_EXCEPT { clock_lo(); }
};

// One ordinary IRAM boundary shares the typed serializer with every other
// platform. Copy callback addresses before entry; the masked loop reads only
// prepared internal frame memory and by-value operations on the caller's
// internal stack. noipa/noclone prevents LTO from moving a specialised clone
// back into flash. Actual SDK ELF/callee/literal placement remains a required
// qualification, as do higher-priority interrupts, ownership and phase bounds.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 9
static FL_NO_INLINE __attribute__((noipa)) void IRAM_ATTR my9221EmitEsp32(
    const fl::u16* frame, fl::size words, My9221Esp32Pins pins) FL_NO_EXCEPT {
    My9221Serializer<My9221Esp32Pins, My9221Timing<0, 0>>::run(frame, words, pins);
}
#else
static FL_NO_INLINE __attribute__((noclone)) void IRAM_ATTR my9221EmitEsp32(
    const fl::u16* frame, fl::size words, My9221Esp32Pins pins) FL_NO_EXCEPT {
    My9221Serializer<My9221Esp32Pins, My9221Timing<0, 0>>::run(frame, words, pins);
}
#endif

template <int Data, fl::u8 Clock> struct My9221EmitterDispatch {
    static FASTLED_FORCE_INLINE void run(const fl::u16* frame, fl::size words) FL_NO_EXCEPT {
        FL_STATIC_ASSERT(fl::FastPin<Data>::validpin(), "MY9221 requires a valid selected data pin");
        FL_STATIC_ASSERT(fl::FastPin<Clock>::validpin(), "MY9221 requires a valid selected clock pin");
        const My9221Esp32PinFunctions data = My9221Esp32Pin<Data>::get();
        const My9221Esp32PinFunctions clock = My9221Esp32Pin<Clock>::get();
        const My9221Esp32Pins pins = {data.hi, data.lo, clock.toggle, clock.lo};
        my9221EmitEsp32(frame, words, pins);
    }
};

#else

// ForcedSoftwarePin operations have no qualified internal-memory/cache-safe
// call closure here. Ordinary FastLED/Generic SPI inclusion remains valid;
// reject only when the real MY9221 dispatch is instantiated, never route it
// through native pins or an unqualified shared serializer.
template <int Data, fl::u8 Clock> struct My9221Esp32ForcedUnsupported {
    static constexpr bool value = false;
};
template <int Data, fl::u8 Clock> struct My9221EmitterDispatch {
    static FASTLED_FORCE_INLINE void run(const fl::u16*, fl::size) FL_NO_EXCEPT {
        FL_STATIC_ASSERT((My9221Esp32ForcedUnsupported<Data, Clock>::value),
            "MY9221 forced ESP32 pin provider/cache closure is unsupported");
    }
};

#endif // !defined(FASTLED_FORCE_SOFTWARE_PINS)

} // namespace detail
} // namespace fl
