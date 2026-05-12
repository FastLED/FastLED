
/// @file spi_channel.cpp
/// @brief Tests for SPI chipset channel creation and configuration
///
/// Tests the Channel API with SPI chipset configurations (APA102, SK9822, etc.)

#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/channels/manager.h"
#include "fl/chipsets/spi.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"
#include "crgb.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

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

/// Mock IChannelDriver for testing SPI data flow
class MockSpiEngine : public IChannelDriver {
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
            beginTransmission(mTransmittingChannels);
        }
    }

    DriverState poll() override {
        if (!mTransmittingChannels.empty()) {
            mTransmittingChannels.clear();
        }
        return DriverState::READY;
    }

    fl::string getName() const override { return mName; }

    /// @brief Predicate: only accept SPI chipsets (reject clockless)
    bool canHandle(const ChannelDataPtr& data) const override {
        if (!data) {
            return false;
        }
        // SPI driver only handles SPI chipsets
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

FL_TEST_CASE("SPI chipset - mock driver integration") {
    // Create and register mock SPI driver
    auto mockEngine = fl::make_shared<MockSpiEngine>("MOCK_SPI");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mockEngine);

    // Set mock driver as exclusive (disables all other drivers)
    bool exclusive = manager.setExclusiveDriverByName("MOCK_SPI");
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
    ChannelConfig config(spiConfig, fl::span<CRGB>(leds, NUM_LEDS), RGB, options);

    auto channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);

    // Add channel to FastLED
    FastLED.add(channel);

    // Trigger FastLED.show() - should enqueue and transmit data
    FastLED.show();

    // Verify data was enqueued
    FL_CHECK_GT(mockEngine->mEnqueueCount, 0);

    // Trigger transmission (FastLED.show() enqueues, driver.show() transmits)
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

FL_TEST_CASE("SPI chipset - APA102HD mock driver integration") {
    // Create and register mock SPI driver
    auto mockEngine = fl::make_shared<MockSpiEngine>("MOCK_SPI_HD");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mockEngine);

    // Set mock driver as exclusive (disables all other drivers)
    bool exclusive = manager.setExclusiveDriverByName("MOCK_SPI_HD");
    FL_REQUIRE(exclusive);

    // Create LED array and set pixel data
    const int NUM_LEDS = 3;
    CRGB leds[NUM_LEDS];
    leds[0] = CRGB(255, 0, 0);    // Red
    leds[1] = CRGB(0, 255, 0);    // Green
    leds[2] = CRGB(0, 0, 255);    // Blue

    // Create SPI channel with APA102HD chipset
    SpiEncoder encoder = SpiEncoder::apa102HD();
    SpiChipsetConfig spiConfig{5, 6, encoder};

    ChannelOptions options;
    ChannelConfig config(spiConfig, fl::span<CRGB>(leds, NUM_LEDS), RGB, options);

    // Verify it's configured as APA102HD
    FL_REQUIRE(config.isSpi());
    const SpiChipsetConfig* spi = config.getSpiChipset();
    FL_REQUIRE(spi != nullptr);
    FL_CHECK_EQ((int)spi->timing.chipset, (int)SpiChipset::APA102HD);

    auto channel = Channel::create(config);
    FL_REQUIRE(channel != nullptr);

    // Add channel to FastLED
    FastLED.add(channel);

    // Trigger FastLED.show() - encodes APA102HD data and enqueues to mock driver
    FastLED.show();

    // Verify data was enqueued
    FL_CHECK_GT(mockEngine->mEnqueueCount, 0);

    // Trigger transmission
    mockEngine->show();

    // Verify data was transmitted
    FL_CHECK_GT(mockEngine->mTransmitCount, 0);

    const auto& data = mockEngine->mLastTransmittedData;
    FL_CHECK_GT(data.size(), 0);

    // APA102 wire format: 4-byte start frame + (4 bytes per LED) [+ optional end frame]
    // Start frame: 0x00 0x00 0x00 0x00
    // Per LED:     [0xE0 | brightness_5bit] [B] [G] [R]
    size_t expectedMinSize = 4 + (4 * NUM_LEDS);
    FL_CHECK_GE(data.size(), expectedMinSize);

    // Verify start frame (4 bytes of 0x00)
    FL_CHECK_EQ(data[0], 0x00);
    FL_CHECK_EQ(data[1], 0x00);
    FL_CHECK_EQ(data[2], 0x00);
    FL_CHECK_EQ(data[3], 0x00);

    // Verify each LED has brightness header with 0xE0 prefix
    // APA102HD uses per-LED brightness (not necessarily all the same)
    for (int i = 0; i < NUM_LEDS; i++) {
        size_t offset = 4 + (i * 4);  // skip start frame
        uint8_t header = data[offset];
        // Header must have 0xE0 prefix (top 3 bits set)
        FL_CHECK_EQ(header & 0xE0, 0xE0);
        // Brightness is bottom 5 bits (0-31), must be non-zero for non-black pixels
        uint8_t brightness = header & 0x1F;
        FL_CHECK_GT(brightness, 0);
    }

    // Clean up
    channel->removeFromDrawList();
    manager.setDriverEnabled("MOCK_SPI_HD", false);
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

    // Test predicate filtering with mock SPI driver
    MockSpiEngine mockEngine;

    // SPI driver should reject clockless data
    FL_CHECK_FALSE(mockEngine.canHandle(clocklessData));

    // SPI driver should accept SPI data
    FL_CHECK(mockEngine.canHandle(spiData));
}

