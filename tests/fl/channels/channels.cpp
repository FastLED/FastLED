/// @file tests/fl/channels/channels.cpp
/// @brief Test suite for Channel API with addressing and color order

#include "fl/channels/channel.h"
#include "fl/xymap.h"
#include "fl/xmap.h"
#include "fl/screenmap.h"
#include "fl/stl/span.h"
#include "fl/fltest.h"
#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/channels/manager.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/gfx/fill.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/scope_exit.h"

using namespace fl;

// ============ Channel + Addressing Integration Tests ============
// End-to-end byte capture tests for Channel API with XYMap addressing

/// @brief Mock driver to capture encoded channel data for byte verification
class ByteCapturingMockEngine : public IChannelDriver {
public:
    explicit ByteCapturingMockEngine(const char* name = "BYTE_CAPTURE")
        : mName(name) {}

    int transmitCount = 0;
    fl::vector<ChannelDataPtr> mCapturedChannels;

    bool canHandle(const ChannelDataPtr& data) const override {
        (void)data;
        return true;
    }

    void enqueue(ChannelDataPtr channelData) override {
        if (channelData) {
            mCapturedChannels.push_back(channelData);
        }
    }

    void show() override {
        // Trigger transmission in real drivers, but for mocking
        // we just keep the data as-is for inspection
    }

    DriverState poll() override {
        return DriverState::READY;
    }

    fl::string getName() const override { return mName; }

    Capabilities getCapabilities() const override {
        return Capabilities(true, true);
    }

private:
    fl::string mName;
};

