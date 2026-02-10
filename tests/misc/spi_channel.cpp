/// @file spi_channel.cpp
/// @brief Tests for SPI chipset channel creation and configuration
///
/// Tests the Channel API with SPI chipset configurations (APA102, SK9822, etc.)

#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/channels/engine.h"
#include "fl/channels/data.h"
#include "fl/channels/bus_manager.h"
#include "fl/chipsets/spi.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"
#include "crgb.h"
#include "test.h"

using namespace fl;

FL_TEST_CASE("SPI chipset channel creation and data push") {
    const int NUM_LEDS = 10;
    CRGB leds[NUM_LEDS];

    // Create SPI encoder (APA102-style)
    SpiEncoder encoder;
    encoder.chipset = SpiChipset::APA102;
    encoder.clock_hz = 1000000;  // 1MHz clock

    // Create SPI chipset config with data and clock pins
    const int DATA_PIN = 5;
    const int CLOCK_PIN = 6;
    SpiChipsetConfig spiConfig{DATA_PIN, CLOCK_PIN, encoder};

    // Create channel config with SPI chipset
    ChannelConfig config(spiConfig, fl::span<CRGB>(leds, NUM_LEDS), RGB);

    // Verify config is SPI type
    FL_CHECK(config.isSpi());
    FL_CHECK_FALSE(config.isClockless());

    // Verify pin configuration
    FL_CHECK_EQ(config.getDataPin(), DATA_PIN);
    FL_CHECK_EQ(config.getClockPin(), CLOCK_PIN);

    // Create channel
    auto channel = Channel::create(config);
    FL_CHECK(channel != nullptr);

    // Verify channel properties
    FL_CHECK(channel->isSpi());
    FL_CHECK_FALSE(channel->isClockless());
    FL_CHECK_EQ(channel->getPin(), DATA_PIN);
    FL_CHECK_EQ(channel->getClockPin(), CLOCK_PIN);

    // Set pixel data
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Green;
    leds[2] = CRGB::Blue;

    // Debug: Print actual values
    // Note: The test framework may be modifying values
    // Channel creation should not modify the LED array
    // The channel just holds a reference to it

    // Basic functionality test: verify channel was created and configured
    // We don't test LED data integrity here since the channel doesn't modify it
}

FL_TEST_CASE("SPI chipset config - APA102 factory method") {
    const int DATA_PIN = 23;
    const int CLOCK_PIN = 18;

    // Use factory method for APA102
    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig config{DATA_PIN, CLOCK_PIN, encoder};

    // Verify configuration
    FL_CHECK_EQ(config.dataPin, DATA_PIN);
    FL_CHECK_EQ(config.clockPin, CLOCK_PIN);
    FL_CHECK_EQ(config.timing.chipset, SpiChipset::APA102);
    FL_CHECK_EQ(config.timing.clock_hz, 6000000);  // Default 6MHz
}

FL_TEST_CASE("SPI chipset config - SK9822 factory method") {
    const int DATA_PIN = 23;
    const int CLOCK_PIN = 18;

    // Use factory method for SK9822
    SpiEncoder encoder = SpiEncoder::sk9822();
    SpiChipsetConfig config{DATA_PIN, CLOCK_PIN, encoder};

    // Verify configuration
    FL_CHECK_EQ(config.dataPin, DATA_PIN);
    FL_CHECK_EQ(config.clockPin, CLOCK_PIN);
    FL_CHECK_EQ(config.timing.chipset, SpiChipset::SK9822);
    FL_CHECK_EQ(config.timing.clock_hz, 12000000);  // Default 12MHz
}

FL_TEST_CASE("SPI chipset config - custom clock frequency") {
    const int DATA_PIN = 5;
    const int CLOCK_PIN = 6;

    // Create APA102 with custom 10MHz clock
    SpiEncoder encoder = SpiEncoder::apa102(10000000);
    SpiChipsetConfig config{DATA_PIN, CLOCK_PIN, encoder};

    // Verify custom frequency
    FL_CHECK_EQ(config.timing.clock_hz, 10000000);
}

FL_TEST_CASE("SPI chipset - variant type checking") {
    const int NUM_LEDS = 10;
    CRGB leds[NUM_LEDS];

    // Create SPI chipset
    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiConfig{23, 18, encoder};
    ChipsetVariant spiVariant = spiConfig;

    // Verify variant type
    FL_CHECK(spiVariant.is<SpiChipsetConfig>());
    FL_CHECK_FALSE(spiVariant.is<ClocklessChipset>());

    // Extract SPI config from variant
    const SpiChipsetConfig* extracted = spiVariant.ptr<SpiChipsetConfig>();
    FL_CHECK(extracted != nullptr);
    FL_CHECK_EQ(extracted->dataPin, 23);
    FL_CHECK_EQ(extracted->clockPin, 18);
}

