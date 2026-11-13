#pragma once

// IWYU pragma: private

/// @file stm32_bitbang_pin.h
/// @brief Software SPI policy for STM32 pin templates that expose a 32-bit BSRR.

#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/type_traits.h"
#include "fl/system/fastpin_base.h"
#include "platforms/shared/spi_bitbang/bitbang_pin.h"

namespace fl {

/// Base of pin templates providing writeBsrr(), mask() and hi()/lo(). The legacy
/// armpin.h template is not one: its F2 path has 16-bit BSRRL/BSRRH registers.
struct Stm32BsrrPin {};

constexpr bool isStm32BsrrPin(const Stm32BsrrPin*) { return true; }
constexpr bool isStm32BsrrPin(const void*) { return false; }

// FL_STM32_ENABLE_FAST_SOFTWARE_SPI: optimized bit-bang software SPI pin policy (default 0).
// UNTESTED ON HARDWARE: no bench evidence exists for this platform yet.
// Default 0 keeps the legacy bit-bang writer. Define it to 1 to enable the
// optimized path at your own risk.
// Set it as a build flag (-D / build_flags), not a #define in the sketch: the
// value must match in every translation unit, including FastLED's own sources.
#ifndef FL_STM32_ENABLE_FAST_SOFTWARE_SPI
#define FL_STM32_ENABLE_FAST_SOFTWARE_SPI 0
#endif
#if FL_STM32_ENABLE_FAST_SOFTWARE_SPI
/// Data updates use one BSRR store whose value selects set (low half) or reset
/// (high half), so no family needs a separate clear address and no data-dependent
/// branch is required. Streaming and kMinWriteCycles stay at their defaults, as no
/// GPIO write latency has been measured here. A FastPin deriving from a BSRR
/// template gets these stores even if it shadows hi()/lo().
template <u8 PIN> struct BitbangPin<FastPin<PIN>> : BitbangPinDefaults<FastPin<PIN>> {
    static constexpr bool kAtomicWrite = isStm32BsrrPin(static_cast<const FastPin<PIN>*>(nullptr));
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void write(u8 value) FL_NO_EXCEPT {
        writeImpl<BIT>(value, fl::integral_constant<bool, kAtomicWrite>());
    }

  private:
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void writeImpl(u8 value, fl::integral_constant<bool, true>) FL_NO_EXCEPT {
        const u32 resetShift = static_cast<u32>((~value >> BIT) & 1U) << 4; // 0 or 16.
        FastPin<PIN>::writeBsrr(static_cast<u32>(FastPin<PIN>::mask()) << resetShift);
    }
    // A user-defined FastPin need not have writeBsrr(), so it keeps hi()/lo().
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void writeImpl(u8 value, fl::integral_constant<bool, false>) FL_NO_EXCEPT {
        BitbangPinDefaults<FastPin<PIN>>::template write<BIT>(value);
    }
};
#endif  // FL_STM32_ENABLE_FAST_SOFTWARE_SPI

} // namespace fl
