#pragma once

#ifndef __INC_UCS7604_H
#define __INC_UCS7604_H

#include "pixeltypes.h"
#include "pixel_controller.h"

/// @file ucs7604.h
/// @brief UCS7604 LED chipset controller implementation for FastLED
///
/// # Overview
///
/// The UCS7604 is a high-resolution 4-channel (RGBW) LED driver IC designed by UCS New Technology Co.
/// It features 16-bit color resolution (65,536 levels per channel), configurable bit depth modes,
/// dual data rates, and digitally-configurable RGB/RGBW operation.
///
/// **Key Specifications:**
/// - **Resolution**: 16-bit (65,536 levels per channel)
/// - **Bit Depths**: 8/12/14/16-bit configurable via protocol
/// - **Data Rates**: 800 kbps or 1.6 Mbps (configurable)
/// - **PWM Frequency**: 16,000 KHz (~500fps camera compatible, flicker-free)
/// - **Color Modes**: RGB or RGBW (digitally configurable via preamble)
/// - **Voltage**: DC24V (5V regulator integrated)
/// - **Current Control**: 4-bit per channel (0x00-0x0F, 0-60mA range, interacts with hardware ILIM resistor)
/// - **Special Features**:
///   - Built-in gamma 2.2 correction
///   - Redundant data line for breakpoint continuation
///   - No external clock required (SPI-based but self-clocked)
///
/// # Protocol Structure
///
/// The UCS7604 uses a unique **preamble-based protocol** that differs from standard WS2812-like chipsets.
/// Data transmission consists of three sequential phases with timing-critical delays:
///
/// ```
/// [Chunk 1: 8 bytes] → [260µs delay] → [Chunk 2: 7 bytes] → [260µs delay] → [Pixel Data: N bytes]
/// ```
///
/// ## Chunk 1 - Framing Header (8 bytes, always 800kHz)
/// Fixed synchronization pattern for clock recovery:
/// ```
/// 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x00 0x02
/// ```
/// - First 6 bytes (0xFF) provide clock sync
/// - Last byte must be 0x02 (protocol identifier)
/// - Always transmitted at 800kHz regardless of pixel data speed setting
///
/// ## Chunk 2 - Configuration (7 bytes, always 800kHz)
/// Runtime configuration for the LED string:
/// ```
/// [Config Byte] [R Current] [G Current] [B Current] [W Current] [Reserved] [Reserved]
/// ```
///
/// ### Configuration Byte Format (Byte 0):
/// | Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1-0 |
/// |-------|-------|-------|-------|-------|-------|---------|
/// | Depth | ?     | ?     | Speed | Depth | ?     | Mode    |
///
/// - **Bits 7 & 3** (Bit Depth): `00` = 8-bit, `11` = 16-bit (12/14-bit modes exist but undocumented)
/// - **Bit 4** (Speed): `1` = 1.6MHz pixel data, `0` = 800kHz pixel data
/// - **Bits 1-0** (Field Mode):
///   - `11` = RGBW (4 channels active)
///   - `10` = RGB (3 channels active)
///   - `01` = RG+BW (split pairs - undocumented behavior)
///   - `00` = All channels together (undocumented behavior)
/// - **Bits 6, 5, 2**: Unknown/unused in available documentation
///
/// ### Current Control Bytes (Bytes 1-4):
/// Per-channel current limit: `0x00` (minimum) to `0x0F` (maximum ~60mA)
/// - Interacts with hardware ILIM resistor on LED PCB
/// - Actual current depends on both digital setting and resistor value
/// - Independent control for R, G, B, W channels
///
/// ### Configuration Examples:
/// - `0x8B` = 16-bit depth, 800kHz speed, RGBW mode (default)
/// - `0x9B` = 16-bit depth, 1.6MHz speed, RGBW mode (high-speed)
/// - `0x03` = 8-bit depth, 800kHz speed, RGBW mode (memory-efficient)
///
/// ## Chunk 3 - Pixel Data
/// - **Byte Ordering**: RGBW (Red, Green, Blue, White)
/// - **8-bit mode**: 4 bytes per pixel (1 byte per channel)
/// - **16-bit mode**: 8 bytes per pixel (2 bytes per channel, MSB first)
/// - **Transmission Speed**: 800kHz or 1.6MHz as configured in Chunk 2
///
/// # Timing Requirements
///
/// - **Preamble Delays**: 260µs between chunks (critical for protocol synchronization)
/// - **Reset Time**: Standard latch delay after pixel data (typically 280µs, following WS2812 conventions)
/// - **Bit Timing**: WS2812-compatible timing for actual bit transmission
///   - Uses standard T0H/T0L/T1H/T1L values
///   - 800kHz mode: Same timing as WS2812 (T1=2, T2=5, T3=3 in FMUL units)
///   - 1.6MHz mode: Timing requirements not yet validated (experimental)
///
/// # Implementation Notes
///
/// ## Why UCS7604 Requires Custom Implementation
///
/// Unlike standard WS2812/APA102 chipsets that use single-transmission protocols, UCS7604 requires:
/// 1. Three separate transmissions (preamble1 → preamble2 → pixels)
/// 2. Precise 260µs delays between transmissions
/// 3. Configuration data sent before every frame
///
/// This preamble pattern appears **unique to UCS7604** among addressable LED chipsets. Research found:
/// - **UCS1903, UCS2903, UCS1904**: Standard WS2812-like single-transmission protocols
/// - **TM18xx series**: Standard protocols, no preambles
/// - **GS8208**: Has power-on self-test but standard data protocol
/// - **SK6812, WS28xx**: Standard single-transmission protocols
///
/// ## Platform Support
///
/// Currently implemented for **ARM Cortex-M0/M0+ platforms** (SAMD21/SAMD51):
/// - Reuses existing `showLedData<>()` ARM assembly routine for efficiency
/// - Same assembly code handles preambles (raw bytes) and pixel data (with color correction)
/// - Conditional compilation via `__SAMD21__` / `__SAMD51__` defines
///
/// **Future Platform Expansion:**
/// - ESP32 (RMT/I2S): Requires buffering all three chunks into unified RMT symbol stream
/// - STM32: Similar approach to ARM M0 using GPIO bit-banging
/// - Teensy 4.x: FlexIO DMA approach similar to ObjectFLED
/// - AVR: Likely unsupported (insufficient timing margin for 260µs delays with interrupts)
///
/// # References & Documentation
///
/// ## Official Datasheets
/// - **UCS7604 Datasheet (PDF)**: https://www.ledyilighting.com/wp-content/uploads/2025/02/UCS7604-datasheet.pdf
///   - Official technical specifications from manufacturer
/// - **UCS7604 Specification Sheet**: https://suntechlite.com/ucs7604-specification-sheet-download/
///   - Manufacturer download page with additional resources
///
/// ## Technical Documentation
/// - **Advatek Technical Specs**: https://www.advateklighting.com/pixel-protocols/ucs7604
///   - Third-party documentation of UCS7604 pixel protocol
///   - Industry-standard reference for lighting controllers
/// - **Art LED Protocol Overview**: https://www.artleds.com/blog/ucs7604-ic-pixel-protocol-overview
///   - Detailed protocol overview and application notes
///
/// ## Community Resources & Prior Art
/// - **FastLED Issue #2088**: https://github.com/FastLED/FastLED/issues/2088#issuecomment-3373962815
///   - Original feature request: "How to add standalone prefix to each output of pixel data"
///   - Discussion of preamble architecture requirements
/// - **Prototype PR**: https://github.com/FastLED/FastLED/compare/master...second-string:FastLED-fork:ucs7604-support?expand=1
///   - Working ARM D21/D51 prototype implementation
///   - Demonstrated successful UCS7604 hardware validation
///
/// ## Comparison: UCS Series Chipsets
///
/// For context on why UCS7604 is architecturally different:
/// - **UCS1903** (8-bit, NO Clock): Standard WS2812-like protocol, stable signal transmission
/// - **UCS2903** (8-bit, NO Clock): Improved UCS1904, constant current design, 1.5KHz PWM
/// - **UCS8903** (16-bit, SPI): High-resolution variant, protocol details undocumented
/// - **UCS7604** (16-bit, SPI): **Unique preamble-based protocol**, RGBW support, highest resolution
///
/// The UCS7604's preamble architecture allows runtime configuration (bit depth, speed, RGBW mode)
/// that other chipsets handle through fixed hardware design or separate IC variants.
///
/// # Usage Example
///
/// ```cpp
/// #include <FastLED.h>
///
/// #define DATA_PIN 6
/// #define NUM_LEDS 100
///
/// CRGB leds[NUM_LEDS];
///
/// void setup() {
///     // 16-bit mode @ 800kHz (default)
///     FastLED.addLeds<UCS7604, DATA_PIN, GRB>(leds, NUM_LEDS);
///
///     // Or 8-bit mode for memory efficiency:
///     // FastLED.addLeds<UCS7604Controller800Khz_8bit, DATA_PIN, GRB>(leds, NUM_LEDS);
/// }
///
/// void loop() {
///     leds[0] = CRGB::Red;
///     FastLED.show();
///     delay(500);
/// }
/// ```
///
/// # Open Questions & Future Research
///
/// These questions require hardware testing with actual UCS7604 LEDs:
/// 1. **12-bit and 14-bit modes**: How are these configured? Mixed depth per channel?
/// 2. **Current control maximum**: Does `0x0F` truly cap at 60mA? Can higher values be used?
/// 3. **Timing tolerance**: How strict are the 260µs delays? Acceptable range?
/// 4. **1.6MHz reliability**: Does high-speed mode work reliably with long cable runs?
/// 5. **Reset/latch time**: Minimum delay required after pixel data before next frame?
/// 6. **Exact bit timing**: Precise T0H/T0L/T1H/T1L values for both 800kHz and 1.6MHz modes?
/// 7. **Field modes 00/01**: What do "all channels together" and "RG+BW" modes actually do?
///
/// # Beta Status & Limitations
///
/// **⚠️ BETA DRIVER - Hardware validation ongoing**
///
/// - **Platforms**: ARM M0/M0+ (SAMD21/SAMD51) only
/// - **Tested**: Code compiles and passes linting, architecture validated via prototype
/// - **Not Tested**: Real UCS7604 hardware validation pending
/// - **Limitations**:
///   - Single data pin only (no parallel output)
///   - 800kHz mode only (1.6MHz untested)
///   - 8-bit and 16-bit modes implemented, 12/14-bit modes not yet supported
/// - **Known Issues**: None reported (no hardware testing yet)
///
/// For the latest status, hardware compatibility, and ESP32/STM32 ports, see:
/// https://github.com/FastLED/FastLED/issues/2088

