/// @file platforms/fast_pins.h
/// @brief Platform-agnostic API for simultaneous multi-pin GPIO control
///
/// This header provides a fast LUT-based API for controlling up to 8 GPIO pins
/// simultaneously with a single write operation. It delegates to platform-specific
/// implementations that leverage hardware SET/CLEAR registers where available.
///
/// Key Features:
/// - 256-entry LUT for 8-pin groups (or smaller LUTs for fewer pins)
/// - Write-only operations (no GPIO reads)
/// - Zero-branching pin writes
/// - Platform-optimized implementations
///
/// Usage Example:
/// @code
///   fl::FastPins<4> writer;
///   writer.setPins(2, 3, 5, 7);    // Configure 4 pins
///   writer.write(0b1010);          // Set pins 2,5 HIGH; 3,7 LOW
/// @endcode
///
/// Platform Support:
/// - ESP32: W1TS/W1TC registers (optimal)
/// - RP2040/RP2350: SIO gpio_set/gpio_clr (optimal)
/// - STM32: BSRR register (optimal)
/// - Teensy 4.x: GPIO_DR_SET/CLEAR (optimal)
/// - AVR & others: Software fallback

#pragma once

#include "fl/stdint.h"

namespace fl {

/// LUT entry for fast-pins operations
/// Contains pre-computed masks for atomic SET and CLEAR operations
struct FastPinsMaskEntry {
    uint32_t set_mask;    ///< Pins to set HIGH
    uint32_t clear_mask;  ///< Pins to clear LOW
};

/// Fast-pins controller supporting up to 8 pins with LUT-based writes
///
/// This class provides zero-overhead multi-pin GPIO writes using a pre-computed
/// lookup table. Each bit in the write pattern corresponds to one configured pin.
///
/// @tparam MAX_PINS Maximum pins in group (1-8)
///                  - 8 pins = 256 LUT entries (2KB)
///                  - 4 pins = 16 LUT entries (128 bytes)
///
/// Performance: ~30-40ns per write on platforms with hardware SET/CLEAR registers
///
/// Memory: LUT size = 2^MAX_PINS * 8 bytes
///
/// Thread Safety: Not thread-safe. Use separate instances or external locking.
template<uint8_t MAX_PINS = 8>
class FastPins {
    static_assert(MAX_PINS >= 1 && MAX_PINS <= 8, "MAX_PINS must be 1-8");

public:
    /// Number of LUT entries = 2^MAX_PINS
    static constexpr uint16_t LUT_SIZE = 1 << MAX_PINS;

    /// Default constructor
    FastPins() = default;

    /// Configure pins using variadic template (compile-time pin count check)
    ///
    /// @param pins Pin numbers to control (must be <= MAX_PINS)
    ///
    /// Example:
    /// @code
    ///   FastPins<4> writer;
    ///   writer.setPins(2, 3, 5, 7);  // 4 pins
    /// @endcode
    template<typename... Pins>
    void setPins(Pins... pins) {
        uint8_t pin_array[] = { static_cast<uint8_t>(pins)... };
        mPinCount = sizeof...(pins);
        static_assert(sizeof...(pins) <= MAX_PINS, "Too many pins for this FastPins instance");
        buildLUT(pin_array, mPinCount);
    }

    /// Write bit pattern to configured pins using pre-computed LUT
    ///
    /// Each bit position corresponds to a pin (LSB = first pin).
    /// This is a write-only operation - no GPIO reads are performed.
    ///
    /// @param pattern Bit pattern to write (0-255 for 8 pins)
    ///
    /// Performance: ~30-40ns on platforms with hardware SET/CLEAR registers
    ///
    /// Example:
    /// @code
    ///   writer.write(0b1010);  // Pins 0,2 HIGH; 1,3 LOW
    /// @endcode
    inline void write(uint8_t pattern) {
        const auto& entry = mLUT[pattern];
        writeImpl(entry.set_mask, entry.clear_mask);
    }

    /// Get LUT for inspection/debugging
    /// @return Pointer to LUT array
    const FastPinsMaskEntry* getLUT() const { return mLUT; }

    /// Get configured pin count
    /// @return Number of pins configured via setPins()
    uint8_t getPinCount() const { return mPinCount; }

private:
    FastPinsMaskEntry mLUT[LUT_SIZE];  ///< Pre-computed lookup table
    uint8_t mPinCount = 0;              ///< Number of configured pins

    // Platform-specific implementation (defined in platform headers)
    void writeImpl(uint32_t set_mask, uint32_t clear_mask);

    // Build LUT during setPins() (defined in platform headers)
    void buildLUT(const uint8_t* pins, uint8_t count);
};

} // namespace fl

// allow-include-after-namespace
// Include platform-specific implementation
// Each platform header defines writeImpl() and buildLUT() for FastPins<>
#if defined(ESP32)
    #include "platforms/esp/32/fast_pins_esp32.h"
#elif defined(__arm__) && (defined(PICO_RP2040) || defined(PICO_RP2350))
    #include "platforms/arm/rp/fast_pins_rp.h"
#elif defined(STM32F1) || defined(STM32F2) || defined(STM32F4)
    #include "platforms/arm/stm32/fast_pins_stm32.h"
#elif defined(FASTLED_TEENSY4) && defined(__IMXRT1062__)
    #include "platforms/arm/teensy/teensy4_common/fast_pins_teensy4.h"
#else
    // Software fallback for all other platforms (AVR, etc.)
    #include "platforms/shared/fast_pins_fallback.h"
#endif