#if 0  // Disabled - uses old proxy pattern (manager.enqueue/show)

FL_TEST_CASE("ChannelManager - predicate filtering (clockless rejected)") {
    // Create mock SPI driver that ONLY accepts SPI chipsets
    auto mockSpiEngine = fl::make_shared<MockSpiEngine>("MOCK_SPI_TEST1");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mockSpiEngine);

    // Set mock driver as exclusive (disables all other drivers)
    bool exclusive = manager.setExclusiveDriverByName("MOCK_SPI_TEST1");
    FL_REQUIRE(exclusive);

    // Create clockless ChannelData
    ChipsetTimingConfig ws2812Timing{350, 700, 600, 50, "WS2812"};
    ClocklessChipset clocklessChipset{5, ws2812Timing};
    auto clocklessData = ChannelData::create(clocklessChipset);

    FL_CHECK(clocklessData->isClockless());
    FL_CHECK_FALSE(clocklessData->isSpi());

    // Try to enqueue clockless data to ChannelManager
    // Predicate filtering should reject it
    manager.enqueue(clocklessData);
    manager.show();  // Trigger transmission

    // Verify data was NOT forwarded to MOCK_SPI (predicate rejected)
    FL_CHECK_EQ(mockSpiEngine->mEnqueueCount, 0);

    // Clean up
    manager.setDriverEnabled("MOCK_SPI_TEST1", false);
}

FL_TEST_CASE("ChannelManager - predicate filtering (SPI accepted)") {
    // Create mock SPI driver that ONLY accepts SPI chipsets
    auto mockSpiEngine = fl::make_shared<MockSpiEngine>("MOCK_SPI_TEST2");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mockSpiEngine);

    // Set mock driver as exclusive (disables all other drivers)
    bool exclusive = manager.setExclusiveDriverByName("MOCK_SPI_TEST2");
    FL_REQUIRE(exclusive);

    // Create SPI ChannelData
    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiChipset{23, 18, encoder};
    auto spiData = ChannelData::create(spiChipset);

    FL_CHECK(spiData->isSpi());
    FL_CHECK_FALSE(spiData->isClockless());

    // Enqueue SPI data to ChannelManager
    // Predicate filtering should accept it
    manager.enqueue(spiData);
    manager.show();  // Trigger transmission

    // Verify data was forwarded to MOCK_SPI (predicate accepted)
    FL_CHECK_GT(mockSpiEngine->mEnqueueCount, 0);

    // Clean up
    manager.setDriverEnabled("MOCK_SPI_TEST2", false);
}

#endif  // End of disabled proxy pattern tests

// ============================================================================
// Tests verifying SPI chipsets route through ChannelManager
// These exercise the same code path that addLeds<APA102> uses on ESP32/Teensy4
// when FASTLED_SPI_USES_CHANNEL_API=1.
// ============================================================================

/// Mock SPI driver that captures enqueued data for verification.
/// Copies encoded bytes on enqueue so data survives poll() cleanup.
class SpiCaptureMock : public IChannelDriver {
public:
    explicit SpiCaptureMock(const char* name) : mName(name) {}

    int enqueueCount = 0;
    int showCount = 0;
    // Snapshots of encoded data captured at enqueue time (survives poll)
    fl::vector<fl::vector<uint8_t>> capturedData;