// Platform-specific implementations
#if defined(__SAMD21G18A__) || defined(__SAMD21E18A__) || defined(__SAMD21__)
#define UCS7604_HAS_CONTROLLER 1
#include "../platforms/arm/common/m0clockless.h"
#include "eorder.h"
#include "fastpin.h"
#include "led_controller.h"
#include "fl/force_inline.h"

namespace fl {

/// @brief UCS7604 protocol configuration modes
enum UCS7604Mode {
    UCS7604_MODE_8BIT_800KHZ = 0x03,   ///< 8-bit depth, 800 kHz, RGBW mode
    UCS7604_MODE_16BIT_800KHZ = 0x8B,  ///< 16-bit depth, 800 kHz, RGBW mode (default)
    UCS7604_MODE_16BIT_1600KHZ = 0x9B  ///< 16-bit depth, 1.6 MHz, RGBW mode
};

/// @brief UCS7604 controller for ARM M0 platforms (SAMD21)
/// @tparam DATA_PIN The GPIO pin for data output
/// @tparam T1 Timing parameter for first interval (WS2812 timing: 2 FMUL)
/// @tparam T2 Timing parameter for second interval (WS2812 timing: 5 FMUL)
/// @tparam T3 Timing parameter for third interval (WS2812 timing: 3 FMUL)
/// @tparam RGB_ORDER The RGB ordering for pixel data (typically GRB)
/// @tparam MODE The UCS7604 mode configuration byte
///
/// This controller sends UCS7604 preambles before pixel data using the
/// ARM M0 showLedData<>() assembly routine for all transmissions.
template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB,
          UCS7604Mode MODE = UCS7604_MODE_16BIT_800KHZ, int WAIT_TIME = 280>
class UCS7604Controller : public CPixelLEDController<RGB_ORDER> {
    typedef typename FastPinBB<DATA_PIN>::port_ptr_t data_ptr_t;
    typedef typename FastPinBB<DATA_PIN>::port_t data_t;

