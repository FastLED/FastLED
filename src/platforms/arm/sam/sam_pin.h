// IWYU pragma: private

#pragma once

/// @file sam_pin.h
/// @brief Arduino Due pin templates and software SPI policy, free of vendor
/// declarations so a host can compile them. Register access comes from the
/// template parameters.

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/type_traits.h"
#include "fl/system/fastpin_base.h"
#include "fl/system/pin.h"  // For PinMode, PinValue enums
#include "platforms/shared/spi_bitbang/bitbang_pin.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

namespace fl {

/// Template definition for arduino due style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
/// The registers are data register, set output register, clear output register, set data direction register
template<u8 PIN, u32 _MASK, typename _PDOR, typename _PSOR, typename _PCOR, typename _PDDR> class _DUEPIN {
public:
	typedef volatile u32 * port_ptr_t;
	typedef u32 port_t;

	inline static void setOutput() { pinMode(PIN, PinMode::Output); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
	inline static void setInput() { pinMode(PIN, PinMode::Input); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

	inline static void hi() __attribute__ ((always_inline)) { _PSOR::r() = _MASK; }
	inline static void lo() __attribute__ ((always_inline)) { _PCOR::r() = _MASK; }
	inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { _PDOR::r() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle();  }

	inline static void toggle() __attribute__ ((always_inline)) { _PDOR::r() ^= _MASK; }

	inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
	inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

	inline static port_t hival() __attribute__ ((always_inline)) { return _PDOR::r() | _MASK; }
	inline static port_t loval() __attribute__ ((always_inline)) { return _PDOR::r() & ~_MASK; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_PDOR::r(); }
	inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_PSOR::r(); }
	inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_PCOR::r(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }

	static constexpr bool validpin() { return true; }
};


/// Template definition for DUE  style ARM pins using bit banding, providing direct access to the various GPIO registers.  GCC
/// does a poor job of optimizing around these accesses so they are not being used just yet.
template<u8 PIN, u32 _BIT, typename _PDOR, typename _PSOR, typename _PCOR, typename _PDDR> class _DUEPIN_BITBAND {
public:
	typedef volatile u32 * port_ptr_t;
	typedef u32 port_t;

	inline static void setOutput() { pinMode(PIN, PinMode::Output); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
	inline static void setInput() { pinMode(PIN, PinMode::Input); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

	inline static void hi() __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() = 1; }
	inline static void lo() __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() = 0; }
	inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

	inline static void toggle() __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() ^= 1; }

	inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi();  }
	inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

	inline static port_t hival() __attribute__ ((always_inline)) { return 1; }
	inline static port_t loval() __attribute__ ((always_inline)) { return 0; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return _PDOR::template rx<_BIT>(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return 1; }

	static constexpr bool validpin() { return true; }
};

/// True only for FastPin specializations backed by a real _DUEPIN. FastPinBB and
/// its one-bit aliases never match.
template <u8 PIN, u32 MASK, typename PDOR, typename PSOR, typename PCOR, typename PDDR>
constexpr bool isDueHardwarePin(const _DUEPIN<PIN, MASK, PDOR, PSOR, PCOR, PDDR>*) { return true; }
constexpr bool isDueHardwarePin(const void*) { return false; }

// FL_SAM_ENABLE_FAST_SOFTWARE_SPI: optimized bit-bang software SPI pin policy (default 0).
// UNTESTED ON HARDWARE: no bench evidence exists for this platform yet.
// Default 0 keeps the legacy bit-bang writer. Define it to 1 to enable the
// optimized path at your own risk.
// Set it as a build flag (-D / build_flags), not a #define in the sketch: the
// value must match in every translation unit, including FastLED's own sources.
#ifndef FL_SAM_ENABLE_FAST_SOFTWARE_SPI
#define FL_SAM_ENABLE_FAST_SOFTWARE_SPI 0
#endif
#if FL_SAM_ENABLE_FAST_SOFTWARE_SPI
/// Data updates use SODR/CODR, which are atomic and do not depend on OWSR, the gate
/// on ODSR writes. Streaming stays off: its whole-port stores are unmeasured here
/// and would overwrite concurrent changes to other pins.
/// kMinWriteCycles stays at its default: no GPIO write latency has been measured
/// here, so existing padding is kept.
template <u8 PIN> struct BitbangPin<FastPin<PIN>> : BitbangPinDefaults<FastPin<PIN>> {
    static constexpr bool kAtomicWrite = isDueHardwarePin(static_cast<const FastPin<PIN>*>(nullptr));
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
#endif  // FL_SAM_ENABLE_FAST_SOFTWARE_SPI

} // namespace fl

FL_DISABLE_WARNING_POP
