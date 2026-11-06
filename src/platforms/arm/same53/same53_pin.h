// IWYU pragma: private

#pragma once

/// @file same53_pin.h
/// @brief ClearCore pin templates and software SPI policy. The includer supplies
/// the vendor PORT declaration from <sam.h>, which keeps this file testable on a
/// host.

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/type_traits.h"
#include "fl/system/fastpin_base.h"
#include "fl/system/pin.h"
#include "platforms/shared/spi_bitbang/bitbang_pin.h"

namespace fl {

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT
#else

/// ClearCore IO0-IO3 direct GPIO access.
///
/// The connector output stages are active-low: a cleared SAME53 GPIO bit is a
/// logical HIGH at the ClearCore connector. setOutput() delegates to Teknic's
/// Arduino core first so connector mode, muxing, and protection state are
/// initialized before FastLED writes the GPIO register directly.
template <u8 PIN, u8 BIT>
class ClearCoreFastPin : public ValidPinBase {
  public:
    typedef volatile u32 *port_ptr_t;
    typedef u32 port_t;

    inline static void setOutput() { pinMode(PIN, PinMode::Output); }
    inline static void setInput() { pinMode(PIN, PinMode::Input); }

    inline static void hi() FL_NO_EXCEPT __attribute__((always_inline)) {
        PORT->Group[0].OUTCLR.reg = mask();
    }
    inline static void lo() FL_NO_EXCEPT __attribute__((always_inline)) {
        PORT->Group[0].OUTSET.reg = mask();
    }
    inline static void set(port_t val) FL_NO_EXCEPT
        __attribute__((always_inline)) {
        PORT->Group[0].OUT.reg = val;
    }
    inline static void strobe() FL_NO_EXCEPT __attribute__((always_inline)) {
        toggle();
        toggle();
    }
    inline static void toggle() FL_NO_EXCEPT __attribute__((always_inline)) {
        PORT->Group[0].OUTTGL.reg = mask();
    }
    inline static void hi(port_ptr_t /*port*/) FL_NO_EXCEPT
        __attribute__((always_inline)) {
        hi();
    }
    inline static void lo(port_ptr_t /*port*/) FL_NO_EXCEPT
        __attribute__((always_inline)) {
        lo();
    }
    inline static void fastset(port_ptr_t port, port_t val) FL_NO_EXCEPT
        __attribute__((always_inline)) {
        *port = val;
    }
    inline static port_t hival() FL_NO_EXCEPT
        __attribute__((always_inline)) {
        return PORT->Group[0].OUT.reg & ~mask();
    }
    inline static port_t loval() FL_NO_EXCEPT
        __attribute__((always_inline)) {
        return PORT->Group[0].OUT.reg | mask();
    }
    inline static port_ptr_t port() FL_NO_EXCEPT
        __attribute__((always_inline)) {
        return &PORT->Group[0].OUT.reg;
    }
    inline static port_ptr_t sport() FL_NO_EXCEPT
        __attribute__((always_inline)) {
        return &PORT->Group[0].OUTCLR.reg;
    }
    inline static port_ptr_t cport() FL_NO_EXCEPT
        __attribute__((always_inline)) {
        return &PORT->Group[0].OUTSET.reg;
    }
    inline static port_t mask() FL_NO_EXCEPT
        __attribute__((always_inline)) {
        return 1UL << BIT;
    }
};

// Teknic ClearCore 1.7.4 HardwareMapping.h:
// IO0=PA00, IO1=PA01, IO2=PA06, IO3=PA07. IO4/IO5 use H-bridge output
// stages and intentionally remain unavailable pending hardware validation.
template <> class FastPin<0> : public ClearCoreFastPin<0, 0> {};
template <> class FastPin<1> : public ClearCoreFastPin<1, 1> {};
template <> class FastPin<2> : public ClearCoreFastPin<2, 6> {};
template <> class FastPin<3> : public ClearCoreFastPin<3, 7> {};

/// True only for pins backed by the active-low ClearCore template.
template <u8 PIN, u8 BIT>
constexpr bool isClearCoreHardwarePin(const ClearCoreFastPin<PIN, BIT>*) { return true; }
constexpr bool isClearCoreHardwarePin(const void*) { return false; }

/// The generic writer's whole-port words treat a set clock bit as logical high,
/// which is backwards on these connectors. Atomic writes keep every path on the
/// polarity-correct hi()/lo() and sport()/cport(), so streaming stays off until a
/// polarity-aware stream exists. kMinWriteCycles stays at its default: no GPIO
/// write latency has been measured here, so existing padding is kept.
template <u8 PIN> struct BitbangPin<FastPin<PIN>> : BitbangPinDefaults<FastPin<PIN>> {
    static constexpr bool kAtomicWrite = isClearCoreHardwarePin(static_cast<const FastPin<PIN>*>(nullptr));
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void write(u8 value) FL_NO_EXCEPT {
        writeImpl<BIT>(value, fl::integral_constant<bool, kAtomicWrite>());
    }

  private:
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void writeImpl(u8 value, fl::integral_constant<bool, true>) FL_NO_EXCEPT {
        // sport() is OUTCLR: a logical one clears the GPIO bit.
        writeBitSetClear<BIT>(value, FastPin<PIN>::mask(), FastPin<PIN>::sport(), FastPin<PIN>::cport());
    }
    // A user-defined FastPin need not have sport()/cport(), so it keeps hi()/lo().
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void writeImpl(u8 value, fl::integral_constant<bool, false>) FL_NO_EXCEPT {
        BitbangPinDefaults<FastPin<PIN>>::template write<BIT>(value);
    }
};

#define MAX_PIN 3
#define HAS_HARDWARE_PIN_SUPPORT 1

#endif // FASTLED_FORCE_SOFTWARE_PINS

} // namespace fl
