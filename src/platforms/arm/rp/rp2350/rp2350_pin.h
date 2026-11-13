// IWYU pragma: private

/// @file rp2350_pin.h
/// @brief RP2350 SIO pin template and software SPI policy. The includer supplies
/// the SDK declarations (sio_hw, gpio_set_function), which keeps this file
/// testable on a host.

#ifndef __RP2350_PIN_H
#define __RP2350_PIN_H

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
// GPIO0-31 use the SIO GPIO registers. GPIO32-47 (RP2350B) use the GPIO_HI
// registers, where bit (PIN - 32) is the pin; _MASK is that bit (#5100).
template<uint PIN, u32 _MASK> class _RP2350PIN {
  static constexpr bool kHiBank = PIN >= 32;
  static volatile u32* outReg() { return kHiBank ? &sio_hw->gpio_hi_out : &sio_hw->gpio_out; }
  static volatile u32* setReg() { return kHiBank ? &sio_hw->gpio_hi_set : &sio_hw->gpio_set; }
  static volatile u32* clrReg() { return kHiBank ? &sio_hw->gpio_hi_clr : &sio_hw->gpio_clr; }
  static volatile u32* toglReg() { return kHiBank ? &sio_hw->gpio_hi_togl : &sio_hw->gpio_togl; }
  static volatile u32* oeSetReg() { return kHiBank ? &sio_hw->gpio_hi_oe_set : &sio_hw->gpio_oe_set; }
  static volatile u32* oeClrReg() { return kHiBank ? &sio_hw->gpio_hi_oe_clr : &sio_hw->gpio_oe_clr; }

public:
  typedef volatile u32 * port_ptr_t;
  typedef u32 port_t;

  inline static constexpr bool validpin() { return true; }

  inline static void setOutput() { gpio_set_function(PIN, GPIO_FUNC_SIO); *oeSetReg() = _MASK; }
  inline static void setInput() { gpio_set_function(PIN, GPIO_FUNC_SIO); *oeClrReg() = _MASK; }

  inline static void hi() __attribute__ ((always_inline)) { *setReg() = _MASK; }
  inline static void lo() __attribute__ ((always_inline)) { *clrReg() = _MASK; }
  inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *outReg() = val; }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) { *toglReg() = _MASK; }

  inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) { return *outReg() | _MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return *outReg() & ~_MASK; }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return outReg(); }
  inline static port_ptr_t sport() __attribute__ ((always_inline)) { return setReg(); }
  inline static port_ptr_t cport() __attribute__ ((always_inline)) { return clrReg(); }
  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

/// True only for GPIO0-31 backed by a real _RP2350PIN. GPIO32-47 (GPIO_HI bank)
/// keep the plain hi()/lo() writer: the set/clear stream is only audited for bank 0.
template <uint PIN, u32 MASK>
constexpr bool isRp2350LowBankPin(const _RP2350PIN<PIN, MASK>*) { return PIN < 32; }
constexpr bool isRp2350LowBankPin(const void*) { return false; }

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
/// (legacy hi()/lo() writes). When enabled, data updates use gpio_set/gpio_clr.
/// Both cores may write GPIO, so led_sysdefs defines
/// FAST_SPI_INTERRUPTS_WRITE_PINS and the writer keeps per-pin stores; the
/// two-store stream is reachable only by editing that define out for a build
/// that owns all of SIO. kMinWriteCycles stays at its default:
/// no SIO write latency has been measured here, so existing padding is kept.
template <u8 PIN> struct BitbangPin<FastPin<PIN>> : BitbangPinDefaults<FastPin<PIN>> {
  static constexpr bool kAtomicWrite = isRp2350LowBankPin(static_cast<const FastPin<PIN>*>(nullptr));
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
#endif  // FL_RP_ENABLE_FAST_SOFTWARE_SPI

} // namespace fl
FL_DISABLE_WARNING_POP

#endif // !FASTLED_FORCE_SOFTWARE_PINS
#endif // __RP2350_PIN_H