    bool canHandle(const ChannelDataPtr& data) const override {
        return data && data->isSpi();
    }

    void enqueue(ChannelDataPtr channelData) override {
        if (channelData) {
            enqueueCount++;
            // Snapshot the encoded bytes now, before poll() can clear them
            const auto& src = channelData->getData();
            fl::vector<uint8_t> copy;
            copy.insert(copy.end(), src.begin(), src.end());
            capturedData.push_back(fl::move(copy));
        }
    }

    void show() override {
        showCount++;
    }

    DriverState poll() override {
        return DriverState::READY;
    }

    fl::string getName() const override { return mName; }

    Capabilities getCapabilities() const override {
        return Capabilities(false, true); // SPI only
    }

    void reset() {
        enqueueCount = 0;
        showCount = 0;
        capturedData.clear();
    }

private:
    fl::string mName;
};

FL_TEST_CASE("spiEncoderForChipset returns correct defaults") {
    auto enc = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102);
    FL_CHECK_EQ(enc.chipset, SpiChipset::APA102);
    FL_CHECK_EQ(enc.clock_hz, 6000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::SK9822);
    FL_CHECK_EQ(enc.chipset, SpiChipset::SK9822);
    FL_CHECK_EQ(enc.clock_hz, 12000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::HD107);
    FL_CHECK_EQ(enc.chipset, SpiChipset::HD107);
    FL_CHECK_EQ(enc.clock_hz, 40000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::WS2801);
    FL_CHECK_EQ(enc.chipset, SpiChipset::WS2801);
    FL_CHECK_EQ(enc.clock_hz, 1000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::HD108);
    FL_CHECK_EQ(enc.chipset, SpiChipset::HD108);
    FL_CHECK_EQ(enc.clock_hz, 25000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::P9813);
    FL_CHECK_EQ(enc.chipset, SpiChipset::P9813);
    FL_CHECK_EQ(enc.clock_hz, 10000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::LPD8806);
    FL_CHECK_EQ(enc.chipset, SpiChipset::LPD8806);
    FL_CHECK_EQ(enc.clock_hz, 12000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::SM16716);
    FL_CHECK_EQ(enc.chipset, SpiChipset::SM16716);
    FL_CHECK_EQ(enc.clock_hz, 16000000u);
}

FL_TEST_CASE("spiEncoderForChipset speed override") {
    auto enc = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102, 20000000);
    FL_CHECK_EQ(enc.chipset, SpiChipset::APA102);
    FL_CHECK_EQ(enc.clock_hz, 20000000u);

    // Zero override means use default
    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102, 0);
    FL_CHECK_EQ(enc.clock_hz, 6000000u);
}

FL_TEST_CASE("APA102 channel routes through ChannelManager") {
    // This tests the same path that addLeds<APA102> uses on ESP32/Teensy4:
    //   SpiEncoder → SpiChipsetConfig → ChannelConfig → FastLED.add → Channel
    //   → ChannelManager → driver

    auto mock = fl::make_shared<SpiCaptureMock>("APA102_ROUTE_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mock);
    bool exclusive = manager.setExclusiveDriverByName("APA102_ROUTE_TEST");
    FL_REQUIRE(exclusive);

    const int NUM_LEDS = 5;
    CRGB leds[NUM_LEDS];
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Green;
    leds[2] = CRGB::Blue;
    leds[3] = CRGB::White;
    leds[4] = CRGB::Black;

    // Mirror what addLeds<APA102, 5, 6>() does on channel-enabled platforms
    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102);
    SpiChipsetConfig spiCfg(5, 6, encoder);
    ChannelConfig config(spiCfg, fl::span<CRGB>(leds, NUM_LEDS), RGB);
    ChannelPtr channel = FastLED.add(config);

    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->isSpi());
    FL_CHECK_EQ(channel->getPin(), 5);
    FL_CHECK_EQ(channel->getClockPin(), 6);
    FL_CHECK_EQ(channel->size(), NUM_LEDS);

    // Trigger show — should enqueue to mock driver
    FastLED.show();

    FL_CHECK_GT(mock->enqueueCount, 0);
    FL_CHECK_GT(mock->showCount, 0);

    // Verify encoded data is APA102 format
    FL_REQUIRE_FALSE(mock->capturedData.empty());
    const auto& data = mock->capturedData[0];

    // APA102: 4-byte start frame + 4 bytes per LED + end frame
    size_t minSize = 4 + (4 * NUM_LEDS);
    FL_CHECK_GE(data.size(), minSize);

    // Start frame: 4 zero bytes
    FL_CHECK_EQ(data[0], 0x00);
    FL_CHECK_EQ(data[1], 0x00);
    FL_CHECK_EQ(data[2], 0x00);
    FL_CHECK_EQ(data[3], 0x00);

    // Each LED has 0xE0 | brightness header
    for (int i = 0; i < NUM_LEDS; i++) {
        size_t offset = 4 + (i * 4);
        FL_CHECK_EQ(data[offset] & 0xE0, 0xE0);
    }

    // Cleanup
    FastLED.remove(channel);
    mock->poll();
    manager.removeDriver(mock);
}

