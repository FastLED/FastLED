/// @file channel_engine_rp_pio.cpp
/// @brief Host lifecycle tests for RP PIO FLEX_IO TX.

#include "test.h"

#include "fl/channels/data.h"
#include "fl/channels/manager.h"
#include "fl/chipsets/spi.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/arm/rp/rpcommon/channel_engine_rp_pio.h"
#include "platforms/arm/rp/rpcommon/rp_pio_peripheral_mock.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

ChannelDataPtr makeChannel(int pin, const fl::vector_psram<u8>& bytes) {
    fl::vector_psram<u8> copy = bytes;
    return ChannelData::create(pin, makeTimingConfig<TIMING_WS2812_800KHZ>(),
                               fl::move(copy));
}

ChannelDataPtr makeSpiChannel(int mosi_pin, int sck_pin, u32 clock_hz,
                              const fl::vector_psram<u8>& bytes) {
    fl::vector_psram<u8> copy = bytes;
    return ChannelData::create(SpiChipsetConfig(
                                   mosi_pin, sck_pin, SpiEncoder::apa102(clock_hz)),
                               fl::move(copy));
}

}  // namespace

FL_TEST_CASE("RP PIO TX waits for terminal state after DMA") {
    auto peripheral = fl::make_shared<RpPioTxPeripheralMock>();
    ChannelEngineRpPio engine(peripheral);
    fl::vector_psram<u8> bytes;
    bytes.push_back(0x00);
    bytes.push_back(0xFF);
    bytes.push_back(0xAA);
    bytes.push_back(0x55);
    auto channel = makeChannel(2, bytes);
    engine.enqueue(channel);
    engine.show();
    FL_REQUIRE(channel->isInUse());
    FL_CHECK_EQ(peripheral->wordCount, static_cast<size_t>(32));
    FL_CHECK_EQ(peripheral->firstWord, 0x00000000u);
    FL_REQUIRE_EQ(peripheral->capturedWords.size(), static_cast<size_t>(32));
    FL_CHECK_EQ(peripheral->capturedWords[0], 0x00000000u);
    for (size_t index = 0; index < 8; ++index) {
        FL_CHECK_EQ(peripheral->capturedWords[index], 0u); // 0x00
        FL_CHECK_EQ(peripheral->capturedWords[8 + index], 0x80000000u); // 0xFF
    }
    FL_CHECK_EQ(peripheral->capturedWords[16], 0x80000000u); // 0xAA MSB
    FL_CHECK_EQ(peripheral->capturedWords[17], 0u);
    FL_CHECK_EQ(peripheral->capturedWords[18], 0x80000000u);
    FL_CHECK_EQ(peripheral->capturedWords[19], 0u);
    FL_CHECK_EQ(peripheral->capturedWords[24], 0u); // 0x55 MSB
    FL_CHECK_EQ(peripheral->capturedWords[25], 0x80000000u);
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);
    peripheral->dmaBusy = false;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    FL_CHECK(channel->isInUse());
    peripheral->terminal = true;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    peripheral->timeUs += 1000;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(channel->isInUse());
    FL_CHECK_EQ(peripheral->deinitializeCalls, 1);
}

FL_TEST_CASE("RP PIO TX preserves pending work and cleans up errors") {
    auto peripheral = fl::make_shared<RpPioTxPeripheralMock>();
    ChannelEngineRpPio engine(peripheral);
    fl::vector_psram<u8> firstBytes;
    firstBytes.push_back(0x12);
    fl::vector_psram<u8> secondBytes;
    secondBytes.push_back(0x34);
    auto first = makeChannel(3, firstBytes);
    auto second = makeChannel(4, secondBytes);
    engine.enqueue(first);
    engine.show();
    engine.enqueue(second);
    FL_CHECK_FALSE(second->isInUse());
    peripheral->error = true;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::ERROR);
    FL_CHECK_FALSE(first->isInUse());
    FL_CHECK_FALSE(second->isInUse());
    FL_CHECK_EQ(engine.lastError(), fl::string("RP PIO: peripheral error"));
    FL_CHECK_FALSE(engine.isActive());
    FL_CHECK(peripheral->abortCalls > 0);
    FL_CHECK(peripheral->deinitializeCalls > 0);
}

FL_TEST_CASE("RP PIO TX rejects a failed second queued start") {
    auto peripheral = fl::make_shared<RpPioTxPeripheralMock>();
    ChannelEngineRpPio engine(peripheral);
    fl::vector_psram<u8> bytes;
    bytes.push_back(0x42);
    auto first = makeChannel(5, bytes);
    auto second = makeChannel(7, bytes); // non-consecutive: must serialize
    engine.enqueue(first);
    engine.enqueue(second);
    engine.show();
    peripheral->dmaBusy = false;
    peripheral->terminal = true;
    FL_REQUIRE_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    peripheral->timeUs += 1000;
    peripheral->configureOk = false;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::ERROR);
    FL_CHECK_FALSE(first->isInUse());
    FL_CHECK_FALSE(second->isInUse());
}

