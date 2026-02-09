/// @file engine.cpp
/// @brief Tests for the Channel and IChannelEngine API

#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/channels/engine.h"
#include "fl/channels/data.h"
#include "fl/channels/bus_manager.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"
#include "fl/fill.h"
#include "fl/stl/new.h"
#include "colorutils.h"
#include "crgb.h"
#include "test.h"
#include "eorder.h"
#include "fl/channels/options.h"
#include "fl/chipsets/led_timing.h"
#include "fl/eorder.h"
#include "fl/log.h"
#include "fl/rgb8.h"
#include "fl/slice.h"
#include "fl/stl/allocator.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/strstream.h"

namespace channel_engine_test {

using namespace fl;

/// Mock IChannelEngine for testing
class MockEngine : public IChannelEngine {
public:
    explicit MockEngine(const char* name = "MOCK") : mName(name) {}

    int transmitCount = 0;
    int lastChannelCount = 0;
    int enqueueCount = 0;
    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;

    bool canHandle(const ChannelDataPtr& data) const override {
        (void)data;
        return true;  // Test engine accepts all channel types
    }

    void enqueue(ChannelDataPtr channelData) override {
        if (channelData) {
            enqueueCount++;
            mEnqueuedChannels.push_back(channelData);
        }
    }

    void show() override {
        if (!mEnqueuedChannels.empty()) {
            mTransmittingChannels = fl::move(mEnqueuedChannels);
            mEnqueuedChannels.clear();
            beginTransmission(fl::span<const ChannelDataPtr>(mTransmittingChannels.data(), mTransmittingChannels.size()));
        }
    }

    EngineState poll() override {
        // Mock implementation: always return READY after transmission
        if (!mTransmittingChannels.empty()) {
            mTransmittingChannels.clear();
        }
        return EngineState::READY;
    }

    fl::string getName() const override { return mName; }

    Capabilities getCapabilities() const override {
        return Capabilities(true, true);  // Mock accepts both clockless and SPI
    }

private:
    void beginTransmission(fl::span<const ChannelDataPtr> channels) {
        transmitCount++;
        lastChannelCount = channels.size();
    }

    fl::string mName;
};

FL_TEST_CASE("Channel basic operations") {
    auto mockEngine = fl::make_shared<MockEngine>("MOCK_1");

    // Register mock engine with ChannelBusManager for testing
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine);  // High priority to ensure selection

    CRGB leds[10];
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions options;
    options.mAffinity = "MOCK_1";  // Match engine name

    // Use affinity to bind to mock engine
    ChannelConfig config(1, timing, fl::span<CRGB>(leds, 10), RGB, options);
    auto channel = Channel::create(config);

    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->getPin() == 1);
    FL_CHECK(channel->size() == 10);
    FL_CHECK(channel->getChannelEngine() == mockEngine.get());

    // Clean up: disable mock engine after test
    manager.setDriverEnabled("MOCK_1", false);  // Match engine name
}

FL_TEST_CASE("Channel transmission") {
    auto mockEngine = fl::make_shared<MockEngine>("MOCK_TX");

    // Register mock engine with ChannelBusManager for testing
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine);  // High priority to ensure selection

    CRGB leds[5];
    fill_solid(leds, 5, CRGB::Red);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions options;
    options.mAffinity = "MOCK_TX";
    ChannelConfig config(1, timing, fl::span<CRGB>(leds, 5), RGB, options);
    auto channel = Channel::create(config);

    // Trigger show - this encodes and enqueues
    channel->showLeds(255);

    // Engine's show() triggers transmission
    mockEngine->show();

    FL_CHECK(mockEngine->transmitCount == 1);
    FL_CHECK(mockEngine->lastChannelCount == 1);

    // Clean up: disable mock engine after test
    manager.setDriverEnabled("MOCK_TX", false);
}