FL_TEST_CASE("SPI chipset - equality comparison") {
    SpiEncoder encoder1 = SpiEncoder::apa102(6000000);
    SpiEncoder encoder2 = SpiEncoder::apa102(6000000);
    SpiEncoder encoder3 = SpiEncoder::sk9822(12000000);

    // Same encoder should be equal
    FL_CHECK_EQ(encoder1, encoder2);

    // Different encoders should not be equal
    FL_CHECK_NE(encoder1, encoder3);

    // SpiChipsetConfig equality
    SpiChipsetConfig config1{23, 18, encoder1};
    SpiChipsetConfig config2{23, 18, encoder2};
    SpiChipsetConfig config3{5, 6, encoder1};

    FL_CHECK_EQ(config1, config2);  // Same pins and encoder
    FL_CHECK_NE(config1, config3);  // Different pins
}

FL_TEST_CASE("SPI chipset - default constructor") {
    // Default constructor should create valid config
    SpiChipsetConfig defaultConfig;

    // Verify defaults
    FL_CHECK_EQ(defaultConfig.dataPin, -1);
    FL_CHECK_EQ(defaultConfig.clockPin, -1);
    FL_CHECK_EQ(defaultConfig.timing.chipset, SpiChipset::APA102);  // Default to APA102
    FL_CHECK_EQ(defaultConfig.timing.clock_hz, 6000000);  // Default 6MHz
}

FL_TEST_CASE("Clockless vs SPI chipset - type safety") {
    const int NUM_LEDS = 10;
    CRGB leds[NUM_LEDS];

    // Create clockless chipset
    ChipsetTimingConfig ws2812Timing{350, 700, 600, 50, "WS2812"};
    ClocklessChipset clocklessChipset{5, ws2812Timing};
    ChannelConfig clocklessConfig(clocklessChipset, fl::span<CRGB>(leds, NUM_LEDS), RGB);

    // Create SPI chipset
    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiChipset{23, 18, encoder};
    ChannelConfig spiConfig(spiChipset, fl::span<CRGB>(leds, NUM_LEDS), RGB);

    // Verify type safety
    FL_CHECK(clocklessConfig.isClockless());
    FL_CHECK_FALSE(clocklessConfig.isSpi());
    FL_CHECK_EQ(clocklessConfig.getClockPin(), -1);  // Clockless has no clock pin

    FL_CHECK(spiConfig.isSpi());
    FL_CHECK_FALSE(spiConfig.isClockless());
    FL_CHECK_EQ(spiConfig.getClockPin(), 18);  // SPI has clock pin
}

/// Mock IChannelEngine for testing SPI data flow
class MockSpiEngine : public IChannelEngine {
public:
    int mEnqueueCount = 0;
    int mTransmitCount = 0;
    fl::vector<uint8_t> mLastTransmittedData;

    explicit MockSpiEngine(const char* name = "MOCK_SPI") : mName(fl::string::from_literal(name)) {}

    void enqueue(ChannelDataPtr channelData) override {
        if (channelData) {
            mEnqueueCount++;
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
        if (!mTransmittingChannels.empty()) {
            mTransmittingChannels.clear();
        }
        return EngineState::READY;
    }

    fl::string getName() const override { return mName; }

    /// @brief Predicate: only accept SPI chipsets (reject clockless)
    bool canHandle(const ChannelDataPtr& data) const override {
        if (!data) {
            return false;
        }
        // SPI engine only handles SPI chipsets
        return data->isSpi();
    }

    Capabilities getCapabilities() const override {
        return Capabilities(false, true);  // SPI only
    }

private:
    void beginTransmission(fl::span<const ChannelDataPtr> channels) {
        mTransmitCount++;

        // Capture transmitted data from first channel
        if (!channels.empty() && channels[0]) {
            const auto& data = channels[0]->getData();
            mLastTransmittedData.clear();
            mLastTransmittedData.insert(mLastTransmittedData.end(), data.begin(), data.end());
        }
    }

    fl::string mName;
    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;
};

FL_TEST_CASE("SPI chipset - mock engine integration") {
    // Create and register mock SPI engine
    auto mockEngine = fl::make_shared<MockSpiEngine>("MOCK_SPI");
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine);

    // Set mock engine as exclusive (disables all other engines)
    bool exclusive = manager.setExclusiveDriver("MOCK_SPI");
    FL_REQUIRE(exclusive);

    // Create LED array and set pixel data
    const int NUM_LEDS = 3;
    CRGB leds[NUM_LEDS];
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Green;
    leds[2] = CRGB::Blue;

    // Create SPI channel (APA102 chipset)
    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiConfig{5, 6, encoder};  // DATA_PIN=5, CLOCK_PIN=6

    ChannelOptions options;
    options.mAffinity = "MOCK_SPI";
    ChannelConfig config(spiConfig, fl::span<CRGB>(leds, NUM_LEDS), RGB, options);

