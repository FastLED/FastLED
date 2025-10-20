#pragma once

/// @file clockless_ezws2812_39mhz.h
/// @brief FastLED ezWS2812 GPIO controller optimized for 39MHz Silicon Labs MGM240/MG24
///
/// This controller provides cycle-accurate WS2812 timing using direct GPIO manipulation
/// specifically optimized for 39MHz CPU frequency. All timing calculations are
/// pre-computed to avoid runtime overhead.

#include "controller.h"
#include "pixel_controller.h"
#include "eorder.h"
#include "fl/force_inline.h"
#include "fl/fastpin.h"

// Check if we're on the right platform
#if !defined(ARDUINO_ARCH_SILABS)
#error "ezWS2812 GPIO 39MHz controller is only available for Silicon Labs MGM240/MG24 platforms"
#endif
namespace fl {
/// @brief ezWS2812 GPIO controller optimized for 39MHz CPUs
///
/// This controller uses direct GPIO manipulation with pre-computed timing
/// optimized specifically for 39MHz CPU frequency. It processes entire
/// byte arrays in tight loops for maximum performance.
///
/// Current implementation: WS2812-specific timing
/// - '0' bit: 0.4µs high, 0.85µs low (1.25µs total)
/// - '1' bit: 0.8µs high, 0.45µs low (1.25µs total)
///
/// At 39MHz: 1 cycle = ~25.64ns
/// - '0' high: ~15.6 cycles, '0' low: ~33.1 cycles
/// - '1' high: ~31.2 cycles, '1' low: ~17.5 cycles
///
/// @todo FUTURE IMPROVEMENT: Make this controller generic with T1/T2/T3 timing parameters
/// This would allow it to support all clockless LED chipsets (SK6812, TM1809, UCS1903, etc.)
/// instead of being WS2812-specific. The generic ClocklessController template parameters
/// could be used: T1 (high time for '1'), T2 (high time for '0'), T3 (low time for both).
/// Pre-computed cycle counts would be calculated at compile-time from T1/T2/T3 values.
/// Example: template<uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB>
///
/// @tparam DATA_PIN GPIO pin number for LED data
/// @tparam RGB_ORDER Color channel ordering (typically GRB for WS2812)
template<u8 DATA_PIN, EOrder RGB_ORDER = GRB>
class ClocklessController_ezWS2812_GPIO_39MHz : public CPixelLEDController<RGB_ORDER> {

    /// @note IMPLEMENTATION ROADMAP for generic T1/T2/T3 support:
    /// 1. Add template parameters: template<DATA_PIN, T1, T2, T3, RGB_ORDER>
    /// 2. Calculate cycles at compile-time:
    ///    - constexpr int T1_CYCLES = (T1 * F_CPU) / 1000000000; // T1 in ns
    ///    - constexpr int T2_CYCLES = (T2 * F_CPU) / 1000000000; // T2 in ns
    ///    - constexpr int T3_CYCLES = (T3 * F_CPU) / 1000000000; // T3 in ns
    /// 3. Generate NOP sequences dynamically based on calculated cycles
    /// 4. Replace hardcoded send1()/send0() with parameterized versions
    /// This would enable support for SK6812 (T1=300ns, T2=300ns, T3=600ns),
    /// TM1809 (T1=350ns, T2=350ns, T3=450ns), and all other clockless chipsets.

private:
    u16 mNumLeds;

    /// @brief Send '1' bit - optimized for 39MHz
    /// 0.8µs high (~31 cycles), 0.45µs low (~17 cycles)
    FASTLED_FORCE_INLINE void send1() const {
        FastPin<DATA_PIN>::hi();
        asm volatile(
            "nop; nop; nop; nop; nop; nop; nop; nop;"  // 8
            "nop; nop; nop; nop; nop; nop; nop; nop;"  // 16
            "nop; nop; nop; nop; nop; nop; nop; nop;"  // 24
            "nop; nop; nop; nop; nop; nop; nop;"       // 31
        );
        FastPin<DATA_PIN>::lo();
        asm volatile(
            "nop; nop; nop; nop; nop; nop; nop; nop;"  // 8
            "nop; nop; nop; nop; nop; nop; nop; nop;"  // 16
            "nop;"                                      // 17
        );
    }