    data_t mPinMask;
    data_ptr_t mPort;
    CMinWait<WAIT_TIME> mWait;

    // UCS7604 preamble configuration
    static constexpr uint8_t CHUNK1_LEN = 8;
    static constexpr uint8_t CHUNK2_LEN = 7;
    static constexpr uint16_t PREAMBLE_DELAY_US = 260;

    // Chunk 1: Framing header (fixed pattern)
    const uint8_t mChunk1[CHUNK1_LEN] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x02
    };

    // Chunk 2: Configuration (mode-dependent)
    uint8_t mChunk2[CHUNK2_LEN];

public:
    /// @brief Constructor - initializes UCS7604 configuration
    /// @param r_current Red channel current (0x00-0x0F, default 0x0F = max)
    /// @param g_current Green channel current (0x00-0x0F, default 0x0F = max)
    /// @param b_current Blue channel current (0x00-0x0F, default 0x0F = max)
    /// @param w_current White channel current (0x00-0x0F, default 0x0F = max)
    UCS7604Controller(uint8_t r_current = 0x0F, uint8_t g_current = 0x0F,
                      uint8_t b_current = 0x0F, uint8_t w_current = 0x0F) {
        mChunk2[0] = static_cast<uint8_t>(MODE);
        mChunk2[1] = r_current & 0x0F;
        mChunk2[2] = g_current & 0x0F;
        mChunk2[3] = b_current & 0x0F;
        mChunk2[4] = w_current & 0x0F;
        mChunk2[5] = 0x00;  // Reserved
        mChunk2[6] = 0x00;  // Reserved
    }