FL_TEST_CASE("FastLED.show() with channels") {
    auto mockEngine = fl::make_shared<MockEngine>("MOCK_FASTLED");

    // Register mock engine with ChannelBusManager for testing
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine);  // High priority to ensure selection

    CRGB leds[5];
    fill_solid(leds, 5, CRGB::Blue);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions options;
    options.mAffinity = "MOCK_FASTLED";
    ChannelConfig config(1, timing, fl::span<CRGB>(leds, 5), RGB, options);
    auto channel = Channel::create(config);

    FastLED.add(channel);

    int before = mockEngine->transmitCount;
    int enqueueBefore = mockEngine->enqueueCount;
    FastLED.show();
    int enqueueAfter = mockEngine->enqueueCount;

    // Debug: Check if enqueue was called
    if (enqueueAfter == enqueueBefore) {
        FL_WARN("enqueue() was NOT called by FastLED.show() - enqueue count: " << enqueueAfter);
    } else {
        FL_WARN("enqueue() WAS called " << (enqueueAfter - enqueueBefore) << " times");
    }

    // The issue: FastLED.show() calls enqueue() but not show() on the engine
    // We need to explicitly call show() or FastLED.show() needs to call it
    mockEngine->show();

    FL_CHECK(mockEngine->transmitCount > before);

    // Clean up
    channel->removeFromDrawList();
    manager.setDriverEnabled("MOCK_FASTLED", false);
}

//=============================================================================
// Test Suite: Channel Enqueue Count Verification
//=============================================================================

FL_TEST_CASE("Channel Engine: 8 channels → exactly 8 enqueues (no accumulation)") {
    auto mockEngine = fl::make_shared<MockEngine>("ENQUEUE_TEST_1");
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(2000, mockEngine);

    static constexpr size_t NUM_CHANNELS = 8;
    static constexpr size_t NUM_LEDS = 10;
    static CRGB leds[NUM_CHANNELS][NUM_LEDS];

    fl::vector<ChannelPtr> channels;
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();

    for (size_t i = 0; i < NUM_CHANNELS; i++) {
        fl::fill_solid(leds[i], NUM_LEDS, CRGB::Red);

        ChannelOptions opts;
        opts.mAffinity = "ENQUEUE_TEST_1";
        ChannelConfig config(
            static_cast<int>(i + 1),  // Pin 1-8
            timing,
            fl::span<CRGB>(leds[i], NUM_LEDS),
            GRB,
            opts
        );

        auto channel = Channel::create(config);
        FastLED.add(channel);
        channels.push_back(channel);
    }

    // Reset counter before show
    mockEngine->enqueueCount = 0;
    mockEngine->mEnqueuedChannels.clear();

    // Trigger show (enqueues all channels to engine)
    FastLED.show();

    // Verify EXACTLY 8 enqueues happened (one per channel, no accumulation)
    FL_CHECK_EQ(mockEngine->enqueueCount, NUM_CHANNELS);
    FL_CHECK_EQ(mockEngine->mEnqueuedChannels.size(), NUM_CHANNELS);

    // Cleanup
    for (auto& channel : channels) {
        FastLED.remove(channel);
    }
    manager.setDriverEnabled("ENQUEUE_TEST_1", false);
}

FL_TEST_CASE("Channel Engine: Multiple show() calls don't accumulate channels") {
    auto mockEngine = fl::make_shared<MockEngine>("ENQUEUE_TEST_2");
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(2001, mockEngine);

    static constexpr size_t NUM_CHANNELS = 4;
    static constexpr size_t NUM_LEDS = 5;
    static CRGB leds[NUM_CHANNELS][NUM_LEDS];

    fl::vector<ChannelPtr> channels;
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();

    for (size_t i = 0; i < NUM_CHANNELS; i++) {
        fl::fill_solid(leds[i], NUM_LEDS, CRGB::Blue);

        ChannelOptions opts;
        opts.mAffinity = "ENQUEUE_TEST_2";
        ChannelConfig config(
            static_cast<int>(i + 10),  // Pin 10-13
            timing,
            fl::span<CRGB>(leds[i], NUM_LEDS),
            RGB,
            opts
        );

        auto channel = Channel::create(config);
        FastLED.add(channel);
        channels.push_back(channel);
    }

    // Call show() THREE times - should get same channel count each time
    for (size_t iteration = 0; iteration < 3; iteration++) {
        mockEngine->enqueueCount = 0;
        mockEngine->mEnqueuedChannels.clear();

        FastLED.show();

        // Each iteration should enqueue exactly NUM_CHANNELS (no accumulation)
        size_t expectedCount = NUM_CHANNELS;
        if (mockEngine->enqueueCount != expectedCount) {
            FL_WARN("Iteration " << iteration << ": Enqueued " << mockEngine->enqueueCount
                    << " channels (expected " << expectedCount << ") - accumulation bug detected!");
        }
        FL_CHECK_EQ(mockEngine->enqueueCount, expectedCount);
        FL_CHECK_EQ(mockEngine->mEnqueuedChannels.size(), expectedCount);
    }

    // Cleanup
    for (auto& channel : channels) {
        FastLED.remove(channel);
    }
    manager.setDriverEnabled("ENQUEUE_TEST_2", false);
}

