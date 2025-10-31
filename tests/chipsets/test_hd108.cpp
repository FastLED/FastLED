/// @file test_hd108.cpp
/// @brief Unit tests for HD108 16-bit SPI LED chipset protocol
///
/// HD108 Protocol Format:
/// - Start frame: 8 bytes of 0x00
/// - LED frames: 8 bytes per LED
///   * 2 header bytes (brightness control)
///   * 6 data bytes (16-bit RGB, big-endian)
/// - End frame: (num_leds / 2) + 4 bytes of 0xFF
///
/// Header Byte Encoding (dual-byte brightness):
/// For 5-bit brightness value bri5:
/// - f0 = 0x80 | ((bri5 & 0x1F) << 2)
/// - f1 = ((bri5 & 0x07) << 5) | (bri5 & 0x1F)

#include "test.h"
#include "FastLED.h"
#include "chipsets.h"
#include "crgb.h"
#include "platforms/shared/active_strip_data/active_strip_data.h"
#include "platforms/shared/active_strip_tracker/active_strip_tracker.h"
#include "fl/ease.h"
#include "fl/span.h"
#include "fl/dbg.h"

using namespace fl;

namespace {

/// Helper to extract 16-bit big-endian value from byte span
u16 getBigEndian16(const fl::span<const u8>& bytes, size_t offset) {
    return (u16(bytes[offset]) << 8) | u16(bytes[offset + 1]);
}

/// Helper to decode 5-bit brightness from header bytes
u8 decodeBrightness(u8 f0, u8 f1) {
    // Extract brightness from f0 (bits 6-2)
    return (f0 & 0x7C) >> 2;
}

/// Helper to verify header bytes match expected 5-bit brightness
void checkHeaderBytes(u8 f0, u8 f1, u8 expected_bri5) {
    // Verify f0 encoding: 0x80 | (bri5 << 2)
    u8 expected_f0 = 0x80 | ((expected_bri5 & 0x1F) << 2);
    CHECK_EQ(f0, expected_f0);

    // Verify f1 encoding: ((bri5 & 0x07) << 5) | (bri5 & 0x1F)
    u8 expected_f1 = ((expected_bri5 & 0x07) << 5) | (expected_bri5 & 0x1F);
    CHECK_EQ(f1, expected_f1);

    // Verify decoded brightness matches
    u8 decoded = decodeBrightness(f0, f1);
    CHECK_EQ(decoded, expected_bri5);
}

/// Test fixture that exposes protected showPixels method
template<int DATA_PIN, fl::u8 CLOCK_PIN, EOrder RGB_ORDER>
class HD108TestController : public HD108Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> {
public:
    using HD108Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER>::showPixels;
};

/// Helper to capture bytes from a controller without using FastLED.show()
/// This provides complete test isolation by directly calling the controller
/// Returns an owned vector to avoid lifetime issues
template<int DATA_PIN, fl::u8 CLOCK_PIN, EOrder RGB_ORDER>
fl::vector<u8> captureBytes(const CRGB* leds, int num_leds, u8 brightness) {
    // CRITICAL: Ensure complete isolation between tests
    // Clear all previous strip data and reset tracking state
    ActiveStripData& stripData = ActiveStripData::Instance();
    stripData.onBeginFrame();  // Clears mStripMap
    ActiveStripTracker::resetForTesting();

    // Create ColorAdjustment with simplified setup
    // Since HD108Controller now uses loadByte(), we just need to set up brightness
    ColorAdjustment adjustment;
    adjustment.premixed = CRGB(255, 255, 255);  // No color correction - use raw pixel values
    #if FASTLED_HD_COLOR_MIXING
    adjustment.color = CRGB(255, 255, 255);      // No color correction
    adjustment.brightness = brightness;           // Set brightness for HD108
    #endif

    // Create PixelController with the LED data
    PixelController<RGB_ORDER> pixels(leds, num_leds, adjustment, DISABLE_DITHER);

    // Create test controller and show pixels directly
    // Each unique DATA_PIN creates a distinct controller type for isolation
    HD108TestController<DATA_PIN, CLOCK_PIN, RGB_ORDER> controller;
    controller.init();
    controller.showPixels(pixels);

    // Trigger the engine event that pushes data to ActiveStripData
    fl::EngineEvents::onEndShowLeds();

    // Get the captured data - should be exactly one strip after onBeginFrame() + showPixels()
    const auto& dataMap = stripData.getData();

    // Ensure exactly one strip is present (test isolation check)
    REQUIRE_MESSAGE(dataMap.size() == 1,
        "Expected exactly 1 strip after capture, got " << dataMap.size());

    // CRITICAL: Return a COPY of the data (owned vector) to avoid lifetime issues
    // The span in dataMap points to the controller's internal buffer (mBytes in StubSPIOutput)
    // which will be destroyed when the controller goes out of scope
    const auto& capturedSpan = dataMap.begin()->second;
    fl::vector<u8> result;
    result.assign(capturedSpan.begin(), capturedSpan.end());
    return result;
}

TEST_CASE("HD108 - Protocol format verification") {
    // Test single LED with full protocol verification:
    // - Start frame: 8 bytes of 0x00
    // - LED frame: header (2 bytes) + RGB (6 bytes, 16-bit big-endian per channel)
    // - End frame: (num_leds/2 + 4) bytes of 0xFF
    // - Brightness encoding in dual-byte header
    // - Gamma correction applied to RGB values
    CRGB leds[1] = {CRGB(255, 0, 0)};

    const fl::u8 expected_byte_sequence[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Start frame
        0xFC, 0xFF,                                        // Header (brightness=31)
        0xFF, 0xFF,                                        // Red channel (gamma_2_8(255))
        0x00, 0x00,                                        // Green channel (gamma_2_8(0))
        0x00, 0x00,                                        // Blue channel (gamma_2_8(0))
        0xFF, 0xFF, 0xFF, 0xFF                            // End frame (4 bytes)
    };

    fl::vector<u8> capturedBytes = captureBytes<11, 13, RGB>(leds, 1, 255);

    REQUIRE_EQ(capturedBytes.size(), sizeof(expected_byte_sequence));
    for (size_t i = 0; i < capturedBytes.size(); i++) {
        CHECK_EQ(capturedBytes[i], expected_byte_sequence[i]);
    }

    // Verify brightness encoding in header
    checkHeaderBytes(capturedBytes[8], capturedBytes[9], 31);

    // Verify gamma correction applied
    CHECK_EQ(getBigEndian16(capturedBytes, 10), gamma_2_8(255));
}

TEST_CASE("HD108 - Multi-LED with brightness and color order") {
    // Test multiple LEDs to verify:
    // - Correct byte count scaling with LED count
    // - End frame size calculation: (num_leds/2 + 4)
    // - Variable brightness mapping (128 -> bri5=16)
    // - GRB color ordering
    // - Gamma correction on all channels
    const int NUM_LEDS = 3;
    CRGB leds[NUM_LEDS] = {
        CRGB(255, 0, 0),    // Red
        CRGB(0, 255, 0),    // Green
        CRGB(128, 128, 128) // Gray
    };

    fl::vector<u8> capturedBytes = captureBytes<28, 29, GRB>(leds, NUM_LEDS, 128);

    // Total: 8 (start) + 24 (3 LEDs * 8) + 5 (end: 3/2 + 4) = 37 bytes
    CHECK_EQ(capturedBytes.size(), 37);

    // Verify brightness mapping (128 -> bri5=16) on first LED
    checkHeaderBytes(capturedBytes[8], capturedBytes[9], 16);

    // Verify LED 1 (Red) with GRB order: G=0, R=255, B=0
    CHECK_EQ(getBigEndian16(capturedBytes, 10), gamma_2_8(0));   // Green first
    CHECK_EQ(getBigEndian16(capturedBytes, 12), gamma_2_8(255)); // Red second
    CHECK_EQ(getBigEndian16(capturedBytes, 14), gamma_2_8(0));   // Blue third

    // Verify LED 2 (Green) with GRB order: G=255, R=0, B=0
    CHECK_EQ(getBigEndian16(capturedBytes, 18), gamma_2_8(255)); // Green first
    CHECK_EQ(getBigEndian16(capturedBytes, 20), gamma_2_8(0));   // Red second
    CHECK_EQ(getBigEndian16(capturedBytes, 22), gamma_2_8(0));   // Blue third

    // Verify end frame (5 bytes of 0xFF)
    for (size_t i = 32; i < 37; i++) {
        CHECK_EQ(capturedBytes[i], 0xFF);
    }
}

} // anonymous namespace
