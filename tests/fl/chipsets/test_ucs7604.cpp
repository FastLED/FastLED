/// @file test_ucs7604.cpp
/// @brief Unit test for UCS7604 LED chipset protocol
///
/// UCS7604 Protocol Format:
/// - Preamble: 15 bytes (device config + current control)
/// - LED data: 3 bytes (RGB 8-bit) per LED
/// - Padding: 0-2 zero bytes to ensure total size divisible by 3

#include "fl/chipsets/ucs7604.h"
#include "fl/ease.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "cled_controller.h"
#include "cpixel_ledcontroller.h"
#include "dither_mode.h"
#include "doctest.h"
#include "eorder.h"
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/chipsets/encoders/ucs7604.h"
#include "fl/chipsets/led_timing.h"
#include "fl/eorder.h"
#include "fl/rgb8.h"
#include "fl/rgbw.h"
#include "fl/slice.h"
#include "pixel_controller.h"
#include "rgbw.h"
#include "fl/stl/vector.h"
#include "hsv2rgb.h"

using namespace fl;

namespace {

/// RGB16 color structure for 16-bit color values (test-only)
/// Similar to CRGB but uses uint16_t for each channel
struct RGB16 {
    union {
        struct {
            uint16_t r;  ///< Red channel (16-bit)
            uint16_t g;  ///< Green channel (16-bit)
            uint16_t b;  ///< Blue channel (16-bit)
        };
        uint16_t raw[3];  ///< Access as array
    };

    /// Default constructor
    RGB16() : r(0), g(0), b(0) {}

    /// Construct from individual 16-bit values
    RGB16(uint16_t ir, uint16_t ig, uint16_t ib) : r(ir), g(ig), b(ib) {}

    /// Array access operator
    uint16_t& operator[](size_t x) { return raw[x]; }
    const uint16_t& operator[](size_t x) const { return raw[x]; }
};

/// RGBW8 color structure for 8-bit RGBW color values (test-only)
struct RGBW8 {
    union {
        struct {
            uint8_t r;  ///< Red channel (8-bit)
            uint8_t g;  ///< Green channel (8-bit)
            uint8_t b;  ///< Blue channel (8-bit)
            uint8_t w;  ///< White channel (8-bit)
        };
        uint8_t raw[4];  ///< Access as array
    };

    /// Default constructor
    RGBW8() : r(0), g(0), b(0), w(0) {}

    /// Construct from individual 8-bit values
    RGBW8(uint8_t ir, uint8_t ig, uint8_t ib, uint8_t iw) : r(ir), g(ig), b(ib), w(iw) {}

    /// Array access operator
    uint8_t& operator[](size_t x) { return raw[x]; }
    const uint8_t& operator[](size_t x) const { return raw[x]; }
};

/// RGBW16 color structure for 16-bit RGBW color values (test-only)
/// Similar to RGB16 but with white channel
struct RGBW16 {
    union {
        struct {
            uint16_t r;  ///< Red channel (16-bit)
            uint16_t g;  ///< Green channel (16-bit)
            uint16_t b;  ///< Blue channel (16-bit)
            uint16_t w;  ///< White channel (16-bit)
        };
        uint16_t raw[4];  ///< Access as array
    };

    /// Default constructor
    RGBW16() : r(0), g(0), b(0), w(0) {}

    /// Construct from individual 16-bit values
    RGBW16(uint16_t ir, uint16_t ig, uint16_t ib, uint16_t iw) : r(ir), g(ig), b(ib), w(iw) {}

    /// Array access operator
    uint16_t& operator[](size_t x) { return raw[x]; }
    const uint16_t& operator[](size_t x) const { return raw[x]; }
};

// Preamble constants for different modes
constexpr uint8_t PREAMBLE_8BIT_800KHZ[15] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Sync pattern (6 bytes)
    0x00, 0x02,                          // Header (2 bytes)
    0x03,                                // MODE: 8-bit @ 800kHz
    0x0F, 0x0F, 0x0F, 0x0F,             // RGBW current control (4 bytes)
    0x00, 0x00                           // Reserved (2 bytes)
};

constexpr uint8_t PREAMBLE_16BIT_800KHZ[15] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Sync pattern (6 bytes)
    0x00, 0x02,                          // Header (2 bytes)
    0x8B,                                // MODE: 16-bit @ 800kHz
    0x0F, 0x0F, 0x0F, 0x0F,             // RGBW current control (4 bytes)
    0x00, 0x00                           // Reserved (2 bytes)
};

/// Interface for accessing captured byte data
class IData {
public:
    virtual ~IData() = default;
    virtual fl::span<const uint8_t> data() const = 0;
};

/// Mock clockless controller that captures byte output
template<int DATA_PIN, typename TIMING, EOrder RGB_ORDER>
class MockClocklessController : public CPixelLEDController<RGB_ORDER>, public IData {
public:
    fl::vector<uint8_t> capturedBytes;

    virtual void init() override {}

    fl::span<const uint8_t> data() const override {
        return fl::span<const uint8_t>(capturedBytes.data(), capturedBytes.size());
    }

    // Made public for testing with UCS7604Controller which uses composition
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) override {
        // Capture raw RGB bytes without any RGBW processing
        // UCS7604Controller already handles RGBW conversion internally
        capturedBytes.clear();
        PixelController<RGB> pixels_rgb = pixels;
        pixels_rgb.disableColorAdjustment();
        auto iterator = pixels_rgb.as_iterator(RgbwInvalid());
        iterator.writeWS2812(&capturedBytes);
    }
};

/// Test wrapper that exposes protected showPixels method and provides access to captured bytes
template<int DATA_PIN, EOrder RGB_ORDER>
class UCS7604TestController8bit : public UCS7604Controller8bitT<DATA_PIN, RGB_ORDER, MockClocklessController> {
private:
    using BaseType = UCS7604Controller8bitT<DATA_PIN, RGB_ORDER, MockClocklessController>;
    using DelegateType = MockClocklessController<DATA_PIN, TIMING_UCS7604_800KHZ, RGB>;

public:
    using BaseType::showPixels;