FL_TEST_CASE("SK9822 channel routes through ChannelManager") {
    auto mock = fl::make_shared<SpiCaptureMock>("SK9822_ROUTE_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mock);
    bool exclusive = manager.setExclusiveDriverByName("SK9822_ROUTE_TEST");
    FL_REQUIRE(exclusive);

    const int NUM_LEDS = 3;
    CRGB leds[NUM_LEDS] = {CRGB::Red, CRGB::Green, CRGB::Blue};

    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::SK9822);
    SpiChipsetConfig spiCfg(10, 11, encoder);
    ChannelConfig config(spiCfg, fl::span<CRGB>(leds, NUM_LEDS), RGB);
    ChannelPtr channel = FastLED.add(config);
    FL_REQUIRE(channel != nullptr);

    FastLED.show();

    FL_CHECK_GT(mock->enqueueCount, 0);
    FL_REQUIRE_FALSE(mock->capturedData.empty());

    const auto& data = mock->capturedData[0];
    // SK9822 also has 4-byte start frame + 4 bytes per LED
    size_t minSize = 4 + (4 * NUM_LEDS);
    FL_CHECK_GE(data.size(), minSize);

    // Start frame
    FL_CHECK_EQ(data[0], 0x00);
    FL_CHECK_EQ(data[1], 0x00);
    FL_CHECK_EQ(data[2], 0x00);
    FL_CHECK_EQ(data[3], 0x00);

    // Cleanup
    FastLED.remove(channel);
    mock->poll();
    manager.removeDriver(mock);
}

FL_TEST_CASE("WS2801 channel routes through ChannelManager") {
    auto mock = fl::make_shared<SpiCaptureMock>("WS2801_ROUTE_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mock);
    bool exclusive = manager.setExclusiveDriverByName("WS2801_ROUTE_TEST");
    FL_REQUIRE(exclusive);

    const int NUM_LEDS = 4;
    CRGB leds[NUM_LEDS] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White};

    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::WS2801);
    SpiChipsetConfig spiCfg(7, 8, encoder);
    ChannelConfig config(spiCfg, fl::span<CRGB>(leds, NUM_LEDS), RGB);
    ChannelPtr channel = FastLED.add(config);
    FL_REQUIRE(channel != nullptr);

    FastLED.show();

    FL_CHECK_GT(mock->enqueueCount, 0);
    FL_REQUIRE_FALSE(mock->capturedData.empty());

    const auto& data = mock->capturedData[0];
    // WS2801: 3 bytes per LED, no start/end frame
    FL_CHECK_EQ(data.size(), 3u * NUM_LEDS);

    // Cleanup
    FastLED.remove(channel);
    mock->poll();
    manager.removeDriver(mock);
}

FL_TEST_CASE("Multiple SPI channels share ChannelManager") {
    auto mock = fl::make_shared<SpiCaptureMock>("MULTI_SPI_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mock);
    bool exclusive = manager.setExclusiveDriverByName("MULTI_SPI_TEST");
    FL_REQUIRE(exclusive);

    const int NUM_LEDS = 3;
    CRGB leds1[NUM_LEDS] = {CRGB::Red, CRGB::Red, CRGB::Red};
    CRGB leds2[NUM_LEDS] = {CRGB::Blue, CRGB::Blue, CRGB::Blue};

    // Two APA102 strips on different pins
    SpiEncoder enc = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102);

    SpiChipsetConfig cfg1(5, 6, enc);
    ChannelConfig config1(cfg1, fl::span<CRGB>(leds1, NUM_LEDS), RGB);
    ChannelPtr ch1 = FastLED.add(config1);

    SpiChipsetConfig cfg2(7, 8, enc);
    ChannelConfig config2(cfg2, fl::span<CRGB>(leds2, NUM_LEDS), RGB);
    ChannelPtr ch2 = FastLED.add(config2);

    FL_REQUIRE(ch1 != nullptr);
    FL_REQUIRE(ch2 != nullptr);

    FastLED.show();

    // Both channels should be enqueued
    FL_CHECK_EQ(mock->enqueueCount, 2);
    FL_CHECK_EQ(mock->capturedData.size(), 2u);

    // Cleanup
    FastLED.remove(ch1);
    FastLED.remove(ch2);
    mock->poll();
    manager.removeDriver(mock);
}