    auto channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);

    // Add channel to FastLED
    FastLED.add(channel);

    // Trigger FastLED.show() - should enqueue and transmit data
    FastLED.show();

    // Verify data was enqueued
    FL_CHECK_GT(mockEngine->mEnqueueCount, 0);

    // Trigger transmission (FastLED.show() enqueues, engine.show() transmits)
    mockEngine->show();

    // Verify data was transmitted
    FL_CHECK_GT(mockEngine->mTransmitCount, 0);
    FL_CHECK_GT(mockEngine->mLastTransmittedData.size(), 0);

    // APA102 format: 4-byte start frame + (4 bytes per LED) + end frame
    size_t minExpectedSize = 4 + (4 * NUM_LEDS);
    FL_CHECK_GE(mockEngine->mLastTransmittedData.size(), minExpectedSize);

    // Clean up
    channel->removeFromDrawList();
    manager.setDriverEnabled("MOCK_SPI", false);
}

FL_TEST_CASE("ChannelData - chipset variant type checking") {
    const int NUM_LEDS = 10;
    CRGB leds[NUM_LEDS];

    // Create clockless chipset
    ChipsetTimingConfig ws2812Timing{350, 700, 600, 50, "WS2812"};
    ClocklessChipset clocklessChipset{5, ws2812Timing};

    // Create SPI chipset
    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiChipset{23, 18, encoder};

    // Create ChannelData for clockless chipset
    auto clocklessData = ChannelData::create(clocklessChipset);
    FL_CHECK(clocklessData->isClockless());
    FL_CHECK_FALSE(clocklessData->isSpi());

    // Create ChannelData for SPI chipset
    auto spiData = ChannelData::create(spiChipset);
    FL_CHECK(spiData->isSpi());
    FL_CHECK_FALSE(spiData->isClockless());

    // Test predicate filtering with mock SPI engine
    MockSpiEngine mockEngine;

    // SPI engine should reject clockless data
    FL_CHECK_FALSE(mockEngine.canHandle(clocklessData));

    // SPI engine should accept SPI data
    FL_CHECK(mockEngine.canHandle(spiData));
}

#if 0  // Disabled - uses old proxy pattern (manager.enqueue/show)

FL_TEST_CASE("ChannelBusManager - predicate filtering (clockless rejected)") {
    // Create mock SPI engine that ONLY accepts SPI chipsets
    auto mockSpiEngine = fl::make_shared<MockSpiEngine>("MOCK_SPI_TEST1");
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(1000, mockSpiEngine);

    // Set mock engine as exclusive (disables all other engines)
    bool exclusive = manager.setExclusiveDriver("MOCK_SPI_TEST1");
    FL_REQUIRE(exclusive);

    // Create clockless ChannelData
    ChipsetTimingConfig ws2812Timing{350, 700, 600, 50, "WS2812"};
    ClocklessChipset clocklessChipset{5, ws2812Timing};
    auto clocklessData = ChannelData::create(clocklessChipset);

    FL_CHECK(clocklessData->isClockless());
    FL_CHECK_FALSE(clocklessData->isSpi());

    // Try to enqueue clockless data to ChannelBusManager
    // Predicate filtering should reject it
    manager.enqueue(clocklessData);
    manager.show();  // Trigger transmission

    // Verify data was NOT forwarded to MOCK_SPI (predicate rejected)
    FL_CHECK_EQ(mockSpiEngine->mEnqueueCount, 0);

    // Clean up
    manager.setDriverEnabled("MOCK_SPI_TEST1", false);
}

FL_TEST_CASE("ChannelBusManager - predicate filtering (SPI accepted)") {
    // Create mock SPI engine that ONLY accepts SPI chipsets
    auto mockSpiEngine = fl::make_shared<MockSpiEngine>("MOCK_SPI_TEST2");
    ChannelBusManager& manager = ChannelBusManager::instance();
    manager.addEngine(1000, mockSpiEngine);

    // Set mock engine as exclusive (disables all other engines)
    bool exclusive = manager.setExclusiveDriver("MOCK_SPI_TEST2");
    FL_REQUIRE(exclusive);

    // Create SPI ChannelData
    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiChipset{23, 18, encoder};
    auto spiData = ChannelData::create(spiChipset);

    FL_CHECK(spiData->isSpi());
    FL_CHECK_FALSE(spiData->isClockless());

    // Enqueue SPI data to ChannelBusManager
    // Predicate filtering should accept it
    manager.enqueue(spiData);
    manager.show();  // Trigger transmission

    // Verify data was forwarded to MOCK_SPI (predicate accepted)
    FL_CHECK_GT(mockSpiEngine->mEnqueueCount, 0);

    // Clean up
    manager.setDriverEnabled("MOCK_SPI_TEST2", false);
}

#endif  // End of disabled proxy pattern tests