    // Propagate RGBW setting to delegate so UCS7604 can query it
    CLEDController& setRgbw(const fl::Rgbw& arg = RgbwDefault::value()) {
        BaseType::setRgbw(arg);
        this->getDelegate().setRgbw(arg);
        return *this;
    }

    // Access captured bytes from the delegate controller
    fl::span<const uint8_t> getCapturedBytes() const {
        const DelegateType& delegate = this->getDelegate();
        const IData* idata = static_cast<const IData*>(&delegate);
        return idata->data();
    }
};

/// Test wrapper that exposes protected showPixels method and provides access to captured bytes
template<int DATA_PIN, EOrder RGB_ORDER>
class UCS7604TestController16bit : public UCS7604Controller16bitT<DATA_PIN, RGB_ORDER, MockClocklessController> {
private:
    using BaseType = UCS7604Controller16bitT<DATA_PIN, RGB_ORDER, MockClocklessController>;
    using DelegateType = MockClocklessController<DATA_PIN, TIMING_UCS7604_800KHZ, RGB>;

public:
    using BaseType::showPixels;

    // Propagate RGBW setting to delegate so UCS7604 can query it
    CLEDController& setRgbw(const fl::Rgbw& arg = RgbwDefault::value()) {
        BaseType::setRgbw(arg);
        this->getDelegate().setRgbw(arg);
        return *this;
    }

    // Access captured bytes from the delegate controller
    fl::span<const uint8_t> getCapturedBytes() const {
        const DelegateType& delegate = this->getDelegate();
        const IData* idata = static_cast<const IData*>(&delegate);
        return idata->data();
    }
};


/// Helper to verify preamble structure
/// @param bytes Captured byte stream
/// @param expected_preamble Expected preamble bytes
void verifyPreamble(fl::span<const uint8_t> bytes, fl::span<const uint8_t> expected_preamble) {
    REQUIRE_EQ(expected_preamble.size(), 15);
    for (size_t i = 0; i < expected_preamble.size(); i++) {
        CHECK_EQ(bytes[i], expected_preamble[i]);
    }
}

/// Helper to verify pixel data (RGB 8-bit mode)
/// Verifies that the byte stream contains the expected RGB pixel data
/// starting at offset 15 (after the preamble)
void verifyPixels8bit(fl::span<const uint8_t> bytes, fl::span<const CRGB> pixels) {
    const size_t PREAMBLE_SIZE = 15;
    const size_t BYTES_PER_PIXEL = 3;  // RGB 8-bit

    for (size_t i = 0; i < pixels.size(); i++) {
        size_t byte_offset = PREAMBLE_SIZE + (i * BYTES_PER_PIXEL);
        CHECK_EQ(bytes[byte_offset + 0], pixels[i].r);  // R
        CHECK_EQ(bytes[byte_offset + 1], pixels[i].g);  // G
        CHECK_EQ(bytes[byte_offset + 2], pixels[i].b);  // B
    }
}

/// Helper to verify pixel data (RGB 16-bit mode)
/// Verifies that the byte stream contains the expected RGB pixel data
/// starting at offset 15 (after the preamble), scaled to 16-bit
void verifyPixels16bit(fl::span<const uint8_t> bytes, fl::span<const RGB16> pixels) {
    const size_t PREAMBLE_SIZE = 15;
    const size_t BYTES_PER_PIXEL = 6;  // RGB 16-bit

    for (size_t i = 0; i < pixels.size(); i++) {
        size_t byte_offset = PREAMBLE_SIZE + (i * BYTES_PER_PIXEL);

        uint16_t r16 = pixels[i].r;
        uint16_t g16 = pixels[i].g;
        uint16_t b16 = pixels[i].b;

        // Verify big-endian 16-bit values
        CHECK_EQ(bytes[byte_offset + 0], r16 >> 8);    // R high
        CHECK_EQ(bytes[byte_offset + 1], r16 & 0xFF);  // R low
        CHECK_EQ(bytes[byte_offset + 2], g16 >> 8);    // G high
        CHECK_EQ(bytes[byte_offset + 3], g16 & 0xFF);  // G low
        CHECK_EQ(bytes[byte_offset + 4], b16 >> 8);    // B high
        CHECK_EQ(bytes[byte_offset + 5], b16 & 0xFF);  // B low
    }
}

/// Helper to verify pixel data (RGBW 8-bit mode)
/// Verifies that the byte stream contains the expected RGBW pixel data
/// Note: UCS7604 pads data BEFORE LED values, so padding comes right after preamble
void verifyPixels8bitRGBW(fl::span<const uint8_t> bytes, fl::span<const RGBW8> pixels, size_t expected_padding) {
    const size_t PREAMBLE_SIZE = 15;
    const size_t BYTES_PER_PIXEL = 4;  // RGBW 8-bit

    // Verify padding bytes are 0x00 (padding comes after preamble, before LED data)
    for (size_t i = 0; i < expected_padding; i++) {
        CHECK_EQ(bytes[PREAMBLE_SIZE + i], 0x00);
    }

    // Verify pixel data (starts after preamble + padding)
    for (size_t i = 0; i < pixels.size(); i++) {
        size_t byte_offset = PREAMBLE_SIZE + expected_padding + (i * BYTES_PER_PIXEL);
        CHECK_EQ(bytes[byte_offset + 0], pixels[i].r);  // R
        CHECK_EQ(bytes[byte_offset + 1], pixels[i].g);  // G
        CHECK_EQ(bytes[byte_offset + 2], pixels[i].b);  // B
        CHECK_EQ(bytes[byte_offset + 3], pixels[i].w);  // W
    }
}