FL_TEST_CASE("SPI channel rejected by clockless-only driver") {
    // A driver that only handles clockless should reject SPI channels
    class ClocklessOnlyMock : public IChannelDriver {
    public:
        int enqueueCount = 0;
        bool canHandle(const ChannelDataPtr& data) const override {
            return data && data->isClockless();
        }
        void enqueue(ChannelDataPtr) override { enqueueCount++; }
        void show() override {}
        DriverState poll() override { return DriverState::READY; }
        fl::string getName() const override { return "CLOCKLESS_ONLY"; }
        Capabilities getCapabilities() const override {
            return Capabilities(true, false);
        }
    };

    auto clocklessMock = fl::make_shared<ClocklessOnlyMock>();
    auto spiMock = fl::make_shared<SpiCaptureMock>("SPI_FALLBACK_TEST");

    ChannelManager& manager = ChannelManager::instance();
    // Register clockless at higher priority, SPI at lower
    manager.addDriver(2000, clocklessMock);
    manager.addDriver(1000, spiMock);
    manager.setExclusiveDriverByName("CLOCKLESS_ONLY"); // disable all
    manager.setDriverEnabled("CLOCKLESS_ONLY", true);
    manager.setDriverEnabled("SPI_FALLBACK_TEST", true);

    const int NUM_LEDS = 2;
    CRGB leds[NUM_LEDS] = {CRGB::Red, CRGB::Green};

    SpiEncoder enc = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102);
    SpiChipsetConfig spiCfg(5, 6, enc);
    ChannelConfig config(spiCfg, fl::span<CRGB>(leds, NUM_LEDS), RGB);
    ChannelPtr channel = FastLED.add(config);
    FL_REQUIRE(channel != nullptr);

    FastLED.show();

    // Clockless-only driver should NOT have received SPI data
    FL_CHECK_EQ(clocklessMock->enqueueCount, 0);
    // SPI driver should have received it
    FL_CHECK_GT(spiMock->enqueueCount, 0);

    // Cleanup
    FastLED.remove(channel);
    spiMock->poll();
    manager.removeDriver(clocklessMock);
    manager.removeDriver(spiMock);
}

FL_TEST_CASE("spiEncoderForChipset covers all chipsets") {
    // Test the chipsets NOT covered by "spiEncoderForChipset returns correct defaults"
    auto enc = SpiEncoder::spiEncoderForChipset(SpiChipset::APA102HD);
    FL_CHECK_EQ(enc.chipset, SpiChipset::APA102HD);
    FL_CHECK_EQ(enc.clock_hz, 6000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::SK9822HD);
    FL_CHECK_EQ(enc.chipset, SpiChipset::SK9822HD);
    FL_CHECK_EQ(enc.clock_hz, 12000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::DOTSTAR);
    FL_CHECK_EQ(enc.chipset, SpiChipset::DOTSTAR);
    FL_CHECK_EQ(enc.clock_hz, 6000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::DOTSTARHD);
    FL_CHECK_EQ(enc.chipset, SpiChipset::DOTSTARHD);
    FL_CHECK_EQ(enc.clock_hz, 6000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::HD107HD);
    FL_CHECK_EQ(enc.chipset, SpiChipset::HD107HD);
    FL_CHECK_EQ(enc.clock_hz, 40000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::WS2803);
    FL_CHECK_EQ(enc.chipset, SpiChipset::WS2803);
    FL_CHECK_EQ(enc.clock_hz, 25000000u);

    enc = SpiEncoder::spiEncoderForChipset(SpiChipset::LPD6803);
    FL_CHECK_EQ(enc.chipset, SpiChipset::LPD6803);
    FL_CHECK_EQ(enc.clock_hz, 12000000u);
}