    /// @brief Send '0' bit - optimized for 39MHz
    /// 0.4µs high (~15 cycles), 0.85µs low (~33 cycles)
    FASTLED_FORCE_INLINE void send0() const {
        FastPin<DATA_PIN>::hi();
        asm volatile(
            "nop; nop; nop; nop; nop; nop; nop; nop;"  // 8
            "nop; nop; nop; nop; nop; nop; nop;"       // 15
        );
        FastPin<DATA_PIN>::lo();
        asm volatile(
            "nop; nop; nop; nop; nop; nop; nop; nop;"  // 8
            "nop; nop; nop; nop; nop; nop; nop; nop;"  // 16
            "nop; nop; nop; nop; nop; nop; nop; nop;"  // 24
            "nop; nop; nop; nop; nop; nop; nop; nop;"  // 32
            "nop;"                                      // 33
        );
    }

    /// @brief Send byte with MSB first - optimized tight loop
    /// @param byte_value 8-bit value to send
    FASTLED_FORCE_INLINE void sendByte(u8 byte_value) const {
        // Unrolled loop for maximum performance
        if (byte_value & 0x80) send1(); else send0(); // bit 7
        if (byte_value & 0x40) send1(); else send0(); // bit 6
        if (byte_value & 0x20) send1(); else send0(); // bit 5
        if (byte_value & 0x10) send1(); else send0(); // bit 4
        if (byte_value & 0x08) send1(); else send0(); // bit 3
        if (byte_value & 0x04) send1(); else send0(); // bit 2
        if (byte_value & 0x02) send1(); else send0(); // bit 1
        if (byte_value & 0x01) send1(); else send0(); // bit 0
    }

    /// @brief Send RGB pixel data in GRB order (WS2812 protocol)
    /// @param r Red channel value
    /// @param g Green channel value
    /// @param b Blue channel value
    FASTLED_FORCE_INLINE void sendPixel(u8 r, u8 g, u8 b) const {
        sendByte(g); // Green first
        sendByte(r); // Red second
        sendByte(b); // Blue third
    }

public:
    /// @brief Constructor
    ClocklessController_ezWS2812_GPIO_39MHz() : mNumLeds(0) {}

    /// @brief Initialize the controller
    virtual void init() override {
        FastPin<DATA_PIN>::setOutput();
        FastPin<DATA_PIN>::lo();
    }

    /// @brief Get maximum refresh rate
    virtual u16 getMaxRefreshRate() const override {
        return 400; // Conservative rate for GPIO timing
    }

protected:
    /// @brief Output pixels to LED strip - optimized for bulk processing
    /// @param pixels FastLED pixel controller with RGB data
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) override {
        mNumLeds = pixels.size();

        // Disable interrupts for precise timing - critical for WS2812
        cli();

        // Process all pixels in tight loop
        while (pixels.has(1)) {
            u8 r = pixels.loadAndScale0();
            u8 g = pixels.loadAndScale1();
            u8 b = pixels.loadAndScale2();

            sendPixel(r, g, b);

            pixels.advanceData();
            pixels.stepDithering();
        }

        // Re-enable interrupts
        sei();

        // WS2812 reset/latch time (>50µs low)
        delayMicroseconds(300);
    }
};

/// @brief Convenient typedef for 39MHz MGM240/MG24 GPIO controller
/// @tparam DATA_PIN GPIO pin number for LED data
/// @tparam RGB_ORDER Color channel ordering (typically GRB for WS2812)
template<u8 DATA_PIN, EOrder RGB_ORDER = GRB>
using EZWS2812_GPIO_39MHz = ClocklessController_ezWS2812_GPIO_39MHz<DATA_PIN, RGB_ORDER>;
}  // namespace fl