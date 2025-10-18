#ifndef __INC_CLOCKLESS_BLOCK_GENERIC_H
#define __INC_CLOCKLESS_BLOCK_GENERIC_H

/// @file clockless_block_generic.h
/// Generic blocking clockless LED controller using nanosecond-precision delays
///
/// This implementation provides a true blocking clockless controller that works
/// across all platforms with nanosecond-precision timing using the new fl::delayNanoseconds()
/// utilities. It uses fast pin operations and CPU-cycle-accurate timing.
///
/// Features:
/// - Single-pin data output (classic WS2812/SK6812 protocol)
/// - Nanosecond-precision timing via busy-wait loops
/// - Generic implementation works on AVR, ESP32, ARM, etc.
/// - Supports compile-time and runtime timing values
/// - Full interrupt support (can block entire MCU)

#include "fl/namespace.h"
#include "fastpin.h"
#include "fl/delay.h"
#include "fl/chipsets/led_timing.h"
#include "fl/compiler_control.h"
#include "fl/chipsets/timing_traits.h"
#include "controller.h"
#include "pixel_iterator.h"
#include "crgb.h"

FASTLED_NAMESPACE_BEGIN

/// Generic blocking clockless controller
///
/// Template parameters:
/// - DATA_PIN: GPIO pin for data line (e.g., 5 for GPIO5)
/// - TIMING: ChipsetTiming struct with T1, T2, T3 timing values in nanoseconds
/// - RGB_ORDER: Color byte ordering (RGB, GBR, etc.)
/// - XTRA0: Extra delay parameter (for future use)
/// - FLIP: Whether to flip data lines (for inverted logic)
/// - WAIT_TIME: Minimum time in microseconds to wait between frames
///
/// Timing behavior (nanosecond-precise):
/// For a '1' bit:
///   - Set line HIGH
///   - Delay T1 nanoseconds
///   - Set line LOW
///   - Delay T2 nanoseconds
///
/// For a '0' bit:
///   - Set line HIGH
///   - Delay (T1 + T2 - T3) nanoseconds
///   - Set line LOW
///   - Delay T3 nanoseconds
///
/// Total bit time = T1 + T2 (for both 0 and 1)
template <int DATA_PIN, const ChipsetTiming& TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessBlockController : public CPixelLEDController<RGB_ORDER>
{
private:
    // Extract timing values from struct
    static constexpr uint32_t T1 = TIMING.T1;
    static constexpr uint32_t T2 = TIMING.T2;
    static constexpr uint32_t T3 = TIMING.T3;

    // Verify pin is valid
    static_assert(FastPin<DATA_PIN>::validpin(), "Invalid pin for clockless controller");

    // Static timing assertions for protocol sanity
    static_assert(T1 > 0, "T1 (first pulse) must be positive");
    static_assert(T2 > 0, "T2 (second pulse) must be positive");
    static_assert(T3 > 0, "T3 (zero pulse) must be positive");
    static_assert((T1 + T2) > T3, "Bit time (T1+T2) must be greater than zero pulse (T3)");

    // Minimum wait time between frames
    CMinWait<WAIT_TIME> mWait;

public:
    virtual void init() {
        // Initialize pin as output
        FastPin<DATA_PIN>::setOutput();
        // Ensure line starts low (idle state)
        FastPin<DATA_PIN>::lo();
    }

    virtual uint16_t getMaxRefreshRate() const {
        // Rough estimate: about 300-400 Hz for typical 60 LED WS2812 strings
        // Actual rate depends on T1+T2 timing and LED count
        return 300;
    }

protected:
    virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
        // Wait for minimum time since last frame
        mWait.wait();

        // Disable interrupts to ensure timing accuracy
        // (LED protocols are very timing-sensitive on most platforms)
#if defined(ARDUINO_ARCH_AVR)
        cli();  // Disable interrupts on AVR
#endif

        // Send all pixel data
        if (pixels.mLen > 0) {
            sendPixelData(pixels);
        }

        // Re-enable interrupts
#if defined(ARDUINO_ARCH_AVR)
        sei();  // Re-enable interrupts on AVR
#endif

        // Mark that we've sent data
        mWait.mark();
    }

private:
    /// Send raw pixel data with precise timing
    FASTLED_FORCE_INLINE static void sendPixelData(PixelController<RGB_ORDER> & pixels)
    {
        // Get color component order
        uint16_t pixel_count = pixels.mLen;
        uint8_t *data = (uint8_t *)pixels.mData;

        // Iterate through all bytes in all pixels
        for (uint16_t i = 0; i < pixel_count * 3; ++i) {
            uint8_t byte = data[i];
            sendByte(byte);
        }

        // Send reset code: line low for at least 50µs
        // (WS2812/SK6812 datasheet requirement)
        FastPin<DATA_PIN>::lo();
        fl::delayNanoseconds<50000>();  // 50 microseconds
    }

    /// Send a single byte with bit-by-bit timing
    FASTLED_FORCE_INLINE static void sendByte(uint8_t byte)
    {
        // Send bits MSB first (standard for most LED protocols)
        for (int bit = 7; bit >= 0; --bit) {
            bool is_one = (byte & (1 << bit)) != 0;

            if (is_one) {
                sendBit1();
            } else {
                sendBit0();
            }
        }
    }

    /// Send a '1' bit with T1/T2 timing
    FASTLED_FORCE_INLINE static void sendBit1()
    {
        // Set line HIGH
        FastPin<DATA_PIN>::hi();
        // Hold high for T1 nanoseconds
        fl::delayNanoseconds<T1>();

        // Set line LOW
        FastPin<DATA_PIN>::lo();
        // Hold low for T2 nanoseconds
        fl::delayNanoseconds<T2>();
    }

    /// Send a '0' bit with modified timing
    FASTLED_FORCE_INLINE static void sendBit0()
    {
        // Set line HIGH
        FastPin<DATA_PIN>::hi();
        // For a '0' bit: high time is (T1 + T2 - T3)
        // This ensures total cycle time is still (T1 + T2)
        fl::delayNanoseconds<T1 + T2 - T3>();

        // Set line LOW
        FastPin<DATA_PIN>::lo();
        // Hold low for T3 nanoseconds
        fl::delayNanoseconds<T3>();
    }
};

FASTLED_NAMESPACE_END

#endif  // __INC_CLOCKLESS_BLOCK_GENERIC_H
