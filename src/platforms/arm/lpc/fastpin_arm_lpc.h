// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_FASTPIN_ARM_LPC_H
#define __INC_FASTPIN_ARM_LPC_H

#include "fl/stl/bit_cast.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/static_assert.h"
#include "fl/system/fastpin_base.h"
#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

// Scoped to chips that share the "modern" LPC GPIO controller (DIR / MASK /
// PIN / MPIN / SET / CLR / NOT register layout at 0xA0000000):
//   - LPC8xx       (LPC845 / LPC804)            — UM11029 / UM11065
//   - LPC11Uxx     (LPC11U24 / LPC11U35)        — UM10462 §9   (#2845 Stage 4.1)
//   - LPC15xx      (LPC15{17,18,19,47,48,49})   — UM11074      (#2845 Stage 4.2)
//
// LPC11xx legacy parts (LPC1110 / LPC1112 / LPC1114 / LPC1115) use the
// older "legacy GPIO" controller at 0x50000000 with 12-bit masked-access
// semantics (UM10398). They route to a sibling header
// `fastpin_arm_lpc11_legacy.h` rather than this file's modern 0xA0000000
// templates -- the two layouts are not register-compatible.
#if defined(FL_IS_ARM_LPC_11_LEGACY) && \
    !(defined(FL_IS_ARM_LPC_845) || defined(FL_IS_ARM_LPC_804) || \
      defined(FL_IS_ARM_LPC_11_USB) || defined(FL_IS_ARM_LPC_15))
#include "platforms/arm/lpc/fastpin_arm_lpc11_legacy.h"
#endif

#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_ARM_LPC_804) || \
    defined(FL_IS_ARM_LPC_11_USB) || defined(FL_IS_ARM_LPC_15)

namespace fl {

// LPC GPIO register base. Prefer the vendor/CMSIS register definition when a
// board package provides one. Reduced host-side builds can still use the
// documented LPC offset constants below without carrying a padded register
// mirror solely to reach the handful of FastPin registers.
#ifndef FL_LPC_GPIO_BASE
#define FL_LPC_GPIO_BASE 0xA0000000UL
#endif

#if defined(LPC_GPIO)
#define FL_LPC_GPIO_HW LPC_GPIO
#elif defined(GPIO)
#define FL_LPC_GPIO_HW GPIO
#endif

namespace lpc_detail {

constexpr u32 FL_LPC_GPIO_PIN_OFFSET    = 0x2100u;
constexpr u32 FL_LPC_GPIO_SET_OFFSET    = 0x2200u;
constexpr u32 FL_LPC_GPIO_CLR_OFFSET    = 0x2280u;
constexpr u32 FL_LPC_GPIO_NOT_OFFSET    = 0x2300u;
constexpr u32 FL_LPC_GPIO_DIRSET_OFFSET = 0x2380u;
constexpr u32 FL_LPC_GPIO_DIRCLR_OFFSET = 0x2400u;

FL_STATIC_ASSERT(FL_LPC_GPIO_SET_OFFSET - FL_LPC_GPIO_PIN_OFFSET == 0x100u,
                 "LPC SET offset must match clockless HI offset");
FL_STATIC_ASSERT(FL_LPC_GPIO_CLR_OFFSET - FL_LPC_GPIO_PIN_OFFSET == 0x180u,
                 "LPC CLR offset must match clockless LO offset");

#if defined(FL_LPC_GPIO_HW)
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_pin() { return &FL_LPC_GPIO_HW->PIN[0]; }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_set() { return &FL_LPC_GPIO_HW->SET[0]; }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_clr() { return &FL_LPC_GPIO_HW->CLR[0]; }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_not() { return &FL_LPC_GPIO_HW->NOT[0]; }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_dirset() { return &FL_LPC_GPIO_HW->DIRSET[0]; }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_dirclr() { return &FL_LPC_GPIO_HW->DIRCLR[0]; }
#else
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_reg(u32 offset) {
    // Constructing a peripheral pointer from a numeric base + offset is the
    // canonical CMSIS pattern. `fl::bit_cast` is the FastLED-blessed alternative
    // to `reinterpret_cast` for this conversion; both lower to the same load.
    return fl::bit_cast<volatile u32*>(FL_LPC_GPIO_BASE + offset);
}
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_pin() { return lpc_gpio_reg(FL_LPC_GPIO_PIN_OFFSET); }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_set() { return lpc_gpio_reg(FL_LPC_GPIO_SET_OFFSET); }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_clr() { return lpc_gpio_reg(FL_LPC_GPIO_CLR_OFFSET); }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_not() { return lpc_gpio_reg(FL_LPC_GPIO_NOT_OFFSET); }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_dirset() { return lpc_gpio_reg(FL_LPC_GPIO_DIRSET_OFFSET); }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_dirclr() { return lpc_gpio_reg(FL_LPC_GPIO_DIRCLR_OFFSET); }
#endif

}  // namespace lpc_detail

// FastPin template for "modern" LPC GPIO pins (LPC8xx / LPC11Uxx / LPC15xx).
// PORT0 is the primary port populated on the LPC845/LPC804 packages this
// driver covers today; multi-port support is per-variant and not currently
// wired up.
template <u8 PIN, u32 _MASK>
class _ARMPIN : public ValidPinBase {
public:
    typedef volatile u32* port_ptr_t;
    typedef u32           port_t;

