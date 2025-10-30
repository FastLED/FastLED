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
/// - **Color Modes**: RGB or RGBW (digitally-configurable via preamble)
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
/// The UCS7604 uses a universal carrier-based implementation that works on **ALL platforms**
/// with clockless controller support:
/// - Wraps any clockless controller (ClocklessBlocking, ClocklessRMT, ClocklessI2S, etc.)
/// - Same code works on AVR, ESP32, ARM (SAMD, STM32, Teensy), RP2040, and more
/// - No platform-specific assembly required
///
/// Implementation details can be found in `ucs7604_universal.h`.
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
///     // Standard 8-bit mode @ 800kHz (recommended for most use cases)
///     FastLED.addLeds<UCS7604, DATA_PIN, GRB>(leds, NUM_LEDS);
///
///     // Or 16-bit high-definition mode for maximum color depth:
///     // FastLED.addLeds<UCS7604HD, DATA_PIN, GRB>(leds, NUM_LEDS);
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
/// - **Platforms**: All platforms with clockless controller support
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

namespace fl {

/// @brief UCS7604 protocol configuration modes
/// Used by UCS7604Controller (see ucs7604_universal.h) to configure chipset operation
enum UCS7604Mode {
    UCS7604_MODE_8BIT_800KHZ = 0x03,   ///< 8-bit depth, 800 kHz, RGBW mode
    UCS7604_MODE_16BIT_800KHZ = 0x8B,  ///< 16-bit depth, 800 kHz, RGBW mode (default)
    UCS7604_MODE_16BIT_1600KHZ = 0x9B  ///< 16-bit depth, 1.6 MHz, RGBW mode
};

}  // namespace fl

// Include required headers for UCS7604Controller implementation
#include "cpixel_ledcontroller.h"
#include "fl/force_inline.h"

namespace fl {

/// @brief UCS7604 controller using carrier pattern
/// @tparam CARRIER_CONTROLLER Any clockless controller type (must use RGB ordering)
/// @tparam RGB_ORDER Color ordering for pixel data (default GRB)
/// @tparam MODE UCS7604 mode byte (8-bit/16-bit, 800kHz/1.6MHz)
///
/// This controller wraps any clockless controller and uses it to transmit:
/// 1. Chunk 1 (framing header) - 8 bytes
/// 2. 260µs delay
/// 3. Chunk 2 (configuration) - 7 bytes
/// 4. 260µs delay
/// 5. Pixel data - N bytes (actual LED data)
///
/// All data is transmitted using WS2812-compatible timing via the carrier.
///
/// # Architecture
///
/// The UCS7604Controller uses the wrapper/carrier pattern:
///
/// ```
/// UCS7604Controller<CARRIER, RGB_ORDER, MODE>
///   └─ wraps ANY ClocklessController as carrier
///   └─ carrier: ClocklessBlocking, ClocklessRMT, ClocklessI2S, etc.
///   └─ Uses carrier's optimized bit-banging for everything
/// ```
///
/// # Key Insight
///
/// All UCS7604 data uses WS2812-compatible timing:
/// - Preamble chunks (8 bytes + 7 bytes) use 800kHz timing
/// - Pixel data uses 800kHz or 1.6MHz timing
/// - Therefore: **one clockless controller can send all of it**
///
/// # Implementation Strategy
///
/// 1. Convert preamble bytes to "fake" CRGB pixels
/// 2. Send preambles via carrier.show() with raw data
/// 3. Send actual pixel data via carrier.show()
/// 4. All transmissions use carrier's optimized bit-banging
///
/// # Platform Support
///
/// Works on ALL FastLED platforms:
/// - AVR (Uno, Nano) - via ClocklessBlocking
/// - ESP32 - via ClocklessBlocking (or ClocklessRMT for performance)
/// - ARM (STM32, SAMD, Teensy) - via platform-specific drivers
/// - RP2040 - via platform-specific drivers
/// - Any platform with a clockless controller
template <
    typename CARRIER_CONTROLLER,
    EOrder RGB_ORDER = GRB,
    fl::UCS7604Mode MODE = fl::UCS7604_MODE_8BIT_800KHZ
>
class UCS7604Controller
    : public CPixelLEDController<RGB_ORDER,
                                 CARRIER_CONTROLLER::LANES_VALUE,
                                 CARRIER_CONTROLLER::MASK_VALUE>
{
private:
    // ControllerT is a wrapper that exposes protected methods of the carrier
    // Similar to RGBWEmulatedController pattern
    typedef CARRIER_CONTROLLER ControllerBaseT;

    class ControllerT : public CARRIER_CONTROLLER {
        friend class UCS7604Controller;

        /// Call carrier's beginShowLeds() (protected method)
        void* callBeginShowLeds(int size) {
            return ControllerBaseT::beginShowLeds(size);
        }

        /// Call carrier's show() (protected method)
        void callShow(const CRGB *data, int nLeds, fl::u8 brightness) {
            ControllerBaseT::show(data, nLeds, brightness);
        }

        /// Call carrier's showPixels() (protected method)
        template<EOrder RGB_ORDER_ARG, int LANES_ARG, fl::u32 MASK_ARG>
        void callShowPixels(PixelController<RGB_ORDER_ARG, LANES_ARG, MASK_ARG> &pixels) {
            ControllerBaseT::showPixels(pixels);
        }

        /// Call carrier's endShowLeds() (protected method)
        void callEndShowLeds(void* data) {
            ControllerBaseT::endShowLeds(data);
        }
    };

    // Static constants for UCS7604 protocol
    static constexpr uint8_t CHUNK1_LEN = 8;   ///< Framing header length
    static constexpr uint8_t CHUNK2_LEN = 7;   ///< Configuration length
    static constexpr uint16_t PREAMBLE_DELAY_US = 260;  ///< Delay between chunks

    // Chunk 1: Framing header (fixed pattern)
    const uint8_t mChunk1[CHUNK1_LEN] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x02
    };