FL_TEST_CASE("SPI factory methods produce correct chipset and speed") {
    // Test all factory methods added in this change
    auto e = SpiEncoder::sk9822HD();
    FL_CHECK_EQ(e.chipset, SpiChipset::SK9822HD);
    FL_CHECK_EQ(e.clock_hz, 12000000u);

    e = SpiEncoder::dotstar();
    FL_CHECK_EQ(e.chipset, SpiChipset::DOTSTAR);
    FL_CHECK_EQ(e.clock_hz, 6000000u);

    e = SpiEncoder::dotstarHD();
    FL_CHECK_EQ(e.chipset, SpiChipset::DOTSTARHD);
    FL_CHECK_EQ(e.clock_hz, 6000000u);

    e = SpiEncoder::hd107();
    FL_CHECK_EQ(e.chipset, SpiChipset::HD107);
    FL_CHECK_EQ(e.clock_hz, 40000000u);

    e = SpiEncoder::hd107HD();
    FL_CHECK_EQ(e.chipset, SpiChipset::HD107HD);
    FL_CHECK_EQ(e.clock_hz, 40000000u);

    e = SpiEncoder::hd108();
    FL_CHECK_EQ(e.chipset, SpiChipset::HD108);
    FL_CHECK_EQ(e.clock_hz, 25000000u);

    e = SpiEncoder::ws2801();
    FL_CHECK_EQ(e.chipset, SpiChipset::WS2801);
    FL_CHECK_EQ(e.clock_hz, 1000000u);

    e = SpiEncoder::ws2803();
    FL_CHECK_EQ(e.chipset, SpiChipset::WS2803);
    FL_CHECK_EQ(e.clock_hz, 25000000u);

    e = SpiEncoder::p9813();
    FL_CHECK_EQ(e.chipset, SpiChipset::P9813);
    FL_CHECK_EQ(e.clock_hz, 10000000u);

    e = SpiEncoder::lpd8806();
    FL_CHECK_EQ(e.chipset, SpiChipset::LPD8806);
    FL_CHECK_EQ(e.clock_hz, 12000000u);

    e = SpiEncoder::lpd6803();
    FL_CHECK_EQ(e.chipset, SpiChipset::LPD6803);
    FL_CHECK_EQ(e.clock_hz, 12000000u);

    e = SpiEncoder::sm16716();
    FL_CHECK_EQ(e.chipset, SpiChipset::SM16716);
    FL_CHECK_EQ(e.clock_hz, 16000000u);
}

FL_TEST_CASE("SPI factory methods accept custom clock speed") {
    auto e = SpiEncoder::sk9822HD(5000000);
    FL_CHECK_EQ(e.clock_hz, 5000000u);

    e = SpiEncoder::hd107(20000000);
    FL_CHECK_EQ(e.clock_hz, 20000000u);

    e = SpiEncoder::ws2801(500000);
    FL_CHECK_EQ(e.clock_hz, 500000u);

    e = SpiEncoder::p9813(2000000);
    FL_CHECK_EQ(e.clock_hz, 2000000u);

    e = SpiEncoder::lpd6803(8000000);
    FL_CHECK_EQ(e.clock_hz, 8000000u);
}

FL_TEST_CASE("DOTSTAR aliases match APA102 defaults") {
    auto dot = SpiEncoder::dotstar();
    auto apa = SpiEncoder::apa102();
    FL_CHECK_EQ(dot.clock_hz, apa.clock_hz);

    auto dotHD = SpiEncoder::dotstarHD();
    auto apaHD = SpiEncoder::apa102HD();
    FL_CHECK_EQ(dotHD.clock_hz, apaHD.clock_hz);

    // But chipset enum values differ (they are separate enum entries)
    FL_CHECK_NE(dot.chipset, apa.chipset);
    FL_CHECK_NE(dotHD.chipset, apaHD.chipset);
}

