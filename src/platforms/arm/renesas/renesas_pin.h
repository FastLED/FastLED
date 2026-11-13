// IWYU pragma: private

/// @file renesas_pin.h
/// @brief Renesas RA pin template and software SPI policy. The includer supplies
/// the BSP declarations, which keeps this file compilable on a host.

#ifndef __INC_RENESAS_PIN_H
#define __INC_RENESAS_PIN_H

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/type_traits.h"
#include "fl/system/fastpin_base.h"
#include "fl/system/pin.h"  // For PinMode, PinValue enums
#include "platforms/shared/spi_bitbang/bitbang_pin.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
namespace fl {

/// Template definition for Renesas RA pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
/// The registers are data output, set output, clear output, toggle output, input, and direction

template<u8 PIN, bsp_io_port_pin_t bspPin, u32 _PORT> class _ARMPIN {
public:

    typedef volatile u16 * port_ptr_t;
    typedef u16 port_t;

    /// Pin is valid for use
    constexpr static bool validpin() { return true; }
    /// Low speed only recommended
    constexpr static bool LowSpeedOnlyRecommended() { return false; }

    #define PORT ((R_PORT0_Type*)(_PORT))
    #define digitalBspPinToPort(P)		   (P >> 8)
    #define digitalBspPinToBitMask(P)      (1 << (P & 0xFF))

    inline static void setOutput() { pinMode(PIN, PinMode::Output); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
    inline static void setInput() { pinMode(PIN, PinMode::Input); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

    inline static void hi() __attribute__ ((always_inline)) { PORT->POSR = digitalBspPinToBitMask(bspPin); }
    inline static void lo() __attribute__ ((always_inline)) { PORT->PORR = digitalBspPinToBitMask(bspPin); }
    inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { PORT->PODR = val; }

    inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

    inline static void toggle() __attribute__ ((always_inline)) { PORT->PODR & digitalBspPinToBitMask(bspPin) ? lo() : hi(); }

    inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

    inline static port_t hival() __attribute__ ((always_inline)) { return PORT->PODR | digitalBspPinToBitMask(bspPin); }
    inline static port_t loval() __attribute__ ((always_inline)) { return PORT->PODR & ~digitalBspPinToBitMask(bspPin); }
    inline static port_ptr_t port() __attribute__ ((always_inline)) { return &PORT->PODR; }
    inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &PORT->POSR; }
    inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &PORT->PORR; }
    inline static port_t mask() __attribute__ ((always_inline)) { return digitalBspPinToBitMask(bspPin); }

};

#undef digitalBspPinToBitMask
#undef digitalBspPinToPort
#undef PORT

/// True only for FastPin specializations backed by a real Renesas _ARMPIN.
template <u8 PIN, bsp_io_port_pin_t BSP, u32 PORT_ADDR>
constexpr bool isRenesasHardwarePin(const _ARMPIN<PIN, BSP, PORT_ADDR>*) { return true; }
constexpr bool isRenesasHardwarePin(const void*) { return false; }

// FL_RENESAS_ENABLE_FAST_SOFTWARE_SPI: optimized bit-bang software SPI pin policy (default 0).
// UNTESTED ON HARDWARE: no bench evidence exists for this platform yet.
// Default 0 keeps the legacy bit-bang writer. Define it to 1 to enable the
// optimized path at your own risk.
// Set it as a build flag (-D / build_flags), not a #define in the sketch: the
// value must match in every translation unit, including FastLED's own sources.
#ifndef FL_RENESAS_ENABLE_FAST_SOFTWARE_SPI
#define FL_RENESAS_ENABLE_FAST_SOFTWARE_SPI 0
#endif
#if FL_RENESAS_ENABLE_FAST_SOFTWARE_SPI
/// Exclusive same-PODR bulk writes stream through the 16-bit PODR; all other data
/// updates use POSR/PORR at their native 16-bit width. kMinWriteCycles stays at its
/// default: no GPIO write latency has been measured, so existing padding is kept.
template <u8 PIN> struct BitbangPin<FastPin<PIN>> : BitbangPinDefaults<FastPin<PIN>> {
    static constexpr bool kAtomicWrite = isRenesasHardwarePin(static_cast<const FastPin<PIN>*>(nullptr));
    static constexpr bool kTwoWriteStream = kAtomicWrite;
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void write(u8 value) FL_NO_EXCEPT {
        writeImpl<BIT>(value, fl::integral_constant<bool, kAtomicWrite>());
    }

  private:
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void writeImpl(u8 value, fl::integral_constant<bool, true>) FL_NO_EXCEPT {
        writeBitSetClear<BIT>(value, FastPin<PIN>::mask(), FastPin<PIN>::sport(), FastPin<PIN>::cport());
    }
    // A user-defined FastPin need not have sport()/cport(), so it keeps hi()/lo().
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void writeImpl(u8 value, fl::integral_constant<bool, false>) FL_NO_EXCEPT {
        BitbangPinDefaults<FastPin<PIN>>::template write<BIT>(value);
    }
};
#endif  // FL_RENESAS_ENABLE_FAST_SOFTWARE_SPI

} // namespace fl
FL_DISABLE_WARNING_POP

#endif // __INC_RENESAS_PIN_H