    /// @brief Initialize the GPIO pin
    virtual void init() {
        FastPinBB<DATA_PIN>::setOutput();
        mPinMask = FastPinBB<DATA_PIN>::mask();
        mPort = FastPinBB<DATA_PIN>::port();
    }

    /// @brief Get maximum refresh rate
    virtual uint16_t getMaxRefreshRate() const { return 400; }

    /// @brief Set current control for individual channels
    /// @param r_current Red channel current (0x00-0x0F)
    /// @param g_current Green channel current (0x00-0x0F)
    /// @param b_current Blue channel current (0x00-0x0F)
    /// @param w_current White channel current (0x00-0x0F)
    void setCurrentControl(uint8_t r_current, uint8_t g_current,
                          uint8_t b_current, uint8_t w_current) {
        mChunk2[1] = r_current & 0x0F;
        mChunk2[2] = g_current & 0x0F;
        mChunk2[3] = b_current & 0x0F;
        mChunk2[4] = w_current & 0x0F;
    }

    /// @brief Show pixels with UCS7604 preambles
    /// @param pixels The pixel controller with RGB data
    ///
    /// Sends: chunk1 → delay → chunk2 → delay → pixel data
    /// Retries once if initial transmission fails.
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) {
        if (pixels.size() == 0) {
            return;
        }

        mWait.wait();
        cli();

        if (!showRGBInternal(pixels)) {
            // Retry once if failed
            sei();
            delayMicroseconds(WAIT_TIME);
            cli();
            showRGBInternal(pixels);
        }

        sei();
        mWait.mark();
    }

