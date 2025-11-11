/// @file test_ucs7604.cpp
/// @brief Unit test for UCS7604 LED chipset protocol
///
/// UCS7604 Protocol Format:
/// - Preamble: 15 bytes (device config + current control)
/// - LED data: 3 bytes (RGB 8-bit) per LED
/// - Padding: 0-2 zero bytes to ensure total size divisible by 3

#include "test.h"
#include "FastLED.h"
#include "fl/chipsets/ucs7604.h"
#include "crgb.h"
#include "fl/vector.h"

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

// Test pin numbers (arbitrary values for testing, not used for actual hardware)
constexpr int FAKE_PIN_1 = 1;
constexpr int FAKE_PIN_2 = 2;

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
        // Capture raw RGB bytes
        capturedBytes.clear();
        PixelController<RGB> pixels_rgb = pixels;
        pixels_rgb.disableColorAdjustment();
        auto iterator = pixels_rgb.as_iterator(RgbwInvalid());

        while (iterator.has(1)) {
            uint8_t r, g, b;
            iterator.loadAndScaleRGB(&r, &g, &b);
            capturedBytes.push_back(r);
            capturedBytes.push_back(g);
            capturedBytes.push_back(b);
            iterator.advanceData();
        }
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
        CRGB(0xFF, 0x00, 0x00),  // Red
        CRGB(0x00, 0xFF, 0x00),  // Green
        CRGB(0x00, 0x00, 0xFF)   // Blue
    };

    // RGB -> RGB (no conversion) - 8-bit to 16-bit: value * 257 = (value << 8) | value
    RGB16 expected[] = {
        RGB16(0xFFFF, 0x0000, 0x0000),  // Red
        RGB16(0x0000, 0xFFFF, 0x0000),  // Green
        RGB16(0x0000, 0x0000, 0xFFFF)   // Blue
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
        CRGB(0xFF, 0x00, 0x00),  // Red
        CRGB(0x00, 0xFF, 0x00),  // Green
        CRGB(0x00, 0x00, 0xFF)   // Blue
    };

    // GRB -> RGB conversion - 8-bit to 16-bit: value * 257 = (value << 8) | value
    RGB16 expected[] = {
        RGB16(0x0000, 0xFFFF, 0x0000),  // Red as GRB -> Green
        RGB16(0xFFFF, 0x0000, 0x0000),  // Green as GRB -> Red
        RGB16(0x0000, 0x0000, 0xFFFF)   // Blue as GRB -> Blue
    };

    fl::span<const uint8_t> output = testUCS7604Controller<GRB, fl::UCS7604_MODE_16BIT_800KHZ>(leds);

    // Verify total size: 15 (preamble) + 18 (3 LEDs * 6 bytes) = 33
    REQUIRE_EQ(output.size(), 33);

    // Verify preamble with 16-bit mode byte
    verifyPreamble(output, PREAMBLE_16BIT_800KHZ);

    // Verify pixel data
    verifyPixels16bit(output, expected);
}

} // anonymous namespace
