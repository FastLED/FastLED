/// @file engine.cpp
/// @brief Tests for the Channel and IChannelEngine API

#include "test.h"
#include "fl/channels/channel.h"
#include "fl/channels/channel_config.h"
#include "fl/channels/channel_engine.h"
#include "fl/channels/channel_data.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "ftl/vector.h"
#include "ftl/move.h"

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

private:
    void beginTransmission(fl::span<const ChannelDataPtr> channels) {
        transmitCount++;
        lastChannelCount = channels.size();
    }

    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;
};

TEST_CASE("Channel basic operations") {
    MockEngine engine;
    CRGB leds[10];
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    LEDSettings settings;

    ChannelConfig config(1, timing, fl::span<CRGB>(leds, 10), RGB, settings);
    auto channel = Channel::create(config, &engine);

    REQUIRE(channel != nullptr);
    CHECK(channel->getPin() == 1);
    CHECK(channel->size() == 10);
    CHECK(channel->getChannelEngine() == &engine);
}

TEST_CASE("Channel transmission") {
    MockEngine engine;
    CRGB leds[5];
    fill_solid(leds, 5, CRGB::Red);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    LEDSettings settings;
    ChannelConfig config(1, timing, fl::span<CRGB>(leds, 5), RGB, settings);
    auto channel = Channel::create(config, &engine);

    // Trigger show - this encodes and enqueues
    channel->showLeds(255);

    // Engine's show() triggers transmission
    engine.show();

    CHECK(engine.transmitCount == 1);
    CHECK(engine.lastChannelCount == 1);
}

TEST_CASE("FastLED.show() with channels") {
    MockEngine engine;
    CRGB leds[5];
    fill_solid(leds, 5, CRGB::Blue);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    LEDSettings settings;
    ChannelConfig config(1, timing, fl::span<CRGB>(leds, 5), RGB, settings);
    auto channel = Channel::create(config, &engine);

    FastLED.addLedChannel(channel);

    int before = engine.transmitCount;
    FastLED.show();

    CHECK(engine.transmitCount > before);

    // Clean up
    channel->removeFromDrawList();
}

} // namespace channel_engine_test