    inline static void setOutput() __attribute__((always_inline)) {
        *lpc_detail::lpc_gpio_dirset() = _MASK;
    }
    inline static void setInput() __attribute__((always_inline)) {
        *lpc_detail::lpc_gpio_dirclr() = _MASK;
    }

    inline static void hi() __attribute__((always_inline)) {
        *lpc_detail::lpc_gpio_set() = _MASK;
    }
    inline static void lo() __attribute__((always_inline)) {
        *lpc_detail::lpc_gpio_clr() = _MASK;
    }
    inline static void set(FASTLED_REGISTER port_t val) __attribute__((always_inline)) {
        *lpc_detail::lpc_gpio_pin() = val;
    }

    inline static void strobe() __attribute__((always_inline)) { toggle(); toggle(); }
    inline static void toggle() __attribute__((always_inline)) { *lpc_detail::lpc_gpio_not() = _MASK; }

    inline static void hi(FASTLED_REGISTER port_ptr_t /*port*/) __attribute__((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t /*port*/) __attribute__((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__((always_inline)) {
        *port = val;
    }

    inline static port_t hival() __attribute__((always_inline)) { return *lpc_detail::lpc_gpio_pin() |  _MASK; }
    inline static port_t loval() __attribute__((always_inline)) { return *lpc_detail::lpc_gpio_pin() & ~_MASK; }
    // Expose PIN[0] as the "port" so the clockless C++ driver can derive the
    // SET / CLR offsets at compile time (it uses HI_OFFSET / LO_OFFSET as
    // byte offsets from this pointer).
    inline static port_ptr_t port()  __attribute__((always_inline)) { return lpc_detail::lpc_gpio_pin(); }
    inline static port_ptr_t sport() __attribute__((always_inline)) { return lpc_detail::lpc_gpio_set(); }
    inline static port_ptr_t cport() __attribute__((always_inline)) { return lpc_detail::lpc_gpio_clr(); }
    inline static port_t     mask()  __attribute__((always_inline)) { return _MASK; }

    inline static bool isset() __attribute__((always_inline)) {
        return (*lpc_detail::lpc_gpio_pin() & _MASK) != 0;
    }
};

#define _FL_DEFPIN(PIN) template<> class FastPin<PIN> : public _ARMPIN<PIN, (1u << (PIN))> {};

// LPC845 (HVQFN48 / HVQFN33 / TSSOP20) and LPC804 expose PIO0_0..PIO0_31.
// Pin availability per package is enforced by hardware; the template just
// instantiates the full 32-pin grid and the linker keeps only what is used.
#define MAX_PIN 31
_FL_DEFPIN(0);  _FL_DEFPIN(1);  _FL_DEFPIN(2);  _FL_DEFPIN(3);
_FL_DEFPIN(4);  _FL_DEFPIN(5);  _FL_DEFPIN(6);  _FL_DEFPIN(7);
_FL_DEFPIN(8);  _FL_DEFPIN(9);  _FL_DEFPIN(10); _FL_DEFPIN(11);
_FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14); _FL_DEFPIN(15);
_FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23);
_FL_DEFPIN(24); _FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27);
_FL_DEFPIN(28); _FL_DEFPIN(29); _FL_DEFPIN(30); _FL_DEFPIN(31);

#define HAS_HARDWARE_PIN_SUPPORT

}  // namespace fl

#ifdef FL_LPC_GPIO_HW
#undef FL_LPC_GPIO_HW
#endif

#endif  // FL_IS_ARM_LPC_845 || FL_IS_ARM_LPC_804 || FL_IS_ARM_LPC_11_USB || FL_IS_ARM_LPC_15

FL_DISABLE_WARNING_POP

#endif  // __INC_FASTPIN_ARM_LPC_H