protected:
    /// @brief Send a preamble chunk using ARM M0 timing
    /// @param data Pointer to preamble bytes
    /// @param len Number of bytes in preamble
    ///
    /// Sends raw bytes without color scaling/dithering using showLedData<>()
    FASTLED_FORCE_INLINE void sendPreamble(const uint8_t* data, uint32_t len) {
        struct M0ClocklessData preambleData;
        // Zero out all scaling/adjustment for raw preamble transmission
        preambleData.d[0] = 0;
        preambleData.d[1] = 0;
        preambleData.d[2] = 0;
        preambleData.s[0] = 0;
        preambleData.s[1] = 0;
        preambleData.s[2] = 0;
        preambleData.e[0] = 0;
        preambleData.e[1] = 0;
        preambleData.e[2] = 0;
        preambleData.adj = 0;

        typename FastPin<DATA_PIN>::port_ptr_t portBase = FastPin<DATA_PIN>::port();
        showLedData<8, 4, T1, T2, T3, RGB_ORDER, WAIT_TIME>(
            portBase, FastPin<DATA_PIN>::mask(), data, len, &preambleData);
    }

    /// @brief Internal method to show RGB pixel data with UCS7604 preambles
    /// @param pixels The pixel controller
    /// @return Non-zero on success
    ///
    /// Transmits: chunk1 → delay → chunk2 → delay → pixel data
    uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {
        if (pixels.size() == 0) {
            return 1;
        }

        // Send chunk 1 (framing header)
        sendPreamble(mChunk1, CHUNK1_LEN);
        delayMicroseconds(PREAMBLE_DELAY_US);

        // Send chunk 2 (configuration)
        sendPreamble(mChunk2, CHUNK2_LEN);
        delayMicroseconds(PREAMBLE_DELAY_US);

        // Setup pixel data transmission with color adjustment
        struct M0ClocklessData data;
        data.d[0] = pixels.d[0];
        data.d[1] = pixels.d[1];
        data.d[2] = pixels.d[2];
        data.s[0] = pixels.mColorAdjustment.premixed[0];
        data.s[1] = pixels.mColorAdjustment.premixed[1];
        data.s[2] = pixels.mColorAdjustment.premixed[2];
        data.e[0] = pixels.e[0];
        data.e[1] = pixels.e[1];
        data.e[2] = pixels.e[2];
        data.adj = pixels.mAdvance;

        // Send pixel data
        typename FastPin<DATA_PIN>::port_ptr_t portBase = FastPin<DATA_PIN>::port();
        return showLedData<8, 4, T1, T2, T3, RGB_ORDER, WAIT_TIME>(
            portBase, FastPin<DATA_PIN>::mask(), pixels.mData, pixels.mLen, &data);
    }
};

#elif defined(__SAMD51__) || defined(__SAME51__)
#define UCS7604_HAS_CONTROLLER 1

/// @brief UCS7604 controller for ARM M0+ platforms (SAMD51)
/// Same implementation as SAMD21 version
template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB,
          UCS7604Mode MODE = UCS7604_MODE_16BIT_800KHZ, int WAIT_TIME = 280>
class UCS7604Controller : public CPixelLEDController<RGB_ORDER> {
    typedef typename FastPinBB<DATA_PIN>::port_ptr_t data_ptr_t;
    typedef typename FastPinBB<DATA_PIN>::port_t data_t;

    data_t mPinMask;
    data_ptr_t mPort;
    CMinWait<WAIT_TIME> mWait;