/// Helper to verify pixel data (RGBW 16-bit mode)
/// Verifies that the byte stream contains the expected RGBW pixel data
/// Note: UCS7604 pads data BEFORE LED values, so padding comes right after preamble
void verifyPixels16bitRGBW(fl::span<const uint8_t> bytes, fl::span<const RGBW16> pixels, size_t expected_padding) {
    const size_t PREAMBLE_SIZE = 15;
    const size_t BYTES_PER_PIXEL = 8;  // RGBW 16-bit

    // Verify padding bytes are 0x00 (padding comes after preamble, before LED data)
    for (size_t i = 0; i < expected_padding; i++) {
        CHECK_EQ(bytes[PREAMBLE_SIZE + i], 0x00);
    }

    // Verify pixel data (starts after preamble + padding)
    for (size_t i = 0; i < pixels.size(); i++) {
        size_t byte_offset = PREAMBLE_SIZE + expected_padding + (i * BYTES_PER_PIXEL);

        uint16_t r16 = pixels[i].r;
        uint16_t g16 = pixels[i].g;
        uint16_t b16 = pixels[i].b;
        uint16_t w16 = pixels[i].w;

        // Verify big-endian 16-bit values
        CHECK_EQ(bytes[byte_offset + 0], r16 >> 8);    // R high
        CHECK_EQ(bytes[byte_offset + 1], r16 & 0xFF);  // R low
        CHECK_EQ(bytes[byte_offset + 2], g16 >> 8);    // G high
        CHECK_EQ(bytes[byte_offset + 3], g16 & 0xFF);  // G low
        CHECK_EQ(bytes[byte_offset + 4], b16 >> 8);    // B high
        CHECK_EQ(bytes[byte_offset + 5], b16 & 0xFF);  // B low
        CHECK_EQ(bytes[byte_offset + 6], w16 >> 8);    // W high
        CHECK_EQ(bytes[byte_offset + 7], w16 & 0xFF);  // W low
    }
}

/// Generic test function for UCS7604 controllers
/// Tests preamble, color order conversion, and pixel data
/// @tparam RGB_ORDER Color order for the INPUT pixels (RGB, GRB, etc.)
/// @tparam MODE UCS7604Mode enum value (8-bit, 16-bit, etc.)
/// @param leds Input LED array
/// @return Captured byte stream from the controller
///
/// Note: UCS7604Controller always uses RGB for the wire protocol, but accepts
/// different color orders for input pixels which are converted internally
template<EOrder RGB_ORDER, fl::UCS7604Mode MODE>
fl::span<const uint8_t> testUCS7604Controller(fl::span<const CRGB> leds) {
    static constexpr int TEST_PIN = 10;

    fl::span<const uint8_t> captured;

    if (MODE == fl::UCS7604_MODE_8BIT_800KHZ) {
        // Test 8-bit mode - controller accepts RGB_ORDER and converts to RGB wire order internally
        static UCS7604TestController8bit<TEST_PIN, RGB_ORDER> controller;

        // Create pixels with specified color order
        PixelController<RGB_ORDER> pixels(leds.data(), leds.size(), ColorAdjustment::noAdjustment(), DISABLE_DITHER);

        controller.init();
        controller.showPixels(pixels);

        // Get captured bytes via IData interface
        captured = controller.getCapturedBytes();
    } else {
        // Test 16-bit mode - controller accepts RGB_ORDER and converts to RGB wire order internally
        static UCS7604TestController16bit<TEST_PIN, RGB_ORDER> controller;

        // Create pixels with specified color order
        PixelController<RGB_ORDER> pixels(leds.data(), leds.size(), ColorAdjustment::noAdjustment(), DISABLE_DITHER);

        controller.init();
        controller.showPixels(pixels);

        // Get captured bytes via IData interface
        captured = controller.getCapturedBytes();
    }

    return captured;
}

TEST_CASE("UCS7604 8-bit - RGB color order") {
    CRGB leds[] = {
        CRGB(0xFF, 0x00, 0x00),  // Red
        CRGB(0x00, 0xFF, 0x00),  // Green
        CRGB(0x00, 0x00, 0xFF)   // Blue
    };

    // RGB -> RGB (no conversion)
    CRGB expected[] = {
        CRGB(0xFF, 0x00, 0x00),
        CRGB(0x00, 0xFF, 0x00),
        CRGB(0x00, 0x00, 0xFF)
    };

    fl::span<const uint8_t> output = testUCS7604Controller<RGB, fl::UCS7604_MODE_8BIT_800KHZ>(leds);

    // Verify total size: 15 (preamble) + 9 (3 LEDs * 3 bytes) = 24
    REQUIRE_EQ(output.size(), 24);

    // Verify preamble with 8-bit mode byte
    verifyPreamble(output, PREAMBLE_8BIT_800KHZ);

    // Verify pixel data
    verifyPixels8bit(output, expected);
}

TEST_CASE("UCS7604 8-bit - GRB color order") {
    CRGB leds[] = {
        CRGB(0xFF, 0x00, 0x00),  // Red
        CRGB(0x00, 0xFF, 0x00),  // Green
        CRGB(0x00, 0x00, 0xFF)   // Blue
    };

    // GRB -> RGB conversion
    CRGB expected[] = {
        CRGB(0x00, 0xFF, 0x00),  // Red as GRB -> Green
        CRGB(0xFF, 0x00, 0x00),  // Green as GRB -> Red
        CRGB(0x00, 0x00, 0xFF)   // Blue as GRB -> Blue
    };

    fl::span<const uint8_t> output = testUCS7604Controller<GRB, fl::UCS7604_MODE_8BIT_800KHZ>(leds);

    // Verify total size: 15 (preamble) + 9 (3 LEDs * 3 bytes) = 24
    REQUIRE_EQ(output.size(), 24);

    // Verify preamble with 8-bit mode byte
    verifyPreamble(output, PREAMBLE_8BIT_800KHZ);

    // Verify pixel data
    verifyPixels8bit(output, expected);
}