    // Chunk 2: Configuration (mode-dependent)
    uint8_t mChunk2[CHUNK2_LEN];

    // The wrapped carrier controller
    ControllerT mCarrier;

public:
    static const int LANES = CARRIER_CONTROLLER::LANES_VALUE;
    static const fl::u32 MASK = CARRIER_CONTROLLER::MASK_VALUE;

    // Carrier must use RGB ordering (no reordering allowed for preambles)
    static_assert(RGB == CARRIER_CONTROLLER::RGB_ORDER_VALUE,
                  "Carrier controller MUST use RGB ordering (no reordering)");

    /// @brief Constructor - initializes UCS7604 configuration
    /// @param r_current Red channel current (0x00-0x0F, default 0x0F = max)
    /// @param g_current Green channel current (0x00-0x0F, default 0x0F = max)
    /// @param b_current Blue channel current (0x00-0x0F, default 0x0F = max)
    /// @param w_current White channel current (0x00-0x0F, default 0x0F = max)
    UCS7604Controller(uint8_t r_current = 0x0F, uint8_t g_current = 0x0F,
                              uint8_t b_current = 0x0F, uint8_t w_current = 0x0F) {
        // Initialize Chunk 2 based on MODE
        mChunk2[0] = static_cast<uint8_t>(MODE);  // Configuration byte
        mChunk2[1] = r_current & 0x0F;  // R current (max)
        mChunk2[2] = g_current & 0x0F;  // G current (max)
        mChunk2[3] = b_current & 0x0F;  // B current (max)
        mChunk2[4] = w_current & 0x0F;  // W current (max)
        mChunk2[5] = 0x00;  // Reserved
        mChunk2[6] = 0x00;  // Reserved
    }

    /// @brief Initialize the carrier controller
    virtual void init() override {
        mCarrier.init();
        // Keep carrier disabled - we control when it shows
        mCarrier.setEnabled(false);
    }

    /// @brief Get maximum refresh rate
    virtual fl::u16 getMaxRefreshRate() const override {
        return 400;  // Conservative estimate for UCS7604
    }

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
    /// Transmission sequence:
    /// 1. Prepare carrier (disable color correction/dithering)
    /// 2. Begin show cycle (carrier sync point)
    /// 3. Send Chunk 1 (framing header) as fake CRGB pixels
    /// 4. Delay 260µs
    /// 5. Send Chunk 2 (configuration) as fake CRGB pixels
    /// 6. Delay 260µs
    /// 7. Send actual pixel data
    /// 8. End show cycle (carrier signal transmission)
    virtual void showPixels(PixelController<RGB_ORDER, LANES, MASK> &pixels) override {
        if (pixels.size() == 0) {
            return;
        }

        // Prepare carrier for raw transmission
        // Force pass-through mode: no color correction, no dithering
        mCarrier.setCorrection(CRGB(255, 255, 255));
        mCarrier.setTemperature(CRGB(255, 255, 255));
        mCarrier.setDither(DISABLE_DITHER);
        mCarrier.setEnabled(true);

        // Begin show cycle (carrier sync point)
        void* sync_data = mCarrier.callBeginShowLeds(pixels.size());

        // Send Chunk 1 (framing header) as fake CRGB pixels
        sendPreambleChunk(mChunk1, CHUNK1_LEN);
        delayMicroseconds(PREAMBLE_DELAY_US);

        // Send Chunk 2 (configuration) as fake CRGB pixels
        sendPreambleChunk(mChunk2, CHUNK2_LEN);
        delayMicroseconds(PREAMBLE_DELAY_US);

        // Send actual pixel data
        sendPixelData(pixels);

        // End show cycle (carrier signal transmission)
        mCarrier.callEndShowLeds(sync_data);
        mCarrier.setEnabled(false);
    }

protected:
    /// @brief Send a preamble chunk as fake CRGB pixels
    /// @param bytes Pointer to preamble bytes
    /// @param len Number of bytes in preamble
    ///
    /// Converts raw bytes to CRGB array (3 bytes per CRGB) and sends via carrier.
    /// The carrier transmits these bytes with WS2812 timing, which UCS7604 accepts.
    FASTLED_FORCE_INLINE void sendPreambleChunk(const uint8_t* bytes, uint8_t len) {
        // Convert byte array to CRGB array (3 bytes per CRGB)
        // Round up: (len + 2) / 3
        int num_fake_pixels = (len + 2) / 3;
        CRGB fake_pixels[3];  // Max 3 CRGB for 8 bytes

        // Zero out and copy bytes
        memset(fake_pixels, 0, sizeof(fake_pixels));
        memcpy(fake_pixels, bytes, len);

        // Send via carrier with full brightness (raw data)
        mCarrier.callShow(fake_pixels, num_fake_pixels, 255);
    }

    /// @brief Send actual pixel data
    /// @param pixels The pixel controller
    ///
    /// Calls the carrier's showPixels() method directly.
    /// This preserves all color correction, dithering, and scaling
    /// that the PixelController has already applied.
    void sendPixelData(PixelController<RGB_ORDER, LANES, MASK> &pixels) {
        mCarrier.callShowPixels(pixels);
    }
};

}  // namespace fl

#endif // __INC_UCS7604_H
