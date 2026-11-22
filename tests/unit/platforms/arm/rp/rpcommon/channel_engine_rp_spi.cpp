/// @file channel_engine_rp_spi.cpp
/// @brief Host lifecycle tests for fixed RP2040 PL022 SPI TX.

#include "test.h"

#include "fl/channels/config.h"
#include "fl/channels/data.h"
#include "fl/chipsets/spi.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/arm/rp/rpcommon/channel_engine_rp_spi.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

class SpiMock final : public IRpSpiPeripheral {
  public:
    bool configure(const RpSpiConfig& config) FL_NO_EXCEPT override {
        lastConfig = config;
        ++configureCalls;
        return configureOk;
    }
    u32 actualClockHz() const FL_NO_EXCEPT override { return actualHz; }
    bool startTxDma(const u8* data, size_t size) FL_NO_EXCEPT override {
        ++startCalls;
        firstByte = size == 0 ? 0 : data[0];
        byteCount = size;
        txBusy = startOk;
        rxBusy = startOk;
        return startOk;
    }
    bool startTxDmaCaptureRx(const u8* data, size_t size, u8* rx_data,
                             size_t rx_size) FL_NO_EXCEPT override {
        ++captureCalls;
        if (rx_data == nullptr || rx_size < size) return false;
        for (size_t index = 0; index < size; ++index) rx_data[index] = data[index];
        return startTxDma(data, size);
    }
    bool isTxDmaBusy() const FL_NO_EXCEPT override { return txBusy; }
    bool isRxDmaBusy() const FL_NO_EXCEPT override { return rxBusy; }
    bool isWireBusy() const FL_NO_EXCEPT override { return wireBusy; }
    bool hasError() const FL_NO_EXCEPT override { return error; }
    u32 nowMicros() const FL_NO_EXCEPT override { return timeUs; }
    void abort() FL_NO_EXCEPT override { ++abortCalls; txBusy = false; rxBusy = false; }
    void deinitialize() FL_NO_EXCEPT override { ++deinitializeCalls; }

    RpSpiConfig lastConfig;
    bool configureOk = true;
    bool startOk = true;
    bool txBusy = false;
    bool rxBusy = false;
    bool wireBusy = false;
    bool error = false;
    u32 actualHz = 6000000;
    u32 timeUs = 0;
    int configureCalls = 0;
    int startCalls = 0;
    int captureCalls = 0;
    int abortCalls = 0;
    int deinitializeCalls = 0;
    size_t byteCount = 0;
    u8 firstByte = 0;
};

ChannelDataPtr makeChannel(int mosi, int sck, u32 clock_hz, u8 first_byte = 0xA5) {
    fl::vector_psram<u8> bytes;
    bytes.push_back(first_byte);
    bytes.push_back(0x5A);
    return ChannelData::create(
        SpiChipsetConfig(mosi, sck, SpiEncoder::apa102(clock_hz)), fl::move(bytes));
}

}  // namespace

FL_TEST_CASE("RP SPI0 DMA waits for both FIFOs and PL022 wire idle") {
    auto peripheral = fl::make_shared<SpiMock>();
    ChannelEngineRpSpi engine(peripheral, 0);
    auto channel = makeChannel(3, 2, 6000000);
    engine.enqueue(channel);
    engine.show();
    FL_REQUIRE(channel->isInUse());
    FL_CHECK_EQ(peripheral->lastConfig.spi_index, 0);
    FL_CHECK_EQ(peripheral->lastConfig.mosi_pin, 3);
    FL_CHECK_EQ(peripheral->lastConfig.miso_pin, 0);
    FL_CHECK_EQ(peripheral->lastConfig.sck_pin, 2);
    FL_CHECK_EQ(peripheral->lastConfig.data_bits, 8);
    FL_CHECK_FALSE(peripheral->lastConfig.cpol);
    FL_CHECK_FALSE(peripheral->lastConfig.cpha);
    FL_CHECK(peripheral->lastConfig.msb_first);
    FL_CHECK_EQ(peripheral->byteCount, static_cast<size_t>(2));
    FL_CHECK_EQ(peripheral->firstByte, 0xA5);
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);
    peripheral->txBusy = false;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);
    peripheral->rxBusy = false;
    peripheral->wireBusy = true;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    peripheral->wireBusy = false;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(channel->isInUse());
    FL_CHECK_EQ(peripheral->deinitializeCalls, 1);
}

FL_TEST_CASE("RP SPI1 accepts only canonical RX SCK MOSI triples and clamps clock") {
    auto peripheral = fl::make_shared<SpiMock>();
    ChannelEngineRpSpi engine(peripheral, 1);
    auto valid = makeChannel(27, 26, 100000000);
    auto mismatched = makeChannel(27, 14, 12000000);
    FL_CHECK(engine.canHandle(valid));
    FL_CHECK_FALSE(engine.canHandle(mismatched));
    engine.enqueue(valid);
    engine.enqueue(mismatched);
    engine.show();
    FL_CHECK_EQ(peripheral->lastConfig.spi_index, 1);
    FL_CHECK_EQ(peripheral->lastConfig.miso_pin, 24);
    FL_CHECK_EQ(peripheral->lastConfig.clock_hz, 62500000u);
    FL_CHECK_EQ(peripheral->startCalls, 1);
}

FL_TEST_CASE("RP SPI captures exact loopback bytes through the channel engine") {
    auto peripheral = fl::make_shared<SpiMock>();
    ChannelEngineRpSpi engine(peripheral, 0);
    u8 captured[2] = {0, 0};
    FL_REQUIRE(engine.captureNextRxBytes(captured, sizeof(captured)));
    engine.enqueue(makeChannel(3, 2, 8000000));
    engine.show();
    FL_CHECK_EQ(peripheral->captureCalls, 1);
    FL_CHECK_EQ(captured[0], 0xA5);
    FL_CHECK_EQ(captured[1], 0x5A);
    FL_CHECK_EQ(engine.lastActualClockHz(), 6000000u);
}

FL_TEST_CASE("RP SPI releases all channels on RX or peripheral failure") {
    auto peripheral = fl::make_shared<SpiMock>();
    ChannelEngineRpSpi engine(peripheral, 0);
    auto first = makeChannel(7, 6, 12000000);
    auto pending = makeChannel(19, 18, 12000000);
    engine.enqueue(first);
    engine.show();
    engine.enqueue(pending);
    peripheral->error = true;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::ERROR);
    FL_CHECK_FALSE(first->isInUse());
    FL_CHECK_FALSE(pending->isInUse());
    FL_CHECK(peripheral->abortCalls > 0);
    FL_CHECK(peripheral->deinitializeCalls > 0);
}

FL_TEST_CASE("RP SPI times out a stalled transfer and releases its ownership") {
    auto peripheral = fl::make_shared<SpiMock>();
    ChannelEngineRpSpi engine(peripheral, 0);
    auto channel = makeChannel(23, 22, 6000000);
    engine.enqueue(channel);
    engine.show();
    peripheral->timeUs = 1000001;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::ERROR);
    FL_CHECK_FALSE(channel->isInUse());
    FL_CHECK(peripheral->abortCalls > 0);
}

}  // FL_TEST_FILE