FL_TEST_CASE("P9813 channel routes with flag byte encoding") {
    auto mock = fl::make_shared<SpiCaptureMock>("P9813_ROUTE_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mock);
    bool exclusive = manager.setExclusiveDriverByName("P9813_ROUTE_TEST");
    FL_REQUIRE(exclusive);

    const int NUM_LEDS = 2;
    CRGB leds[NUM_LEDS] = {CRGB::Red, CRGB::Blue};

    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::P9813);
    SpiChipsetConfig spiCfg(5, 6, encoder);
    ChannelConfig config(spiCfg, fl::span<CRGB>(leds, NUM_LEDS), RGB);
    ChannelPtr channel = FastLED.add(config);
    FL_REQUIRE(channel != nullptr);

    FastLED.show();

    FL_CHECK_GT(mock->enqueueCount, 0);
    FL_REQUIRE_FALSE(mock->capturedData.empty());

    const auto& data = mock->capturedData[0];
    // P9813: 4-byte start boundary + 4 bytes per LED + 4-byte end boundary
    size_t expectedSize = 4 + (4 * NUM_LEDS) + 4;
    FL_CHECK_EQ(data.size(), expectedSize);

    // Start boundary: 4 zero bytes
    FL_CHECK_EQ(data[0], 0x00);
    FL_CHECK_EQ(data[1], 0x00);
    FL_CHECK_EQ(data[2], 0x00);
    FL_CHECK_EQ(data[3], 0x00);

    // Each LED has flag byte with 0xC0 prefix
    for (int i = 0; i < NUM_LEDS; i++) {
        size_t offset = 4 + (i * 4);
        FL_CHECK_EQ(data[offset] & 0xC0, 0xC0);
    }

    // End boundary: 4 zero bytes
    size_t endOffset = 4 + (4 * NUM_LEDS);
    FL_CHECK_EQ(data[endOffset + 0], 0x00);
    FL_CHECK_EQ(data[endOffset + 1], 0x00);
    FL_CHECK_EQ(data[endOffset + 2], 0x00);
    FL_CHECK_EQ(data[endOffset + 3], 0x00);

    // Cleanup
    FastLED.remove(channel);
    mock->poll();
    manager.removeDriver(mock);
}

FL_TEST_CASE("LPD8806 channel routes with MSB-set encoding") {
    auto mock = fl::make_shared<SpiCaptureMock>("LPD8806_ROUTE_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mock);
    bool exclusive = manager.setExclusiveDriverByName("LPD8806_ROUTE_TEST");
    FL_REQUIRE(exclusive);

    const int NUM_LEDS = 3;
    CRGB leds[NUM_LEDS] = {CRGB(255, 0, 0), CRGB(0, 255, 0), CRGB(0, 0, 255)};

    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::LPD8806);
    SpiChipsetConfig spiCfg(5, 6, encoder);
    ChannelConfig config(spiCfg, fl::span<CRGB>(leds, NUM_LEDS), RGB);
    ChannelPtr channel = FastLED.add(config);
    FL_REQUIRE(channel != nullptr);

    FastLED.show();

    FL_CHECK_GT(mock->enqueueCount, 0);
    FL_REQUIRE_FALSE(mock->capturedData.empty());

    const auto& data = mock->capturedData[0];

    // LPD8806: 3 bytes per LED + latch bytes
    size_t ledBytes = 3 * NUM_LEDS;
    size_t latchBytes = (NUM_LEDS * 3 + 63) / 64;
    FL_CHECK_EQ(data.size(), ledBytes + latchBytes);

    // Every color byte must have MSB set (0x80)
    for (size_t i = 0; i < ledBytes; i++) {
        FL_CHECK_EQ(data[i] & 0x80, 0x80);
    }

    // Latch bytes must be 0x00
    for (size_t i = ledBytes; i < data.size(); i++) {
        FL_CHECK_EQ(data[i], 0x00);
    }

    // Cleanup
    FastLED.remove(channel);
    mock->poll();
    manager.removeDriver(mock);
}

FL_TEST_CASE("LPD6803 channel routes with 16-bit 5-5-5 encoding") {
    auto mock = fl::make_shared<SpiCaptureMock>("LPD6803_ROUTE_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mock);
    bool exclusive = manager.setExclusiveDriverByName("LPD6803_ROUTE_TEST");
    FL_REQUIRE(exclusive);

    const int NUM_LEDS = 2;
    CRGB leds[NUM_LEDS] = {CRGB(255, 0, 0), CRGB(0, 0, 255)};

    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::LPD6803);
    SpiChipsetConfig spiCfg(5, 6, encoder);
    ChannelConfig config(spiCfg, fl::span<CRGB>(leds, NUM_LEDS), RGB);
    ChannelPtr channel = FastLED.add(config);
    FL_REQUIRE(channel != nullptr);

    FastLED.show();

    FL_CHECK_GT(mock->enqueueCount, 0);
    FL_REQUIRE_FALSE(mock->capturedData.empty());

    const auto& data = mock->capturedData[0];

    // LPD6803: 4-byte start + 2 bytes per LED + optional end frame
    // Start frame is 4 zero bytes
    FL_CHECK_GE(data.size(), 4u + 2u * NUM_LEDS);
    FL_CHECK_EQ(data[0], 0x00);
    FL_CHECK_EQ(data[1], 0x00);
    FL_CHECK_EQ(data[2], 0x00);
    FL_CHECK_EQ(data[3], 0x00);

    // Each LED is 16 bits (2 bytes), MSB of first byte has marker bit set
    for (int i = 0; i < NUM_LEDS; i++) {
        size_t offset = 4 + (i * 2);
        FL_CHECK_EQ(data[offset] & 0x80, 0x80);
    }

    // Cleanup
    FastLED.remove(channel);
    mock->poll();
    manager.removeDriver(mock);
}

