// IWYU pragma: private

/// @file rp2040_pin.h
/// @brief RP2040 SIO pin template and software SPI policy. The includer supplies
/// the SDK declarations (sio_hw, gpio_set_function), which keeps this file
/// testable on a host.

#ifndef __RP2040_PIN_H
#define __RP2040_PIN_H

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/type_traits.h"
#include "fl/system/fastpin_base.h"
#include "platforms/shared/spi_bitbang/bitbang_pin.h"

#if !defined(FASTLED_FORCE_SOFTWARE_PINS)

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
namespace fl {

// warning: set and fastset are not thread-safe! use with caution!
template<uint PIN, u32 _MASK> class _RP2040PIN {
public:
  typedef volatile u32 * port_ptr_t;
  typedef u32 port_t;

  inline static constexpr bool validpin() { return true; }

  inline static void setOutput() { gpio_set_function(PIN, GPIO_FUNC_SIO); sio_hw->gpio_oe_set = _MASK; }
  inline static void setInput() { gpio_set_function(PIN, GPIO_FUNC_SIO); sio_hw->gpio_oe_clr = _MASK; }

  inline static void hi() __attribute__ ((always_inline)) { sio_hw->gpio_set = _MASK; }
  inline static void lo() __attribute__ ((always_inline)) { sio_hw->gpio_clr = _MASK; }
  inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { sio_hw->gpio_out = val; }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) { sio_hw->gpio_togl = _MASK; }

  inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) { return sio_hw->gpio_out | _MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return sio_hw->gpio_out & ~_MASK; }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return &sio_hw->gpio_out; }
  inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &sio_hw->gpio_set; }
  inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &sio_hw->gpio_clr; }
  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

/// True only for FastPin specializations backed by a real _RP2040PIN.
template <uint PIN, u32 MASK>
constexpr bool isRp2040HardwarePin(const _RP2040PIN<PIN, MASK>*) { return true; }
constexpr bool isRp2040HardwarePin(const void*) { return false; }

// FL_RP_ENABLE_FAST_SOFTWARE_SPI: optimized bit-bang software SPI pin policy (default 0).
// UNTESTED ON HARDWARE: no bench evidence exists for this platform yet.
// Default 0 keeps the legacy bit-bang writer. Define it to 1 to enable the
// optimized path at your own risk.
// Set it as a build flag (-D / build_flags), not a #define in the sketch: the
// value must match in every translation unit, including FastLED's own sources.
#ifndef FL_RP_ENABLE_FAST_SOFTWARE_SPI
#define FL_RP_ENABLE_FAST_SOFTWARE_SPI 0
#endif
#if FL_RP_ENABLE_FAST_SOFTWARE_SPI
/// Opt-in optimized policy; off by default, when pins use BitbangPinDefaults
/// (legacy hi()/lo() writes). When enabled, data updates use gpio_set/gpio_clr
/// without a data branch. Both cores may write GPIO, so led_sysdefs defines
/// FAST_SPI_INTERRUPTS_WRITE_PINS and the writer keeps per-pin stores; the
/// two-store stream is reachable only for a build that removes that define and
/// owns all of SIO. kMinWriteCycles stays at its default: no SIO write latency
/// has been measured here, so existing padding is kept.
template <u8 PIN> struct BitbangPin<FastPin<PIN>> : BitbangPinDefaults<FastPin<PIN>> {
  static constexpr bool kAtomicWrite = isRp2040HardwarePin(static_cast<const FastPin<PIN>*>(nullptr));
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
#endif  // FL_RP_ENABLE_FAST_SOFTWARE_SPI

} // namespace fl
FL_DISABLE_WARNING_POP

#endif // !FASTLED_FORCE_SOFTWARE_PINS
#endif // __RP2040_PIN_H