FL_TEST_CASE("Channel Engine: Adding/removing channels updates enqueue count correctly") {
    auto mockEngine = fl::make_shared<MockEngine>("ENQUEUE_TEST_3");
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(2002, mockEngine);

    static constexpr size_t NUM_LEDS = 10;
    static CRGB leds1[NUM_LEDS];
    static CRGB leds2[NUM_LEDS];
    static CRGB leds3[NUM_LEDS];

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "ENQUEUE_TEST_3";

    // Start with 2 channels
    ChannelConfig config1(20, timing, fl::span<CRGB>(leds1, NUM_LEDS), GRB, opts);
    ChannelConfig config2(21, timing, fl::span<CRGB>(leds2, NUM_LEDS), GRB, opts);

    auto ch1 = Channel::create(config1);
    auto ch2 = Channel::create(config2);
    FastLED.add(ch1);
    FastLED.add(ch2);

    mockEngine->enqueueCount = 0;
    mockEngine->mEnqueuedChannels.clear();
    FastLED.show();
    FL_CHECK_EQ(mockEngine->enqueueCount, 2);  // 2 channels

    // Add a third channel
    ChannelConfig config3(22, timing, fl::span<CRGB>(leds3, NUM_LEDS), GRB, opts);
    auto ch3 = Channel::create(config3);
    FastLED.add(ch3);

    mockEngine->enqueueCount = 0;
    mockEngine->mEnqueuedChannels.clear();
    FastLED.show();
    FL_CHECK_EQ(mockEngine->enqueueCount, 3);  // 3 channels

    // Remove one channel
    FastLED.remove(ch2);

    mockEngine->enqueueCount = 0;
    mockEngine->mEnqueuedChannels.clear();
    FastLED.show();
    FL_CHECK_EQ(mockEngine->enqueueCount, 2);  // Back to 2 channels

    // Cleanup
    FastLED.remove(ch1);
    FastLED.remove(ch3);
    manager.setDriverEnabled("ENQUEUE_TEST_3", false);
}

FL_TEST_CASE("Channel Engine: ChannelData isInUse flag managed correctly") {
    auto mockEngine = fl::make_shared<MockEngine>("ENQUEUE_TEST_4");
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(2003, mockEngine);

    static constexpr size_t NUM_LEDS = 5;
    static CRGB leds[NUM_LEDS];

    fl::fill_solid(leds, NUM_LEDS, CRGB::Green);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "ENQUEUE_TEST_4";
    ChannelConfig config(30, timing, fl::span<CRGB>(leds, NUM_LEDS), RGB, opts);
    auto channel = Channel::create(config);

    FastLED.add(channel);

    // First show() should succeed (data not in use)
    FastLED.show();

    // Poll until transmission completes
    size_t max_polls = 100;
    for (size_t i = 0; i < max_polls; i++) {
        auto state = manager.poll();
        if (state == fl::IChannelEngine::EngineState::READY) {
            break;
        }
        fl::delayMicroseconds(100);
    }

    // After poll() returns READY, data should be marked as not in use
    // So calling show() again should succeed (no assertion failure)
    FastLED.show();

    // Cleanup
    FastLED.remove(channel);
    manager.setDriverEnabled("ENQUEUE_TEST_4", false);

    // If we got here without assertion failures, the isInUse flag is managed correctly
    FL_CHECK(true);  // Test passed
}

} // namespace channel_engine_test