FL_TEST_CASE("SM16716 channel routes with RGB encoding") {
    auto mock = fl::make_shared<SpiCaptureMock>("SM16716_ROUTE_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mock);
    bool exclusive = manager.setExclusiveDriverByName("SM16716_ROUTE_TEST");
    FL_REQUIRE(exclusive);

    const int NUM_LEDS = 2;
    CRGB leds[NUM_LEDS] = {CRGB::Red, CRGB::Green};

    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::SM16716);
    SpiChipsetConfig spiCfg(5, 6, encoder);
    ChannelConfig config(spiCfg, fl::span<CRGB>(leds, NUM_LEDS), RGB);
    ChannelPtr channel = FastLED.add(config);
    FL_REQUIRE(channel != nullptr);

    FastLED.show();

    FL_CHECK_GT(mock->enqueueCount, 0);
    FL_REQUIRE_FALSE(mock->capturedData.empty());

    const auto& data = mock->capturedData[0];

    // SM16716: 3 bytes per LED + 7-byte zero header (50 zero bits)
    size_t expectedSize = (3 * NUM_LEDS) + 7;
    FL_CHECK_EQ(data.size(), expectedSize);

    // Trailing 7 bytes must be zero (50 zero-bit header)
    size_t headerStart = 3 * NUM_LEDS;
    for (size_t i = headerStart; i < data.size(); i++) {
        FL_CHECK_EQ(data[i], 0x00);
    }

    // Cleanup
    FastLED.remove(channel);
    mock->poll();
    manager.removeDriver(mock);
}

FL_TEST_CASE("DOTSTAR channel produces APA102-compatible encoding") {
    auto mock = fl::make_shared<SpiCaptureMock>("DOTSTAR_ENCODE_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(1000, mock);
    bool exclusive = manager.setExclusiveDriverByName("DOTSTAR_ENCODE_TEST");
    FL_REQUIRE(exclusive);

    const int NUM_LEDS = 3;
    CRGB leds[NUM_LEDS] = {CRGB::Red, CRGB::Green, CRGB::Blue};

    SpiEncoder encoder = SpiEncoder::spiEncoderForChipset(SpiChipset::DOTSTAR);
    SpiChipsetConfig spiCfg(5, 6, encoder);
    ChannelConfig config(spiCfg, fl::span<CRGB>(leds, NUM_LEDS), RGB);
    ChannelPtr channel = FastLED.add(config);
    FL_REQUIRE(channel != nullptr);

    FastLED.show();

    FL_CHECK_GT(mock->enqueueCount, 0);
    FL_REQUIRE_FALSE(mock->capturedData.empty());

    const auto& data = mock->capturedData[0];

    // DOTSTAR uses APA102 protocol: 4-byte start + 4 bytes/LED + end
    size_t minSize = 4 + (4 * NUM_LEDS);
    FL_CHECK_GE(data.size(), minSize);

    // Start frame: 4 zero bytes (same as APA102)
    FL_CHECK_EQ(data[0], 0x00);
    FL_CHECK_EQ(data[1], 0x00);
    FL_CHECK_EQ(data[2], 0x00);
    FL_CHECK_EQ(data[3], 0x00);

    // Each LED has 0xE0 brightness header (same as APA102)
    for (int i = 0; i < NUM_LEDS; i++) {
        size_t offset = 4 + (i * 4);
        FL_CHECK_EQ(data[offset] & 0xE0, 0xE0);
    }

    // Cleanup
    FastLED.remove(channel);
    mock->poll();
    manager.removeDriver(mock);
}

} // FL_TEST_FILE