TEST_CASE("UCS7604 8-bit - BRG color order") {
    CRGB leds[] = {
        CRGB(0xFF, 0x00, 0x00),  // Red
        CRGB(0x00, 0xFF, 0x00),  // Green
        CRGB(0x00, 0x00, 0xFF)   // Blue
    };

    // BRG -> RGB conversion
    CRGB expected[] = {
        CRGB(0x00, 0xFF, 0x00),  // Red as BRG -> Green
        CRGB(0x00, 0x00, 0xFF),  // Green as BRG -> Blue
        CRGB(0xFF, 0x00, 0x00)   // Blue as BRG -> Red
    };

    fl::span<const uint8_t> output = testUCS7604Controller<BRG, fl::UCS7604_MODE_8BIT_800KHZ>(leds);

    // Verify total size: 15 (preamble) + 9 (3 LEDs * 3 bytes) = 24
    REQUIRE_EQ(output.size(), 24);

    // Verify preamble with 8-bit mode byte
    verifyPreamble(output, PREAMBLE_8BIT_800KHZ);

    // Verify pixel data
    verifyPixels8bit(output, expected);
}

TEST_CASE("UCS7604 16-bit - RGB color order") {
    CRGB leds[] = {
        CRGB(127, 0, 0),  // Red (mid-range to show gamma curve)
        CRGB(0, 127, 0),  // Green (mid-range to show gamma curve)
        CRGB(0, 0, 127)   // Blue (mid-range to show gamma curve)
    };

    // RGB -> RGB (no conversion) - 8-bit to 16-bit with gamma 2.8 correction
    const uint16_t g0 = fl::gamma_2_8(0);
    const uint16_t g127 = fl::gamma_2_8(127);
    RGB16 expected[] = {
        RGB16(g127, g0, g0),  // Red
        RGB16(g0, g127, g0),  // Green
        RGB16(g0, g0, g127)   // Blue
    };

    fl::span<const uint8_t> output = testUCS7604Controller<RGB, fl::UCS7604_MODE_16BIT_800KHZ>(leds);

    // Verify total size: 15 (preamble) + 18 (3 LEDs * 6 bytes) = 33
    REQUIRE_EQ(output.size(), 33);

    // Verify preamble with 16-bit mode byte
    verifyPreamble(output, PREAMBLE_16BIT_800KHZ);

    // Verify pixel data
    verifyPixels16bit(output, expected);
}

TEST_CASE("UCS7604 16-bit - GRB color order") {
    CRGB leds[] = {
        CRGB(127, 0, 0),  // Red (mid-range to show gamma curve)
        CRGB(0, 127, 0),  // Green (mid-range to show gamma curve)
        CRGB(0, 0, 127)   // Blue (mid-range to show gamma curve)
    };

    // GRB -> RGB conversion with gamma 2.8 correction
    // When input is GRB order, it gets reordered to RGB for wire protocol
    const uint16_t g0 = fl::gamma_2_8(0);
    const uint16_t g127 = fl::gamma_2_8(127);
    RGB16 expected[] = {
        RGB16(g0, g127, g0),  // Red as GRB -> Green at wire
        RGB16(g127, g0, g0),  // Green as GRB -> Red at wire
        RGB16(g0, g0, g127)   // Blue as GRB -> Blue at wire
    };

    fl::span<const uint8_t> output = testUCS7604Controller<GRB, fl::UCS7604_MODE_16BIT_800KHZ>(leds);

    // Verify total size: 15 (preamble) + 18 (3 LEDs * 6 bytes) = 33
    REQUIRE_EQ(output.size(), 33);

    // Verify preamble with 16-bit mode byte
    verifyPreamble(output, PREAMBLE_16BIT_800KHZ);

    // Verify pixel data
    verifyPixels16bit(output, expected);
}

TEST_CASE("UCS7604 runtime brightness control") {
    // Test the global brightness control functions

    // Save original brightness
    fl::ucs7604::CurrentControl original = fl::ucs7604::brightness();

    // Test set_brightness and brightness functions with single value
    fl::ucs7604::set_brightness(fl::ucs7604::CurrentControl(0x08));
    fl::ucs7604::CurrentControl current = fl::ucs7604::brightness();
    CHECK_EQ(current.r, 0x08);
    CHECK_EQ(current.g, 0x08);
    CHECK_EQ(current.b, 0x08);
    CHECK_EQ(current.w, 0x08);

    // Test clamping to 4-bit range
    fl::ucs7604::set_brightness(fl::ucs7604::CurrentControl(0xFF));
    current = fl::ucs7604::brightness();
    CHECK_EQ(current.r, 0x0F);  // Should clamp to 0x0F
    CHECK_EQ(current.g, 0x0F);
    CHECK_EQ(current.b, 0x0F);
    CHECK_EQ(current.w, 0x0F);

    // Test individual channel control via struct
    fl::ucs7604::set_brightness(fl::ucs7604::CurrentControl(0x03, 0x05, 0x07, 0x09));
    current = fl::ucs7604::brightness();
    CHECK_EQ(current.r, 0x03);
    CHECK_EQ(current.g, 0x05);
    CHECK_EQ(current.b, 0x07);
    CHECK_EQ(current.w, 0x09);

    // Test individual channel control via inline function
    fl::ucs7604::set_brightness(0x02, 0x04, 0x06, 0x08);
    current = fl::ucs7604::brightness();
    CHECK_EQ(current.r, 0x02);
    CHECK_EQ(current.g, 0x04);
    CHECK_EQ(current.b, 0x06);
    CHECK_EQ(current.w, 0x08);

    // Test that controller uses global brightness
    fl::ucs7604::set_brightness(fl::ucs7604::CurrentControl(0x05));

    UCS7604TestController8bit<10, RGB> controller;

    CRGB leds[] = {
        CRGB(0xFF, 0x00, 0x00)  // Red
    };

    PixelController<RGB> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.init();
    controller.showPixels(pixels);

    fl::span<const uint8_t> output = controller.getCapturedBytes();

    // Verify preamble has the brightness value (0x05) in current control bytes
    // Preamble bytes 9-12 are RGBW current control
    CHECK_EQ(output[9], 0x05);   // R current
    CHECK_EQ(output[10], 0x05);  // G current
    CHECK_EQ(output[11], 0x05);  // B current
    CHECK_EQ(output[12], 0x05);  // W current

    // Restore original brightness
    fl::ucs7604::set_brightness(original);
}

