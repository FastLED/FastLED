/// @file test_channel_engine_spi_routing.cpp
/// @brief Tests for ChannelEngineSpi chipset routing logic
///
/// Tests the canHandle() predicate in ChannelEngineSpi to ensure correct routing:
/// - Accept clockless chipsets (WS2812, SK6812, etc.)
/// - Reject true SPI chipsets (APA102, SK9822, etc.)
///
/// This is critical for preventing routing conflicts with SpiChannelEngineAdapter.

#include "test.h"
#include "FastLED.h"
#include "fl/channels/data.h"
#include "fl/channels/config.h"
#include "fl/chipsets/spi.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/vector.h"

#ifdef ESP32
#include "platforms/esp/32/drivers/spi/channel_driver_spi.h"
#endif

using namespace fl;

#ifdef ESP32

namespace {

/// @brief Create SPI channel data (APA102, SK9822, etc.)
ChannelDataPtr createSpiChannelData(int dataPin = 5, int clockPin = 18) {
    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiConfig{dataPin, clockPin, encoder};
    fl::vector_psram<uint8_t> data = {0x00, 0xFF, 0xAA, 0x55};
    return ChannelData::create(spiConfig, fl::move(data));
}

/// @brief Create clockless channel data (WS2812, SK6812, etc.)
ChannelDataPtr createClocklessChannelData(int pin = 5) {
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    fl::vector_psram<uint8_t> data = {0xFF, 0x00, 0xAA};
    return ChannelData::create(pin, timing, fl::move(data));
}

} // anonymous namespace

FL_TEST_CASE("ChannelEngineSpi - canHandle accepts WS2812 (clockless)") {
    ChannelEngineSpi driver;

    auto data = createClocklessChannelData(5);
    CHECK_TRUE(driver.canHandle(data));
}

FL_TEST_CASE("ChannelEngineSpi - canHandle accepts SK6812 (clockless)") {
    ChannelEngineSpi driver;

    auto timing = makeTimingConfig<TIMING_SK6812>();
    fl::vector_psram<uint8_t> channelData = {0xFF, 0x00, 0xAA};
    auto data = ChannelData::create(5, timing, fl::move(channelData));

    CHECK_TRUE(driver.canHandle(data));
}

FL_TEST_CASE("ChannelEngineSpi - canHandle rejects APA102 (true SPI)") {
    ChannelEngineSpi driver;

    auto data = createSpiChannelData(5, 18);
    FL_CHECK_FALSE(driver.canHandle(data));
}

FL_TEST_CASE("ChannelEngineSpi - canHandle rejects SK9822 (true SPI)") {
    ChannelEngineSpi driver;

    SpiEncoder encoder = SpiEncoder::sk9822();
    SpiChipsetConfig spiConfig{5, 18, encoder};
    fl::vector_psram<uint8_t> channelData = {0x00, 0xFF};
    auto data = ChannelData::create(spiConfig, fl::move(channelData));

    FL_CHECK_FALSE(driver.canHandle(data));
}

FL_TEST_CASE("ChannelEngineSpi - canHandle rejects null channel data") {
    ChannelEngineSpi driver;

    FL_CHECK_FALSE(driver.canHandle(nullptr));
}

FL_TEST_CASE("ChannelEngineSpi - Routing architecture validation") {
    // This test validates the critical routing distinction:
    //
    // ChannelEngineSpi (this driver):
    //   - Implements CLOCKLESS protocols (WS2812, SK6812) using SPI hardware
    //   - Uses SPI clock internally for timing, NOT connected to LEDs
    //   - Accept: !data->isSpi() (clockless chipsets)
    //   - Reject: data->isSpi() (true SPI chipsets)
    //
    // SpiChannelEngineAdapter (hardware SPI):
    //   - Implements TRUE SPI protocols (APA102, SK9822)
    //   - Uses SPI clock physically connected to LEDs
    //   - Accept: data->isSpi() (true SPI chipsets)
    //   - Reject: !data->isSpi() (clockless chipsets)
    //
    // Correct routing:
    //   APA102 → SpiChannelEngineAdapter (priority 5-9)
    //   WS2812 → ChannelEngineSpi (priority 2)

    ChannelEngineSpi driver;

    // Clockless data should be accepted
    auto clocklessData = createClocklessChannelData(5);
    CHECK_TRUE(driver.canHandle(clocklessData));
    FL_CHECK_FALSE(clocklessData->isSpi());

    // True SPI data should be rejected
    auto spiData = createSpiChannelData(5, 18);
    FL_CHECK_FALSE(driver.canHandle(spiData));
    CHECK_TRUE(spiData->isSpi());
}

#endif // ESP32