FL_TEST_CASE("Serpentine 2x2 with APA102 encodes pixels in expected byte order") {
    const int WIDTH = 2, HEIGHT = 2;
    const int NUM_LEDS = WIDTH * HEIGHT;

    // Create a 2x2 grid with known distinct colors
    CRGB workspace[NUM_LEDS] = {
        CRGB(255, 0, 0),      // Index 0: Red at (0,0)
        CRGB(0, 255, 0),      // Index 1: Green at (1,0)
        CRGB(0, 0, 255),      // Index 2: Blue at (0,1)
        CRGB(255, 255, 0),    // Index 3: Yellow at (1,1)
    };

    // Create mock engine to capture encoded bytes
    auto mockEngine = fl::make_shared<ByteCapturingMockEngine>("BYTE_CAPTURE_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(2000, mockEngine);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiConfig{5, 6, encoder};
    ChannelOptions options;
    options.mAffinity = "BYTE_CAPTURE_TEST";

    ChannelConfig config(spiConfig, fl::span<CRGB>(workspace, NUM_LEDS), RGB, options);
    auto channel = Channel::create(config);
    FL_CHECK(channel != nullptr);

    // Set up automatic cleanup at scope exit
    auto cleanup = fl::make_scope_exit([&]() {
        channel->removeFromDrawList();
        manager.removeDriver(mockEngine);
    });

    // Apply serpentine addressing
    fl::XYMap serpentine = fl::XYMap::constructSerpentine(WIDTH, HEIGHT);
    channel->setScreenMap(serpentine);

    // Add channel to FastLED and trigger show (encodes pixels)
    FastLED.add(channel);
    mockEngine->mCapturedChannels.clear();
    FastLED.show();

    // Verify we captured the encoded data
    FL_CHECK(mockEngine->mCapturedChannels.size() > 0);

    if (!mockEngine->mCapturedChannels.empty()) {
        const auto& channelData = mockEngine->mCapturedChannels[0];
        const auto& encodedBytes = channelData->getData();

        // APA102 format: 4-byte start frame (0x00 0x00 0x00 0x00),
        // then for each LED: [brightness/0xFF][B][G][R], then end frame
        // For 4 LEDs with serpentine order [0,1,3,2]: Red, Green, Yellow, Blue

        // Verify we have enough bytes (4 start + 4*4 LED data + at least 1 end)
        FL_CHECK(encodedBytes.size() >= 4 + (NUM_LEDS * 4));

        // Verify start frame (4 zero bytes for APA102)
        if (encodedBytes.size() >= 4) {
            FL_CHECK_EQ(encodedBytes[0], 0x00);
            FL_CHECK_EQ(encodedBytes[1], 0x00);
            FL_CHECK_EQ(encodedBytes[2], 0x00);
            FL_CHECK_EQ(encodedBytes[3], 0x00);
        }

        // Verify LED data in serpentine order: [pixel0=Red, pixel1=Green, pixel3=Yellow, pixel2=Blue]
        // Each LED is 4 bytes: [0xFF or brightness][B][G][R]
        // Start of LED data is at byte 4
        size_t ledStart = 4;

        // LED0 (Red at 0,0): R=255, G=0, B=0 → [0xFF][0x00][0x00][0xFF]
        if (encodedBytes.size() > ledStart + 3) {
            FL_CHECK_EQ(encodedBytes[ledStart + 1], 0x00);  // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 2], 0x00);  // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 3], 0xFF);  // Red
        }

        // LED1 (Green at 1,0): R=0, G=255, B=0 → [0xFF][0x00][0xFF][0x00]
        if (encodedBytes.size() > ledStart + 7) {
            FL_CHECK_EQ(encodedBytes[ledStart + 5], 0x00);  // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 6], 0xFF);  // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 7], 0x00);  // Red
        }

        // LED2 (Yellow at 1,1): R=255, G=255, B=0 → [0xFF][0x00][0xFF][0xFF]
        if (encodedBytes.size() > ledStart + 11) {
            FL_CHECK_EQ(encodedBytes[ledStart + 9], 0x00);  // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 10], 0xFF); // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 11], 0xFF); // Red
        }

        // LED3 (Blue at 0,1): R=0, G=0, B=255 → [0xFF][0xFF][0x00][0x00]
        if (encodedBytes.size() > ledStart + 15) {
            FL_CHECK_EQ(encodedBytes[ledStart + 13], 0xFF); // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 14], 0x00); // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 15], 0x00); // Red
        }
    }
}

FL_TEST_CASE("Serpentine 2x2 with WS2812 GRB encodes pixels in expected byte order") {
    const int WIDTH = 2, HEIGHT = 2;
    const int NUM_LEDS = WIDTH * HEIGHT;

    // Create a 2x2 grid with known distinct colors
    CRGB workspace[NUM_LEDS] = {
        CRGB(255, 0, 0),      // Index 0: Red at (0,0)
        CRGB(0, 255, 0),      // Index 1: Green at (1,0)
        CRGB(0, 0, 255),      // Index 2: Blue at (0,1)
        CRGB(255, 255, 0),    // Index 3: Yellow at (1,1)
    };

    // Create mock engine to capture encoded bytes
    auto mockEngine = fl::make_shared<ByteCapturingMockEngine>("WS2812_BYTE_CAPTURE");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(2001, mockEngine);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions options;
    options.mAffinity = "WS2812_BYTE_CAPTURE";

    ChannelConfig config(1, timing, fl::span<CRGB>(workspace, NUM_LEDS), GRB, options);
    auto channel = Channel::create(config);
    FL_CHECK(channel != nullptr);

    // Set up automatic cleanup at scope exit
    auto cleanup = fl::make_scope_exit([&]() {
        channel->removeFromDrawList();
        manager.removeDriver(mockEngine);
    });

    // Apply serpentine addressing
    fl::XYMap serpentine = fl::XYMap::constructSerpentine(WIDTH, HEIGHT);
    channel->setScreenMap(serpentine);

    // Add channel to FastLED and trigger show (encodes pixels)
    FastLED.add(channel);
    mockEngine->mCapturedChannels.clear();
    FastLED.show();

    // Verify we captured the encoded data
    FL_CHECK(mockEngine->mCapturedChannels.size() > 0);

    if (!mockEngine->mCapturedChannels.empty()) {
        const auto& channelData = mockEngine->mCapturedChannels[0];
        const auto& encodedBytes = channelData->getData();

        // WS2812 format: No preamble, just raw GRB bytes (3 bytes per LED)
        // Serpentine order [0,1,3,2]: Red, Green, Yellow, Blue
        // Red (R=255, G=0, B=0): GRB order → [G][R][B] = [0x00][0xFF][0x00]
        // Green (R=0, G=255, B=0): GRB order → [G][R][B] = [0xFF][0x00][0x00]
        // Yellow (R=255, G=255, B=0): GRB order → [G][R][B] = [0xFF][0xFF][0x00]
        // Blue (R=0, G=0, B=255): GRB order → [G][R][B] = [0x00][0x00][0xFF]

        // Verify we have enough bytes (3 bytes per LED * 4 LEDs = 12 bytes minimum)
        // WS2812 may add reset bytes at the end, so we just check minimum
        FL_CHECK(encodedBytes.size() >= NUM_LEDS * 3);

        // Verify LED data in serpentine order with GRB color order
        // LED0 (Red at 0,0) in GRB: [0x00][0xFF][0x00]
        if (encodedBytes.size() > 2) {
            FL_CHECK_EQ(encodedBytes[0], 0x00);  // Green component
            FL_CHECK_EQ(encodedBytes[1], 0xFF);  // Red component
            FL_CHECK_EQ(encodedBytes[2], 0x00);  // Blue component
        }

        // LED1 (Green at 1,0) in GRB: [0xFF][0x00][0x00]
        if (encodedBytes.size() > 5) {
            FL_CHECK_EQ(encodedBytes[3], 0xFF);  // Green component
            FL_CHECK_EQ(encodedBytes[4], 0x00);  // Red component
            FL_CHECK_EQ(encodedBytes[5], 0x00);  // Blue component
        }

        // LED2 (Yellow at 1,1) in GRB: [0xFF][0xFF][0x00]
        if (encodedBytes.size() > 8) {
            FL_CHECK_EQ(encodedBytes[6], 0xFF);  // Green component
            FL_CHECK_EQ(encodedBytes[7], 0xFF);  // Red component
            FL_CHECK_EQ(encodedBytes[8], 0x00);  // Blue component
        }

        // LED3 (Blue at 0,1) in GRB: [0x00][0x00][0xFF]
        if (encodedBytes.size() > 11) {
            FL_CHECK_EQ(encodedBytes[9], 0x00);   // Green component
            FL_CHECK_EQ(encodedBytes[10], 0x00);  // Red component
            FL_CHECK_EQ(encodedBytes[11], 0xFF);  // Blue component
        }
    }
}

FL_TEST_CASE("XMap reverse addressing with APA102 encodes pixels in reverse order") {
    const int NUM_LEDS = 4;

    // Create a 4-LED strip with distinct colors
    CRGB workspace[NUM_LEDS] = {
        CRGB(255, 0, 0),      // Index 0: Red
        CRGB(0, 255, 0),      // Index 1: Green
        CRGB(0, 0, 255),      // Index 2: Blue
        CRGB(255, 255, 0),    // Index 3: Yellow
    };

    // Create mock engine to capture encoded bytes
    auto mockEngine = fl::make_shared<ByteCapturingMockEngine>("XMAP_CAPTURE_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(2001, mockEngine);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiConfig{5, 6, encoder};
    ChannelOptions options;
    options.mAffinity = "XMAP_CAPTURE_TEST";

    ChannelConfig config(spiConfig, fl::span<CRGB>(workspace, NUM_LEDS), RGB, options);
    auto channel = Channel::create(config);
    FL_CHECK(channel != nullptr);

    // Set up automatic cleanup at scope exit
    auto cleanup = fl::make_scope_exit([&]() {
        channel->removeFromDrawList();
        manager.removeDriver(mockEngine);
    });

    // Apply reverse addressing using XMap
    fl::XMap reverse(NUM_LEDS, true);  // true = reverse order
    channel->setScreenMap(reverse);

    // Add channel to FastLED and trigger show (encodes pixels)
    FastLED.add(channel);
    mockEngine->mCapturedChannels.clear();
    FastLED.show();

    // Verify we captured the encoded data
    FL_CHECK(mockEngine->mCapturedChannels.size() > 0);

    if (!mockEngine->mCapturedChannels.empty()) {
        const auto& channelData = mockEngine->mCapturedChannels[0];
        const auto& encodedBytes = channelData->getData();

        // APA102 format: 4-byte start frame, then [0xFF][B][G][R] per LED
        // With reverse XMap, physical order should be [3,2,1,0]: Yellow, Blue, Green, Red
        FL_CHECK(encodedBytes.size() >= 4 + (NUM_LEDS * 4));

        // Verify start frame
        if (encodedBytes.size() >= 4) {
            FL_CHECK_EQ(encodedBytes[0], 0x00);
            FL_CHECK_EQ(encodedBytes[1], 0x00);
            FL_CHECK_EQ(encodedBytes[2], 0x00);
            FL_CHECK_EQ(encodedBytes[3], 0x00);
        }

        size_t ledStart = 4;

        // Physical LED 0 (maps to source 3 = Yellow): [0xFF][0x00][0xFF][0xFF]
        if (encodedBytes.size() > ledStart + 3) {
            FL_CHECK_EQ(encodedBytes[ledStart + 1], 0x00);  // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 2], 0xFF);  // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 3], 0xFF);  // Red
        }

        // Physical LED 1 (maps to source 2 = Blue): [0xFF][0xFF][0x00][0x00]
        if (encodedBytes.size() > ledStart + 7) {
            FL_CHECK_EQ(encodedBytes[ledStart + 5], 0xFF);  // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 6], 0x00);  // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 7], 0x00);  // Red
        }

        // Physical LED 2 (maps to source 1 = Green): [0xFF][0x00][0xFF][0x00]
        if (encodedBytes.size() > ledStart + 11) {
            FL_CHECK_EQ(encodedBytes[ledStart + 9], 0x00);   // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 10], 0xFF);  // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 11], 0x00);  // Red
        }

        // Physical LED 3 (maps to source 0 = Red): [0xFF][0x00][0x00][0xFF]
        if (encodedBytes.size() > ledStart + 15) {
            FL_CHECK_EQ(encodedBytes[ledStart + 13], 0x00);  // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 14], 0x00);  // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 15], 0xFF);  // Red
        }
    }
}
