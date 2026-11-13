

// ok no namespace fl
#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/compiler_control.h"
#include "platforms/shared/spi_bitbang/bitbang_pin.h"

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

namespace fl {

template <fl::u8 PIN> class FastPin;

/// True only for FastPin specializations backed by a real _AVRPIN output latch.
template <fl::u8 PIN, fl::u8 MASK, typename PortReg, typename DdrReg, typename PinReg>
constexpr bool isAvrHardwarePin(const _AVRPIN<PIN, MASK, PortReg, DdrReg, PinReg>*) { return true; }
constexpr bool isAvrHardwarePin(const void*) { return false; }

// FL_AVR_ENABLE_FAST_SOFTWARE_SPI: optimized bit-bang software SPI pin policy (default 0).
// UNTESTED ON HARDWARE: no bench evidence exists for this platform yet.
// Default 0 keeps the legacy bit-bang writer. Define it to 1 to enable the
// optimized path at your own risk.
// Set it as a build flag (-D / build_flags), not a #define in the sketch: the
// value must match in every translation unit, including FastLED's own sources.
#ifndef FL_AVR_ENABLE_FAST_SOFTWARE_SPI
#define FL_AVR_ENABLE_FAST_SOFTWARE_SPI 0
#endif
#if FL_AVR_ENABLE_FAST_SOFTWARE_SPI
/// An _AVRPIN writes its whole 8-bit latch with one store, so the exclusive
/// same-port software SPI path can merge each falling edge with the next data
/// update. Forced software pins are not _AVRPINs and keep the per-bit writer.
template <fl::u8 PIN> struct BitbangPin<FastPin<PIN>> : BitbangPinDefaults<FastPin<PIN>> {
    static constexpr bool kTwoWriteStream =
        isAvrHardwarePin(static_cast<const FastPin<PIN>*>(nullptr));
};
#endif  // FL_AVR_ENABLE_FAST_SOFTWARE_SPI

} // namespace fl
