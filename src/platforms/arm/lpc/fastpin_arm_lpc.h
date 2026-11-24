// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_FASTPIN_ARM_LPC_H
#define __INC_FASTPIN_ARM_LPC_H

#include "fl/stl/bit_cast.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/type_traits.h"
#include "fl/system/fastpin_base.h"
#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/shared/spi_bitbang/bitbang_pin.h"

// fl/system/fastpin_base.h (included above) rejects forced software pins on
// LPC families without a pin backend (everything but LPC845 / LPC804).

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

// Scoped to chips that share the "modern" LPC GPIO controller (DIR / MASK /
// PIN / MPIN / SET / CLR / NOT at the same offsets from the block base):
//   - LPC8xx   (LPC845 / LPC804)           base 0xA0000000, has DIRSET/DIRCLR
//                                          (UM11029 / UM11065)
//   - LPC11Uxx (LPC11U24 / LPC11U35)       base 0x50000000, no DIRSET/DIRCLR
//                                          (UM10462 GPIO chapter; LPC_GPIO_Type
//                                          in NXP LPC11Uxx.h)
//   - LPC15xx  (LPC15{17,18,19,47,48,49})  base 0x1C000000, no DIRSET/DIRCLR
//                                          (UM10736 GPIO chapter;
//                                          LPC_GPIO_PORT_Type in NXP LPC15xx.h)
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
// LPC11Uxx and LPC15xx have no DIRSET/DIRCLR aliases: their direction is the
// DIR word only, so pin direction changes read-modify-write it.
#if defined(FL_IS_ARM_LPC_11_USB)
#define FL_LPC_GPIO_DEFAULT_BASE 0x50000000UL
#define FL_LPC_GPIO_HAS_DIRSET 0
#elif defined(FL_IS_ARM_LPC_15)
#define FL_LPC_GPIO_DEFAULT_BASE 0x1C000000UL
#define FL_LPC_GPIO_HAS_DIRSET 0
#else
#define FL_LPC_GPIO_DEFAULT_BASE 0xA0000000UL
#define FL_LPC_GPIO_HAS_DIRSET 1
#endif

#ifndef FL_LPC_GPIO_BASE
#define FL_LPC_GPIO_BASE FL_LPC_GPIO_DEFAULT_BASE
#endif

// Vendor block pointers: LPC_GPIO (LPC11Uxx.h), GPIO (LPC845.h / LPC804.h),
// LPC_GPIO_PORT (LPC15xx.h).
#if defined(LPC_GPIO)
#define FL_LPC_GPIO_HW LPC_GPIO
#elif defined(GPIO)
#define FL_LPC_GPIO_HW GPIO
#elif defined(LPC_GPIO_PORT)
#define FL_LPC_GPIO_HW LPC_GPIO_PORT
#endif

namespace lpc_detail {

constexpr u32 FL_LPC_GPIO_DIR_OFFSET    = 0x2000u;
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
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_dir() { return &FL_LPC_GPIO_HW->DIR[0]; }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_pin() { return &FL_LPC_GPIO_HW->PIN[0]; }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_set() { return &FL_LPC_GPIO_HW->SET[0]; }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_clr() { return &FL_LPC_GPIO_HW->CLR[0]; }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_not() { return &FL_LPC_GPIO_HW->NOT[0]; }
#if FL_LPC_GPIO_HAS_DIRSET
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_dirset() { return &FL_LPC_GPIO_HW->DIRSET[0]; }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_dirclr() { return &FL_LPC_GPIO_HW->DIRCLR[0]; }
#endif
#else
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_reg(u32 offset) {
    // Constructing a peripheral pointer from a numeric base + offset is the
    // canonical CMSIS pattern. `fl::bit_cast` is the FastLED-blessed alternative
    // to `reinterpret_cast` for this conversion; both lower to the same load.
    return fl::bit_cast<volatile u32*>(FL_LPC_GPIO_BASE + offset);
}
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_dir() { return lpc_gpio_reg(FL_LPC_GPIO_DIR_OFFSET); }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_pin() { return lpc_gpio_reg(FL_LPC_GPIO_PIN_OFFSET); }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_set() { return lpc_gpio_reg(FL_LPC_GPIO_SET_OFFSET); }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_clr() { return lpc_gpio_reg(FL_LPC_GPIO_CLR_OFFSET); }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_not() { return lpc_gpio_reg(FL_LPC_GPIO_NOT_OFFSET); }
#if FL_LPC_GPIO_HAS_DIRSET
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_dirset() { return lpc_gpio_reg(FL_LPC_GPIO_DIRSET_OFFSET); }
static FASTLED_FORCE_INLINE volatile u32* lpc_gpio_dirclr() { return lpc_gpio_reg(FL_LPC_GPIO_DIRCLR_OFFSET); }
#endif
#endif

// Direction backends. LPC8xx sets and clears direction bits with one store to
// DIRSET/DIRCLR. LPC11Uxx/LPC15xx only have DIR, so the update is a
// read-modify-write; like any RMW it is not atomic against an ISR that
// changes another pin's direction on the same port.
struct LpcGpioDirReadModifyWrite {
    static FASTLED_FORCE_INLINE void setOutput(u32 mask) {
        volatile u32* dir = lpc_gpio_dir();
        *dir = *dir | mask;
    }
    static FASTLED_FORCE_INLINE void setInput(u32 mask) {
        volatile u32* dir = lpc_gpio_dir();
        *dir = *dir & ~mask;
    }
};

#if FL_LPC_GPIO_HAS_DIRSET
struct LpcGpioDirSetClr {
    static FASTLED_FORCE_INLINE void setOutput(u32 mask) { *lpc_gpio_dirset() = mask; }
    static FASTLED_FORCE_INLINE void setInput(u32 mask) { *lpc_gpio_dirclr() = mask; }
};
using LpcGpioDir = LpcGpioDirSetClr;
#else
using LpcGpioDir = LpcGpioDirReadModifyWrite;
#endif

}  // namespace lpc_detail