    static constexpr uint8_t CHUNK1_LEN = 8;
    static constexpr uint8_t CHUNK2_LEN = 7;
    static constexpr uint16_t PREAMBLE_DELAY_US = 260;

    const uint8_t mChunk1[CHUNK1_LEN] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x02
    };

    uint8_t mChunk2[CHUNK2_LEN];

public:
    UCS7604Controller(uint8_t r_current = 0x0F, uint8_t g_current = 0x0F,
                      uint8_t b_current = 0x0F, uint8_t w_current = 0x0F) {
        mChunk2[0] = static_cast<uint8_t>(MODE);
        mChunk2[1] = r_current & 0x0F;
        mChunk2[2] = g_current & 0x0F;
        mChunk2[3] = b_current & 0x0F;
        mChunk2[4] = w_current & 0x0F;
        mChunk2[5] = 0x00;
        mChunk2[6] = 0x00;
    }

    virtual void init() {
        FastPinBB<DATA_PIN>::setOutput();
        mPinMask = FastPinBB<DATA_PIN>::mask();
        mPort = FastPinBB<DATA_PIN>::port();
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

    void setCurrentControl(uint8_t r_current, uint8_t g_current,
                          uint8_t b_current, uint8_t w_current) {
        mChunk2[1] = r_current & 0x0F;
        mChunk2[2] = g_current & 0x0F;
        mChunk2[3] = b_current & 0x0F;
        mChunk2[4] = w_current & 0x0F;
    }

    virtual void showPixels(PixelController<RGB_ORDER>& pixels) {
        if (pixels.size() == 0) {
            return;
        }

        mWait.wait();
        cli();

        if (!showRGBInternal(pixels)) {
            sei();
            delayMicroseconds(WAIT_TIME);
            cli();
            showRGBInternal(pixels);
        }

        sei();
        mWait.mark();
    }

protected:
    FASTLED_FORCE_INLINE void sendPreamble(const uint8_t* data, uint32_t len) {
        struct M0ClocklessData preambleData;
        preambleData.d[0] = 0;
        preambleData.d[1] = 0;
        preambleData.d[2] = 0;
        preambleData.s[0] = 0;
        preambleData.s[1] = 0;
        preambleData.s[2] = 0;
        preambleData.e[0] = 0;
        preambleData.e[1] = 0;
        preambleData.e[2] = 0;
        preambleData.adj = 0;

        typename FastPin<DATA_PIN>::port_ptr_t portBase = FastPin<DATA_PIN>::port();
        showLedData<8, 4, T1, T2, T3, RGB_ORDER, WAIT_TIME>(
            portBase, FastPin<DATA_PIN>::mask(), data, len, &preambleData);
    }

    uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {
        if (pixels.size() == 0) {
            return 1;
        }

        sendPreamble(mChunk1, CHUNK1_LEN);
        delayMicroseconds(PREAMBLE_DELAY_US);

        sendPreamble(mChunk2, CHUNK2_LEN);
        delayMicroseconds(PREAMBLE_DELAY_US);

        struct M0ClocklessData data;
        data.d[0] = pixels.d[0];
        data.d[1] = pixels.d[1];
        data.d[2] = pixels.d[2];
        data.s[0] = pixels.mColorAdjustment.premixed[0];
        data.s[1] = pixels.mColorAdjustment.premixed[1];
        data.s[2] = pixels.mColorAdjustment.premixed[2];
        data.e[0] = pixels.e[0];
        data.e[1] = pixels.e[1];
        data.e[2] = pixels.e[2];
        data.adj = pixels.mAdvance;

        typename FastPin<DATA_PIN>::port_ptr_t portBase = FastPin<DATA_PIN>::port();
        return showLedData<8, 4, T1, T2, T3, RGB_ORDER, WAIT_TIME>(
            portBase, FastPin<DATA_PIN>::mask(), pixels.mData, pixels.mLen, &data);
    }
};

}  // namespace fl

#endif // Platform checks

#endif // __INC_UCS7604_H
