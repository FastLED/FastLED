/// @file engine.cpp
/// @brief Tests for the Channel and IChannelEngine API

#include "test.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/channels/engine.h"
#include "fl/channels/data.h"
#include "fl/channels/bus_manager.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"

namespace channel_engine_test {

using namespace fl;

/// Mock IChannelEngine for testing
class MockEngine : public IChannelEngine {
public:
    int transmitCount = 0;
    int lastChannelCount = 0;

    void enqueue(ChannelDataPtr channelData) override {
        if (channelData) {
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

    const char* getName() const override { return "MOCK"; }

private:
    void beginTransmission(fl::span<const ChannelDataPtr> channels) {
        transmitCount++;
        lastChannelCount = channels.size();
    }

    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;
};

TEST_CASE("Channel basic operations") {
    auto mockEngine = fl::make_shared<MockEngine>();

    // Register mock engine with ChannelBusManager for testing
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine, "MOCK");  // High priority to ensure selection

    CRGB leds[10];
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions options;
    options.mAffinity = "MOCK";

    // Use affinity to bind to mock engine
    ChannelConfig config(1, timing, fl::span<CRGB>(leds, 10), RGB, options);
    auto channel = Channel::create(config);

    REQUIRE(channel != nullptr);
    CHECK(channel->getPin() == 1);
    CHECK(channel->size() == 10);
    CHECK(channel->getChannelEngine() == mockEngine.get());

    // Clean up: disable mock engine after test
    manager.setDriverEnabled("MOCK", false);
}

TEST_CASE("Channel transmission") {
    auto mockEngine = fl::make_shared<MockEngine>();

    // Register mock engine with ChannelBusManager for testing
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine, "MOCK_TX");  // High priority to ensure selection

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

    CHECK(mockEngine->transmitCount == 1);
    CHECK(mockEngine->lastChannelCount == 1);

    // Clean up: disable mock engine after test
    manager.setDriverEnabled("MOCK_TX", false);
}

TEST_CASE("FastLED.show() with channels") {
    auto mockEngine = fl::make_shared<MockEngine>();

    // Register mock engine with ChannelBusManager for testing
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine, "MOCK_FASTLED");  // High priority to ensure selection

    CRGB leds[5];
    fill_solid(leds, 5, CRGB::Blue);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions options;
    options.mAffinity = "MOCK_FASTLED";
    ChannelConfig config(1, timing, fl::span<CRGB>(leds, 5), RGB, options);
    auto channel = Channel::create(config);

    FastLED.addLedChannel(channel);

    int before = mockEngine->transmitCount;
    FastLED.show();

    CHECK(mockEngine->transmitCount > before);

    // Clean up
    channel->removeFromDrawList();
    manager.setDriverEnabled("MOCK_FASTLED", false);
}

} // namespace channel_engine_test
