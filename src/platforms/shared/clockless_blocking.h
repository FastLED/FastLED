#pragma once

/// @file clockless_blocking.h
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

#include "fl/fastpin.h"
#include "fl/delay.h"
#include "fl/chipsets/led_timing.h"
#include "fl/compiler_control.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/log.h"
#include "controller.h"
#include "pixel_iterator.h"
#include "crgb.h"
#include "fastled_delay.h"

namespace fl {

/// Generic blocking clockless controller
///
/// This is the software-based blocking implementation that works on all platforms.
/// Platform-specific drivers (ClocklessRMT, ClocklessSPI, etc.) provide hardware-accelerated
/// alternatives when available.
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
///   - Delay (T1 + T2) nanoseconds
///   - Set line LOW
///   - Delay T3 nanoseconds
///
/// For a '0' bit:
///   - Set line HIGH
///   - Delay T1 nanoseconds
///   - Set line LOW
///   - Delay (T2 + T3) nanoseconds
///
/// Total bit time = T1 + T2 + T3 (for both 0 and 1)
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessBlockingGeneric : public CPixelLEDController<RGB_ORDER>
{
private:
    // Reference timing values from the TIMING type
    enum : uint32_t {
        T1 = TIMING::T1,
        T2 = TIMING::T2,
        T3 = TIMING::T3
    };

    // Verify pin is valid
    // static_assert(fl::FastPin<DATA_PIN>::validpin(), "Invalid pin for clockless controller");

    // Static timing assertions for protocol sanity
    static_assert(T1 > 0, "T1 (high time for bit 0) must be positive");
    static_assert(T2 > 0, "T2 (additional high time for bit 1) must be positive");
    static_assert(T3 > 0, "T3 (addtional time for low tail duration) must be positive");

    // Minimum wait time between frames
    CMinWait<WAIT_TIME> mWait;

public:
    virtual void init() {
        // Warn once that generic fallback controller is being used
        // This may indicate missing platform-specific optimizations
        static bool warned = false;  // okay static in header
        if (!warned) {
            FL_WARN("Using GENERIC fallback clockless controller - platform-specific driver not available!");
            FL_WARN("  This may result in reduced performance or timing issues.");
            FL_WARN("  Expected platforms (ESP32/Teensy/etc) should use hardware drivers.");
            warned = true;
        }

        // Initialize pin as output
        fl::FastPin<DATA_PIN>::setOutput();
        // Ensure line starts low (idle state)
        fl::FastPin<DATA_PIN>::lo();
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
        fl::FastPin<DATA_PIN>::lo();
        fl::delayNanoseconds<280000>();  // 280 microseconds
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

    /// Send a '1' bit with correct WS2812 timing
    /// T1H (high time for 1-bit) = T1+T2, T1L (low time) = T3
    FASTLED_FORCE_INLINE static void sendBit1()
    {
        // Set line HIGH
        fl::FastPin<DATA_PIN>::hi();
        // Hold high for T1+T2 nanoseconds (e.g., 875ns for WS2812)
        fl::delayNanoseconds<T1 + T2>();

        // Set line LOW
        fl::FastPin<DATA_PIN>::lo();
        // Hold low for T3 nanoseconds (e.g., 375ns for WS2812)
        fl::delayNanoseconds<T3>();
    }

    /// Send a '0' bit with correct WS2812 timing
    /// T0H (high time for 0-bit) = T1, T0L (low time) = T2+T3
    FASTLED_FORCE_INLINE static void sendBit0()
    {
        // Set line HIGH
        fl::FastPin<DATA_PIN>::hi();
        // Hold high for T1 nanoseconds (e.g., 250ns for WS2812)
        fl::delayNanoseconds<T1>();

        // Set line LOW
        fl::FastPin<DATA_PIN>::lo();
        // Hold low for T2+T3 nanoseconds (e.g., 1000ns for WS2812)
        fl::delayNanoseconds<T2 + T3>();
    }
};

// Backwards compatibility aliases
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
using ClocklessBlocking = ClocklessBlockingGeneric<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;

#ifndef FASTLED_CLOCKLESS_STUB_DEFINED
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
using ClocklessBlockController = ClocklessBlockingGeneric<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
#endif

// Note: ClocklessController alias selection is now handled by fl/clockless_controller_impl.h
// which checks both FL_CLOCKLESS_CONTROLLER_DEFINED and FASTLED_CLOCKLESS_STUB_DEFINED flags

}  // namespace fl