TEST_CASE("UCS7604 brightness with color order - GRB") {
    // Save original brightness
    fl::ucs7604::CurrentControl original = fl::ucs7604::brightness();

    // Set different current for each channel
    // r=0x3 controls RED LEDs, g=0x5 controls GREEN LEDs, b=0x7 controls BLUE LEDs
    fl::ucs7604::set_brightness(0x3, 0x5, 0x7, 0x9);

    // For GRB color order:
    // - User's R channel -> wire position 1 (G) -> should get r_current (0x3)
    // - User's G channel -> wire position 0 (R) -> should get g_current (0x5)
    // - User's B channel -> wire position 2 (B) -> should get b_current (0x7)
    // - W channel -> wire position 3 -> should get w_current (0x9)

    UCS7604TestController8bit<10, GRB> controller;

    CRGB leds[] = {
        CRGB(0xFF, 0x00, 0x00)  // Red LED
    };

    PixelController<GRB> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.init();
    controller.showPixels(pixels);

    fl::span<const uint8_t> output = controller.getCapturedBytes();

    // Preamble bytes 9-12 are RGBW current control in wire order (RGB)
    // For GRB input order, the wire should have:
    // - Position 0 (wire R): g_current = 0x5 (because user's G goes to wire R in GRB)
    // - Position 1 (wire G): r_current = 0x3 (because user's R goes to wire G in GRB)
    // - Position 2 (wire B): b_current = 0x7 (because user's B stays at wire B)
    // - Position 3 (wire W): w_current = 0x9
    CHECK_EQ(output[9],  0x5);  // Wire R gets user G current
    CHECK_EQ(output[10], 0x3);  // Wire G gets user R current
    CHECK_EQ(output[11], 0x7);  // Wire B gets user B current
    CHECK_EQ(output[12], 0x9);  // Wire W gets user W current

    // Restore original brightness
    fl::ucs7604::set_brightness(original);
}