#ifdef FL_FASTPIN_HAS_NO_PINMAP
// FASTLED_FORCE_SOFTWARE_PINS (LPC845 / LPC804 only; fastpin_base.h rejects
// the other families): keep the PIO0_0..31 grid below so unlisted pins stay
// invalid, but route every access through fl::pinMode / fl::digitalWrite
// (platforms/arm/lpc/pin_lpc.hpp) instead of the GPIO registers.
template <u8 PIN, u32 _MASK>
class _ARMPIN : public NoPinMapPin<PIN> {};
#else
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
        lpc_detail::LpcGpioDir::setOutput(_MASK);
    }
    inline static void setInput() __attribute__((always_inline)) {
        lpc_detail::LpcGpioDir::setInput(_MASK);
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

    // Reading SET returns the output latch; reading PIN returns pin state.
    inline static port_t hival() __attribute__((always_inline)) { return *lpc_detail::lpc_gpio_set() |  _MASK; }
    inline static port_t loval() __attribute__((always_inline)) { return *lpc_detail::lpc_gpio_set() & ~_MASK; }
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

/// Match FastPin specializations backed by the real modern LPC _ARMPIN.
template <class Pin, u8 PIN, u32 MASK>
constexpr BitbangPinAccess lpcBitbangPinAccess(const _ARMPIN<PIN, MASK>*) {
    return bitbangPinAccess<Pin, _ARMPIN<PIN, MASK>>();
}
template <class Pin>
constexpr BitbangPinAccess lpcBitbangPinAccess(const void*) { return BitbangPinAccess::Default; }
#endif  // FL_FASTPIN_HAS_NO_PINMAP

// FL_LPC_ENABLE_FAST_SOFTWARE_SPI: optimized bit-bang software SPI pin policy (default 1).
// Opt-out: define it to 0 to fall back to the legacy bit-bang writer.
// Set it as a build flag (-D / build_flags), not a #define in the sketch: the
// value must match in every translation unit, including FastLED's own sources.
#ifndef FL_LPC_ENABLE_FAST_SOFTWARE_SPI
#define FL_LPC_ENABLE_FAST_SOFTWARE_SPI 1
#endif
// The fast policy writes GPIO registers directly, bypassing forced core-API pins.
#if FL_LPC_ENABLE_FAST_SOFTWARE_SPI && !defined(FL_FASTPIN_HAS_NO_PINMAP)
/// Exclusive bulk writes stream through PIN[0], from a snapshot read through SET
/// because reading SET returns the output latch and reading PIN returns pin state.
/// Every other data update uses SET/CLR. kMinWriteCycles stays at its
/// default: no GPIO write latency has been measured, so existing padding is kept.
template <u8 PIN> struct BitbangPin<FastPin<PIN>> : BitbangPinDefaults<FastPin<PIN>> {
    static constexpr auto kPinAccess = lpcBitbangPinAccess<FastPin<PIN>>(static_cast<const FastPin<PIN>*>(nullptr));
    static constexpr bool kAtomicWrite = kPinAccess == BitbangPinAccess::Native;
    static constexpr bool kRequiresPinWrites = kPinAccess == BitbangPinAccess::Custom;
    static constexpr bool kTwoWriteStream = kAtomicWrite;
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void write(u8 value) FL_NO_EXCEPT {
        writeImpl<BIT>(value, fl::integral_constant<bool, kAtomicWrite>());
    }

  private:
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void writeImpl(u8 value, fl::integral_constant<bool, true>) FL_NO_EXCEPT {
        writeBitSetClearNoBranch<BIT>(value, FastPin<PIN>::mask(), FastPin<PIN>::sport(), FastPin<PIN>::cport());
    }
    // A user-defined FastPin need not have sport()/cport(), so it keeps hi()/lo().
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void writeImpl(u8 value, fl::integral_constant<bool, false>) FL_NO_EXCEPT {
        BitbangPinDefaults<FastPin<PIN>>::template write<BIT>(value);
    }
};
#endif  // FL_LPC_ENABLE_FAST_SOFTWARE_SPI && !FL_FASTPIN_HAS_NO_PINMAP

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

#ifndef FL_FASTPIN_HAS_NO_PINMAP
#define HAS_HARDWARE_PIN_SUPPORT
#endif

}  // namespace fl

#ifdef FL_LPC_GPIO_HW
#undef FL_LPC_GPIO_HW
#endif

#endif  // FL_IS_ARM_LPC_845 || FL_IS_ARM_LPC_804 || FL_IS_ARM_LPC_11_USB || FL_IS_ARM_LPC_15

FL_DISABLE_WARNING_POP

#endif  // __INC_FASTPIN_ARM_LPC_H