FL_TEST_CASE("RP PIO TX batches only equal-length consecutive compatible lanes") {
    auto peripheral = fl::make_shared<RpPioTxPeripheralMock>();
    ChannelEngineRpPio engine(peripheral);
    fl::vector_psram<u8> leftBytes;
    leftBytes.push_back(0x80);
    fl::vector_psram<u8> rightBytes;
    rightBytes.push_back(0x00);
    auto left = makeChannel(10, leftBytes);
    auto right = makeChannel(11, rightBytes);
    engine.enqueue(left);
    engine.enqueue(right);
    engine.show();
    FL_REQUIRE_EQ(peripheral->lastConfig.tx_pin, 10);
    FL_REQUIRE_EQ(peripheral->lastConfig.lane_count, 2);
    FL_REQUIRE_EQ(peripheral->capturedWords.size(), static_cast<size_t>(8));
    // Lane 0 occupies the most-significant emitted bit and lane 1 the next.
    FL_CHECK_EQ(peripheral->capturedWords[0], 0x80000000u);
    for (size_t index = 1; index < peripheral->capturedWords.size(); ++index) {
        FL_CHECK_EQ(peripheral->capturedWords[index], 0u);
    }
    peripheral->dmaBusy = false;
    peripheral->terminal = true;
    FL_REQUIRE_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    peripheral->timeUs += 1000;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(left->isInUse());
    FL_CHECK_FALSE(right->isInUse());
}

FL_TEST_CASE("RP PIO TX selects four and eight lane batches only for full runs") {
    for (u8 lanes = 4; lanes <= 8; lanes = static_cast<u8>(lanes * 2)) {
        auto peripheral = fl::make_shared<RpPioTxPeripheralMock>();
        ChannelEngineRpPio engine(peripheral);
        fl::vector_psram<u8> bytes;
        bytes.push_back(0xFF);
        for (u8 lane = 0; lane < lanes; ++lane) {
            engine.enqueue(makeChannel(12 + lane, bytes));
        }
        engine.show();
        FL_CHECK_EQ(peripheral->lastConfig.lane_count, lanes);
        FL_CHECK_EQ(peripheral->capturedWords.size(), static_cast<size_t>(8));
        FL_CHECK_EQ(peripheral->capturedWords[0],
                    ((1u << lanes) - 1u) << (32u - lanes));
    }
}

FL_TEST_CASE("RP FLEX_IO PIO SPI sends arbitrary pin pairs") {
    auto clockless = fl::make_shared<RpPioTxPeripheralMock>();
    auto spi = fl::make_shared<RpPioSpiPeripheralMock>();
    ChannelEngineRpPio engine(clockless, spi);
    fl::vector_psram<u8> bytes;
    bytes.push_back(0xA5);
    bytes.push_back(0x5A);
    auto channel = makeSpiChannel(5, 9, 4000000, bytes);
    FL_REQUIRE(engine.canHandle(channel));
    FL_CHECK(engine.getCapabilities().supportsClockless);
    FL_CHECK(engine.getCapabilities().supportsSpi);
    FL_CHECK_EQ(engine.getName(), fl::string("FLEX_IO"));
    engine.enqueue(channel);
    engine.show();
    FL_CHECK(channel->isInUse());
    FL_CHECK_EQ(spi->lastConfig.mosi_pin, 5);
    FL_CHECK_EQ(spi->lastConfig.sck_pin, 9);
    FL_CHECK_EQ(spi->lastConfig.clock_hz, 4000000u);
    FL_REQUIRE_EQ(spi->capturedWords.size(), static_cast<size_t>(2));
    FL_CHECK_EQ(spi->capturedWords[0], 0xA5000000u);
    FL_CHECK_EQ(spi->capturedWords[1], 0x5A000000u);
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);
    spi->dmaBusy = false;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    spi->terminal = true;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(channel->isInUse());
    FL_CHECK_EQ(spi->deinitializeCalls, 1);
}

FL_TEST_CASE("RP PIO instances preserve distinct concrete driver names") {
    const fl::shared_ptr<IRpPioSpiPeripheral> no_spi;
    auto pio0 = fl::make_shared<ChannelEngineRpPio>(
        fl::make_shared<RpPioTxPeripheralMock>(), no_spi, "PIO0");
    auto pio1 = fl::make_shared<ChannelEngineRpPio>(
        fl::make_shared<RpPioTxPeripheralMock>(), no_spi, "PIO1");
    auto pio2 = fl::make_shared<ChannelEngineRpPio>(
        fl::make_shared<RpPioTxPeripheralMock>(), no_spi, "PIO2");

    ChannelManager& manager = ChannelManager::instance();
    manager.clearAllDrivers();
    manager.addDriver(4, pio0);
    manager.addDriver(3, pio1);
    manager.addDriver(2, pio2);

    FL_CHECK_EQ(manager.getDriverCount(), static_cast<fl::size>(3));
    FL_CHECK(manager.getDriverByName("PIO0") != nullptr);
    FL_CHECK(manager.getDriverByName("PIO1") != nullptr);
    FL_CHECK(manager.getDriverByName("PIO2") != nullptr);
    manager.clearAllDrivers();
}

FL_TEST_CASE("RP FLEX_IO defers native SPI pin pairs to the hardware driver") {
    auto clockless = fl::make_shared<RpPioTxPeripheralMock>();
    auto spi = fl::make_shared<RpPioSpiPeripheralMock>();
    ChannelEngineRpPio engine(clockless, spi);
    fl::vector_psram<u8> bytes;
    bytes.push_back(0x01);
    FL_CHECK_FALSE(engine.canHandle(makeSpiChannel(3, 2, 4000000, bytes)));
    FL_CHECK(engine.canHandle(makeSpiChannel(5, 9, 4000000, bytes)));
}

}  // FL_TEST_FILE