TEST_CASE("UCS7604 preamble updates with current control changes") {
    // Save original brightness
    fl::ucs7604::CurrentControl original = fl::ucs7604::brightness();

    UCS7604TestController8bit<10, RGB> controller;
    CRGB leds[] = { CRGB(0xFF, 0x00, 0x00) };

    // Test 1: Set all channels to same value
    fl::ucs7604::set_brightness(fl::ucs7604::CurrentControl(0x08));
    PixelController<RGB> pixels1(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.init();
    controller.showPixels(pixels1);
    fl::span<const uint8_t> output1 = controller.getCapturedBytes();

    CHECK_EQ(output1[9],  0x08);  // R current
    CHECK_EQ(output1[10], 0x08);  // G current
    CHECK_EQ(output1[11], 0x08);  // B current
    CHECK_EQ(output1[12], 0x08);  // W current

    // Test 2: Set individual channel values
    fl::ucs7604::set_brightness(0x03, 0x05, 0x07, 0x09);
    PixelController<RGB> pixels2(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.showPixels(pixels2);
    fl::span<const uint8_t> output2 = controller.getCapturedBytes();

    CHECK_EQ(output2[9],  0x03);  // R current
    CHECK_EQ(output2[10], 0x05);  // G current
    CHECK_EQ(output2[11], 0x07);  // B current
    CHECK_EQ(output2[12], 0x09);  // W current

    // Test 3: Test clamping - values > 0x0F should be clamped to 0x0F
    fl::ucs7604::set_brightness(0xFF, 0x1A, 0x23, 0x45);
    PixelController<RGB> pixels3(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.showPixels(pixels3);
    fl::span<const uint8_t> output3 = controller.getCapturedBytes();

    CHECK_EQ(output3[9],  0x0F);  // R current (0xFF clamped to 0x0F)
    CHECK_EQ(output3[10], 0x0A);  // G current (0x1A clamped to 0x0A)
    CHECK_EQ(output3[11], 0x03);  // B current (0x23 clamped to 0x03)
    CHECK_EQ(output3[12], 0x05);  // W current (0x45 clamped to 0x05)

    // Test 4: Test minimum values
    fl::ucs7604::set_brightness(0x00, 0x00, 0x00, 0x00);
    PixelController<RGB> pixels4(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.showPixels(pixels4);
    fl::span<const uint8_t> output4 = controller.getCapturedBytes();

    CHECK_EQ(output4[9],  0x00);  // R current
    CHECK_EQ(output4[10], 0x00);  // G current
    CHECK_EQ(output4[11], 0x00);  // B current
    CHECK_EQ(output4[12], 0x00);  // W current

    // Test 5: Test maximum valid values (0x0F)
    fl::ucs7604::set_brightness(0x0F, 0x0F, 0x0F, 0x0F);
    PixelController<RGB> pixels5(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.showPixels(pixels5);
    fl::span<const uint8_t> output5 = controller.getCapturedBytes();

    CHECK_EQ(output5[9],  0x0F);  // R current
    CHECK_EQ(output5[10], 0x0F);  // G current
    CHECK_EQ(output5[11], 0x0F);  // B current
    CHECK_EQ(output5[12], 0x0F);  // W current

    // Test 6: Test mixed valid values in range
    fl::ucs7604::set_brightness(0x01, 0x04, 0x08, 0x0C);
    PixelController<RGB> pixels6(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.showPixels(pixels6);
    fl::span<const uint8_t> output6 = controller.getCapturedBytes();

    CHECK_EQ(output6[9],  0x01);  // R current
    CHECK_EQ(output6[10], 0x04);  // G current
    CHECK_EQ(output6[11], 0x08);  // B current
    CHECK_EQ(output6[12], 0x0C);  // W current

    // Restore original brightness
    fl::ucs7604::set_brightness(original);
}

TEST_CASE("UCS7604 preamble updates with current control changes - GRB order") {
    // Save original brightness
    fl::ucs7604::CurrentControl original = fl::ucs7604::brightness();

    UCS7604TestController8bit<10, GRB> controller;
    CRGB leds[] = { CRGB(0xFF, 0x00, 0x00) };

    // Test with different current values for each channel
    // User sets: R=0x3, G=0x5, B=0x7, W=0x9
    // For GRB order, wire should receive: wire_R=0x5, wire_G=0x3, wire_B=0x7, wire_W=0x9
    fl::ucs7604::set_brightness(0x3, 0x5, 0x7, 0x9);
    PixelController<GRB> pixels1(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.init();
    controller.showPixels(pixels1);
    fl::span<const uint8_t> output1 = controller.getCapturedBytes();

    CHECK_EQ(output1[9],  0x5);  // Wire R gets user G current (0x5)
    CHECK_EQ(output1[10], 0x3);  // Wire G gets user R current (0x3)
    CHECK_EQ(output1[11], 0x7);  // Wire B gets user B current (0x7)
    CHECK_EQ(output1[12], 0x9);  // Wire W gets user W current (0x9)

    // Test clamping with GRB order
    // User sets: R=0xFF, G=0x1A, B=0x23, W=0x45
    // After clamping: R=0xF, G=0xA, B=0x3, W=0x5
    // For GRB order, wire should receive: wire_R=0xA, wire_G=0xF, wire_B=0x3, wire_W=0x5
    fl::ucs7604::set_brightness(0xFF, 0x1A, 0x23, 0x45);
    PixelController<GRB> pixels2(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.showPixels(pixels2);
    fl::span<const uint8_t> output2 = controller.getCapturedBytes();

    CHECK_EQ(output2[9],  0x0A);  // Wire R gets user G current (0x1A -> 0x0A)
    CHECK_EQ(output2[10], 0x0F);  // Wire G gets user R current (0xFF -> 0x0F)
    CHECK_EQ(output2[11], 0x03);  // Wire B gets user B current (0x23 -> 0x03)
    CHECK_EQ(output2[12], 0x05);  // Wire W gets user W current (0x45 -> 0x05)

    // Restore original brightness
    fl::ucs7604::set_brightness(original);
}

TEST_CASE("UCS7604 current control follows color order transformations") {
    // Save original brightness
    fl::ucs7604::CurrentControl original = fl::ucs7604::brightness();

    CRGB leds[] = { CRGB(0xFF, 0x00, 0x00) };

    // Set distinct current values for each channel so we can track them
    // R=0x1, G=0x2, B=0x3, W=0x4
    fl::ucs7604::set_brightness(0x1, 0x2, 0x3, 0x4);

    // Test RGB order - no transformation
    {
        UCS7604TestController8bit<10, RGB> controller;
        PixelController<RGB> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        controller.init();
        controller.showPixels(pixels);
        fl::span<const uint8_t> output = controller.getCapturedBytes();

        // RGB order: preamble should have R=0x1, G=0x2, B=0x3, W=0x4 (no swap)
        CHECK_EQ(output[9],  0x1);  // Wire R = user R
        CHECK_EQ(output[10], 0x2);  // Wire G = user G
        CHECK_EQ(output[11], 0x3);  // Wire B = user B
        CHECK_EQ(output[12], 0x4);  // Wire W = user W
    }

    // Test GRB order - R and G swapped
    {
        UCS7604TestController8bit<10, GRB> controller;
        PixelController<GRB> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        controller.init();
        controller.showPixels(pixels);
        fl::span<const uint8_t> output = controller.getCapturedBytes();

        // GRB order: preamble should have R=0x2, G=0x1, B=0x3, W=0x4 (R↔G swap)
        CHECK_EQ(output[9],  0x2);  // Wire R = user G (swapped)
        CHECK_EQ(output[10], 0x1);  // Wire G = user R (swapped)
        CHECK_EQ(output[11], 0x3);  // Wire B = user B (unchanged)
        CHECK_EQ(output[12], 0x4);  // Wire W = user W (unchanged)
    }

    // Test BRG order - rotate left (B→R→G→B)
    {
        UCS7604TestController8bit<10, BRG> controller;
        PixelController<BRG> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        controller.init();
        controller.showPixels(pixels);
        fl::span<const uint8_t> output = controller.getCapturedBytes();

        // BRG order: preamble should have R=0x3, G=0x1, B=0x2, W=0x4 (rotate left)
        CHECK_EQ(output[9],  0x3);  // Wire R = user B
        CHECK_EQ(output[10], 0x1);  // Wire G = user R
        CHECK_EQ(output[11], 0x2);  // Wire B = user G
        CHECK_EQ(output[12], 0x4);  // Wire W = user W (unchanged)
    }

    // Test RBG order - G and B swapped
    {
        UCS7604TestController8bit<10, RBG> controller;
        PixelController<RBG> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        controller.init();
        controller.showPixels(pixels);
        fl::span<const uint8_t> output = controller.getCapturedBytes();

        // RBG order: preamble should have R=0x1, G=0x3, B=0x2, W=0x4 (G↔B swap)
        CHECK_EQ(output[9],  0x1);  // Wire R = user R (unchanged)
        CHECK_EQ(output[10], 0x3);  // Wire G = user B (swapped)
        CHECK_EQ(output[11], 0x2);  // Wire B = user G (swapped)
        CHECK_EQ(output[12], 0x4);  // Wire W = user W (unchanged)
    }

    // Test GBR order - rotate right (G→B→R→G)
    {
        UCS7604TestController8bit<10, GBR> controller;
        PixelController<GBR> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        controller.init();
        controller.showPixels(pixels);
        fl::span<const uint8_t> output = controller.getCapturedBytes();

        // GBR order: Wire sends G,B,R which UCS7604 interprets as R,G,B registers
        // So R-register gets G value, G-register gets B value, B-register gets R value
        // Preamble current control: R-reg-current=g_current, G-reg-current=b_current, B-reg-current=r_current
        CHECK_EQ(output[9],  0x2);  // R-register current = user G current
        CHECK_EQ(output[10], 0x3);  // G-register current = user B current
        CHECK_EQ(output[11], 0x1);  // B-register current = user R current
        CHECK_EQ(output[12], 0x4);  // W-register current = user W current (unchanged)
    }

    // Test BGR order - reverse RGB
    {
        UCS7604TestController8bit<10, BGR> controller;
        PixelController<BGR> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        controller.init();
        controller.showPixels(pixels);
        fl::span<const uint8_t> output = controller.getCapturedBytes();

        // BGR order: preamble should have R=0x3, G=0x2, B=0x1, W=0x4 (reverse)
        CHECK_EQ(output[9],  0x3);  // Wire R = user B
        CHECK_EQ(output[10], 0x2);  // Wire G = user G (unchanged)
        CHECK_EQ(output[11], 0x1);  // Wire B = user R
        CHECK_EQ(output[12], 0x4);  // Wire W = user W (unchanged)
    }

    // Restore original brightness
    fl::ucs7604::set_brightness(original);
}

TEST_CASE("UCS7604 8-bit RGBW - 3 LEDs (no padding)") {
    // 3 LEDs RGBW 8-bit: 15 + (3*4) = 27 bytes (27 % 3 = 0, no padding)
    CRGB leds[] = {
        CRGB(0xFF, 0x00, 0x00),  // Red
        CRGB(0x00, 0xFF, 0x00),  // Green
        CRGB(0x00, 0x00, 0xFF)   // Blue
    };

    UCS7604TestController8bit<10, RGB> controller;
    controller.setRgbw(RgbwDefault());  // Enable RGBW mode on controller

    PixelController<RGB> pixels(leds, 3, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.init();
    controller.showPixels(pixels);

    fl::span<const uint8_t> output = controller.getCapturedBytes();

    // Expected RGBW values (white channel calculated from RGB)
    RGBW8 expected[] = {
        RGBW8(0xFF, 0x00, 0x00, 0x00),  // Red -> R=255, G=0, B=0, W=0
        RGBW8(0x00, 0xFF, 0x00, 0x00),  // Green -> R=0, G=255, B=0, W=0
        RGBW8(0x00, 0x00, 0xFF, 0x00)   // Blue -> R=0, G=0, B=255, W=0
    };

    // Verify total size: 15 (preamble) + 0 (padding) + 12 (3 LEDs * 4 bytes) = 27
    REQUIRE_EQ(output.size(), 27);

    // Verify preamble with 8-bit mode byte
    verifyPreamble(output, PREAMBLE_8BIT_800KHZ);

    // Verify pixel data with no padding
    verifyPixels8bitRGBW(output, expected, 0);
}

TEST_CASE("UCS7604 8-bit RGBW - 4 LEDs (2 bytes padding)") {
    // 4 LEDs RGBW 8-bit: 15 + (4*4) = 31 bytes (31 % 3 = 1, need 2 bytes padding)
    CRGB leds[] = {
        CRGB(0xFF, 0x00, 0x00),  // Red
        CRGB(0x00, 0xFF, 0x00),  // Green
        CRGB(0x00, 0x00, 0xFF),  // Blue
        CRGB(0xFF, 0xFF, 0x00)   // Yellow
    };

    UCS7604TestController8bit<10, RGB> controller;
    controller.setRgbw(RgbwDefault());  // Enable RGBW mode

    PixelController<RGB> pixels(leds, 4, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.init();
    controller.showPixels(pixels);

    fl::span<const uint8_t> output = controller.getCapturedBytes();

    // Expected RGBW values
    RGBW8 expected[] = {
        RGBW8(0xFF, 0x00, 0x00, 0x00),
        RGBW8(0x00, 0xFF, 0x00, 0x00),
        RGBW8(0x00, 0x00, 0xFF, 0x00),
        RGBW8(0xFF, 0xFF, 0x00, 0x00)
    };

    // Verify total size: 15 (preamble) + 2 (padding) + 16 (4 LEDs * 4 bytes) = 33
    REQUIRE_EQ(output.size(), 33);

    // Verify preamble with 8-bit mode byte
    verifyPreamble(output, PREAMBLE_8BIT_800KHZ);

    // Verify pixel data with 2 bytes padding
    verifyPixels8bitRGBW(output, expected, 2);
}

TEST_CASE("UCS7604 8-bit RGBW - 5 LEDs (1 byte padding)") {
    // 5 LEDs RGBW 8-bit: 15 + (5*4) = 35 bytes (35 % 3 = 2, need 1 byte padding)
    CRGB leds[] = {
        CRGB(0xFF, 0x00, 0x00),  // Red
        CRGB(0x00, 0xFF, 0x00),  // Green
        CRGB(0x00, 0x00, 0xFF),  // Blue
        CRGB(0xFF, 0xFF, 0x00),  // Yellow
        CRGB(0xFF, 0x00, 0xFF)   // Magenta
    };

    UCS7604TestController8bit<10, RGB> controller;
    controller.setRgbw(RgbwDefault());  // Enable RGBW mode

    PixelController<RGB> pixels(leds, 5, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.init();
    controller.showPixels(pixels);

    fl::span<const uint8_t> output = controller.getCapturedBytes();

    // Expected RGBW values
    RGBW8 expected[] = {
        RGBW8(0xFF, 0x00, 0x00, 0x00),
        RGBW8(0x00, 0xFF, 0x00, 0x00),
        RGBW8(0x00, 0x00, 0xFF, 0x00),
        RGBW8(0xFF, 0xFF, 0x00, 0x00),
        RGBW8(0xFF, 0x00, 0xFF, 0x00)
    };

    // Verify total size: 15 (preamble) + 1 (padding) + 20 (5 LEDs * 4 bytes) = 36
    REQUIRE_EQ(output.size(), 36);

    // Verify preamble with 8-bit mode byte
    verifyPreamble(output, PREAMBLE_8BIT_800KHZ);

    // Verify pixel data with 1 byte padding
    verifyPixels8bitRGBW(output, expected, 1);
}

TEST_CASE("UCS7604 16-bit RGBW - 3 LEDs (no padding)") {
    // 3 LEDs RGBW 16-bit: 15 + (3*8) = 39 bytes (39 % 3 = 0, no padding)
    CRGB leds[] = {
        CRGB(127, 0, 0),  // Red (mid-range to show gamma curve)
        CRGB(0, 127, 0),  // Green
        CRGB(0, 0, 127)   // Blue
    };

    UCS7604TestController16bit<10, RGB> controller;
    controller.setRgbw(RgbwDefault());  // Enable RGBW mode

    PixelController<RGB> pixels(leds, 3, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.init();
    controller.showPixels(pixels);

    fl::span<const uint8_t> output = controller.getCapturedBytes();

    // Expected RGBW values with gamma 2.8 correction
    const uint16_t g0 = fl::gamma_2_8(0);
    const uint16_t g127 = fl::gamma_2_8(127);
    RGBW16 expected[] = {
        RGBW16(g127, g0, g0, g0),  // Red
        RGBW16(g0, g127, g0, g0),  // Green
        RGBW16(g0, g0, g127, g0)   // Blue
    };

    // Verify total size: 15 (preamble) + 0 (padding) + 24 (3 LEDs * 8 bytes) = 39
    REQUIRE_EQ(output.size(), 39);

    // Verify preamble with 16-bit mode byte
    verifyPreamble(output, PREAMBLE_16BIT_800KHZ);

    // Verify pixel data with no padding
    verifyPixels16bitRGBW(output, expected, 0);
}

TEST_CASE("UCS7604 16-bit RGBW - 4 LEDs (2 bytes padding)") {
    // 4 LEDs RGBW 16-bit: 15 + (4*8) = 47 bytes (47 % 3 = 2, need 1 byte padding)
    CRGB leds[] = {
        CRGB(127, 0, 0),    // Red
        CRGB(0, 127, 0),    // Green
        CRGB(0, 0, 127),    // Blue
        CRGB(127, 127, 0)   // Yellow
    };

    UCS7604TestController16bit<10, RGB> controller;
    controller.setRgbw(RgbwDefault());  // Enable RGBW mode

    PixelController<RGB> pixels(leds, 4, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.init();
    controller.showPixels(pixels);

    fl::span<const uint8_t> output = controller.getCapturedBytes();

    // Expected RGBW values with gamma 2.8 correction
    const uint16_t g0 = fl::gamma_2_8(0);
    const uint16_t g127 = fl::gamma_2_8(127);
    RGBW16 expected[] = {
        RGBW16(g127, g0, g0, g0),
        RGBW16(g0, g127, g0, g0),
        RGBW16(g0, g0, g127, g0),
        RGBW16(g127, g127, g0, g0)
    };

    // Verify total size: 15 (preamble) + 1 (padding) + 32 (4 LEDs * 8 bytes) = 48
    REQUIRE_EQ(output.size(), 48);

    // Verify preamble with 16-bit mode byte
    verifyPreamble(output, PREAMBLE_16BIT_800KHZ);

    // Verify pixel data with 1 byte padding
    verifyPixels16bitRGBW(output, expected, 1);
}

TEST_CASE("UCS7604 16-bit RGBW - 5 LEDs (1 byte padding)") {
    // 5 LEDs RGBW 16-bit: 15 + (5*8) = 55 bytes (55 % 3 = 1, need 2 bytes padding)
    CRGB leds[] = {
        CRGB(127, 0, 0),    // Red
        CRGB(0, 127, 0),    // Green
        CRGB(0, 0, 127),    // Blue
        CRGB(127, 127, 0),  // Yellow
        CRGB(127, 0, 127)   // Magenta
    };

    UCS7604TestController16bit<10, RGB> controller;
    controller.setRgbw(RgbwDefault());  // Enable RGBW mode

    PixelController<RGB> pixels(leds, 5, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    controller.init();
    controller.showPixels(pixels);

    fl::span<const uint8_t> output = controller.getCapturedBytes();

    // Expected RGBW values with gamma 2.8 correction
    const uint16_t g0 = fl::gamma_2_8(0);
    const uint16_t g127 = fl::gamma_2_8(127);
    RGBW16 expected[] = {
        RGBW16(g127, g0, g0, g0),
        RGBW16(g0, g127, g0, g0),
        RGBW16(g0, g0, g127, g0),
        RGBW16(g127, g127, g0, g0),
        RGBW16(g127, g0, g127, g0)
    };

    // Verify total size: 15 (preamble) + 2 (padding) + 40 (5 LEDs * 8 bytes) = 57
    REQUIRE_EQ(output.size(), 57);

    // Verify preamble with 16-bit mode byte
    verifyPreamble(output, PREAMBLE_16BIT_800KHZ);

    // Verify pixel data with 2 bytes padding
    verifyPixels16bitRGBW(output, expected, 2);
}

} // anonymous namespace
