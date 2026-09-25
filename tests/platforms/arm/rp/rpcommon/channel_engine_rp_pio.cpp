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

fl::shared_ptr<IRpPioTxPeripheral> createTxMockPeripheral() {
    class MockWrapper final : public IRpPioTxPeripheral {
      public:
        bool configure(const RpPioTxConfig& config) FL_NO_EXCEPT override {
            return RpPioTxPeripheralMock::instance().configure(config);
        }
        bool startTxDma(const u32* words, size_t word_count) FL_NO_EXCEPT override {
            return RpPioTxPeripheralMock::instance().startTxDma(words, word_count);
        }
        bool isDmaBusy() const FL_NO_EXCEPT override {
            return RpPioTxPeripheralMock::instance().isDmaBusy();
        }
        bool isTerminalComplete() const FL_NO_EXCEPT override {
            return RpPioTxPeripheralMock::instance().isTerminalComplete();
        }
        bool hasError() const FL_NO_EXCEPT override {
            return RpPioTxPeripheralMock::instance().hasError();
        }
        u32 nowMicros() const FL_NO_EXCEPT override {
            return RpPioTxPeripheralMock::instance().nowMicros();
        }
        void abort() FL_NO_EXCEPT override { RpPioTxPeripheralMock::instance().abort(); }
        void deinitialize() FL_NO_EXCEPT override {
            RpPioTxPeripheralMock::instance().deinitialize();
        }
    };
    return fl::make_shared<MockWrapper>();
}

fl::shared_ptr<IRpPioSpiPeripheral> createSpiMockPeripheral() {
    class MockWrapper final : public IRpPioSpiPeripheral {
      public:
        bool configure(const RpPioSpiConfig& config) FL_NO_EXCEPT override {
            return RpPioSpiPeripheralMock::instance().configure(config);
        }
        bool startTxDma(const u32* words, size_t word_count) FL_NO_EXCEPT override {
            return RpPioSpiPeripheralMock::instance().startTxDma(words, word_count);
        }
        bool isDmaBusy() const FL_NO_EXCEPT override {
            return RpPioSpiPeripheralMock::instance().isDmaBusy();
        }
        bool isTerminalComplete() const FL_NO_EXCEPT override {
            return RpPioSpiPeripheralMock::instance().isTerminalComplete();
        }
        bool hasError() const FL_NO_EXCEPT override {
            return RpPioSpiPeripheralMock::instance().hasError();
        }
        u32 nowMicros() const FL_NO_EXCEPT override {
            return RpPioSpiPeripheralMock::instance().nowMicros();
        }
        void abort() FL_NO_EXCEPT override { RpPioSpiPeripheralMock::instance().abort(); }
        void deinitialize() FL_NO_EXCEPT override {
            RpPioSpiPeripheralMock::instance().deinitialize();
        }
    };
    return fl::make_shared<MockWrapper>();
}

RpPioTxPeripheralMock& resetTxMock() {
    RpPioTxPeripheralMock& mock = RpPioTxPeripheralMock::instance();
    mock.reset();
    return mock;
}

RpPioSpiPeripheralMock& resetSpiMock() {
    RpPioSpiPeripheralMock& mock = RpPioSpiPeripheralMock::instance();
    mock.reset();
    return mock;
}

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
    RpPioTxPeripheralMock& peripheral = resetTxMock();
    ChannelEngineRpPio engine(createTxMockPeripheral());
    fl::vector_psram<u8> bytes;
    bytes.push_back(0x00);
    bytes.push_back(0xFF);
    bytes.push_back(0xAA);
    bytes.push_back(0x55);
    auto channel = makeChannel(2, bytes);
    engine.enqueue(channel);
    engine.show();
    FL_REQUIRE(channel->isInUse());
    // Packed single lane (#4621): one DMA byte per data byte, read
    // little-endian by an 8-bit DMA -> 00 FF AA 55 on the wire.
    FL_CHECK(peripheral.lastConfig.packed);
    FL_CHECK_EQ(peripheral.transferCount, static_cast<size_t>(4));
    FL_REQUIRE_EQ(peripheral.capturedWords.size(), static_cast<size_t>(1));
    FL_CHECK_EQ(peripheral.capturedWords[0], 0x55AAFF00u);
    FL_CHECK_EQ(engine.lastWordCount(), static_cast<size_t>(4));
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);
    peripheral.dmaBusy = false;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    FL_CHECK(channel->isInUse());
    peripheral.terminal = true;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    peripheral.timeUs += 1000;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(channel->isInUse());
    FL_CHECK_EQ(peripheral.deinitializeCalls, 1);
}

FL_TEST_CASE("RP PIO TX preserves pending work and cleans up errors") {
    RpPioTxPeripheralMock& peripheral = resetTxMock();
    ChannelEngineRpPio engine(createTxMockPeripheral());
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
    peripheral.error = true;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::ERROR);
    FL_CHECK_FALSE(first->isInUse());
    FL_CHECK_FALSE(second->isInUse());
    FL_CHECK_EQ(engine.lastError(), fl::string("RP PIO: peripheral error"));
    FL_CHECK_FALSE(engine.isActive());
    FL_CHECK(peripheral.abortCalls > 0);
    FL_CHECK(peripheral.deinitializeCalls > 0);
}

FL_TEST_CASE("RP PIO TX rejects a failed second queued start") {
    RpPioTxPeripheralMock& peripheral = resetTxMock();
    ChannelEngineRpPio engine(createTxMockPeripheral());
    fl::vector_psram<u8> bytes;
    bytes.push_back(0x42);
    auto first = makeChannel(5, bytes);
    auto second = makeChannel(7, bytes); // non-consecutive: must serialize
    engine.enqueue(first);
    engine.enqueue(second);
    engine.show();
    peripheral.dmaBusy = false;
    peripheral.terminal = true;
    FL_REQUIRE_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    peripheral.timeUs += 1000;
    peripheral.configureOk = false;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::ERROR);
    FL_CHECK_FALSE(first->isInUse());
    FL_CHECK_FALSE(second->isInUse());
}

FL_TEST_CASE("RP PIO TX batches only equal-length consecutive compatible lanes") {
    RpPioTxPeripheralMock& peripheral = resetTxMock();
    ChannelEngineRpPio engine(createTxMockPeripheral());
    fl::vector_psram<u8> leftBytes;
    leftBytes.push_back(0x80);
    fl::vector_psram<u8> rightBytes;
    rightBytes.push_back(0x00);
    auto left = makeChannel(10, leftBytes);
    auto right = makeChannel(11, rightBytes);
    engine.enqueue(left);
    engine.enqueue(right);
    engine.show();
    FL_REQUIRE_EQ(peripheral.lastConfig.tx_pin, 10);
    FL_REQUIRE_EQ(peripheral.lastConfig.lane_count, 2);
    // One 16-bit transfer per data byte: 8 two-bit planes, MSB plane first,
    // lane 0 in each plane's MSB.
    FL_CHECK_EQ(peripheral.transferCount, static_cast<size_t>(1));
    FL_REQUIRE_EQ(peripheral.capturedWords.size(), static_cast<size_t>(1));
    FL_CHECK_EQ(peripheral.capturedWords[0], 0x00008000u);
    peripheral.dmaBusy = false;
    peripheral.terminal = true;
    FL_REQUIRE_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    peripheral.timeUs += 1000;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(left->isInUse());
    FL_CHECK_FALSE(right->isInUse());
}

FL_TEST_CASE("RP PIO TX appends XTRA0 zero bits after every byte") {
    RpPioTxPeripheralMock& peripheral = resetTxMock();
    ChannelEngineRpPio engine(createTxMockPeripheral());
    fl::vector_psram<u8> bytes;
    bytes.push_back(0xFF);
    bytes.push_back(0x81);
    auto channel = makeChannel(4, bytes);
    channel->setExtraZeroBitsPerByte(4);  // GE8822 / GW6205
    engine.enqueue(channel);
    engine.show();
    FL_CHECK_FALSE(peripheral.lastConfig.packed);  // XTRA0 breaks byte packing
    FL_REQUIRE_EQ(peripheral.capturedWords.size(), static_cast<size_t>(24));
    for (size_t index = 0; index < 8; ++index) {
        FL_CHECK_EQ(peripheral.capturedWords[index], 0x80000000u);
    }
    for (size_t index = 8; index < 12; ++index) {
        FL_CHECK_EQ(peripheral.capturedWords[index], 0u);
    }
    FL_CHECK_EQ(peripheral.capturedWords[12], 0x80000000u);  // 0x81 MSB
    for (size_t index = 13; index < 19; ++index) {
        FL_CHECK_EQ(peripheral.capturedWords[index], 0u);
    }
    FL_CHECK_EQ(peripheral.capturedWords[19], 0x80000000u);  // 0x81 LSB
    for (size_t index = 20; index < 24; ++index) {
        FL_CHECK_EQ(peripheral.capturedWords[index], 0u);
    }
}

FL_TEST_CASE("RP PIO TX does not batch lanes with different XTRA0") {
    RpPioTxPeripheralMock& peripheral = resetTxMock();
    ChannelEngineRpPio engine(createTxMockPeripheral());
    fl::vector_psram<u8> bytes;
    bytes.push_back(0x80);
    auto left = makeChannel(10, bytes);
    auto right = makeChannel(11, bytes);
    right->setExtraZeroBitsPerByte(4);
    engine.enqueue(left);
    engine.enqueue(right);
    engine.show();
    FL_CHECK_EQ(peripheral.lastConfig.lane_count, 1);
    FL_CHECK_EQ(peripheral.transferCount, static_cast<size_t>(1));
}

FL_TEST_CASE("RP PIO TX runs independent strips concurrently") {
    RpPioTxPeripheralMock& primary = resetTxMock();
    fl::vector<fl::shared_ptr<RpPioTxPeripheralMock>> extras;
    ChannelEngineRpPio engine(
        createTxMockPeripheral(), fl::shared_ptr<IRpPioSpiPeripheral>(), "PIO0",
        [&extras]() -> fl::shared_ptr<IRpPioTxPeripheral> {
            auto mock = fl::make_shared<RpPioTxPeripheralMock>();
            extras.push_back(mock);
            return mock;
        });
    fl::vector_psram<u8> shortBytes;
    shortBytes.push_back(0xFF);
    fl::vector_psram<u8> longBytes;
    longBytes.push_back(0x00);
    longBytes.push_back(0x00);
    auto first = makeChannel(5, shortBytes);
    auto second = makeChannel(9, longBytes);  // other pin, other length
    engine.enqueue(first);
    engine.enqueue(second);
    engine.show();
    // Both strips are on the wire at once, each on its own peripheral.
    FL_REQUIRE_EQ(extras.size(), static_cast<size_t>(1));
    FL_CHECK_EQ(primary.lastConfig.tx_pin, 5);
    FL_CHECK_EQ(primary.transferCount, static_cast<size_t>(1));
    FL_CHECK_EQ(extras[0]->lastConfig.tx_pin, 9);
    FL_CHECK_EQ(extras[0]->transferCount, static_cast<size_t>(2));
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);

    // The short strip finishes first; the long one keeps the frame alive.
    primary.dmaBusy = false;
    primary.terminal = true;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);  // latch starts
    primary.timeUs += 1000;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);  // second still sending
    FL_CHECK_EQ(primary.deinitializeCalls, 1);  // short strip released its SM
    FL_CHECK(first->isInUse());  // frame not complete yet
    extras[0]->dmaBusy = false;
    extras[0]->terminal = true;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    extras[0]->timeUs += 1000;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(first->isInUse());
    FL_CHECK_FALSE(second->isInUse());
    FL_CHECK_EQ(primary.deinitializeCalls, 1);
    FL_CHECK_EQ(extras[0]->deinitializeCalls, 1);
}

FL_TEST_CASE("RP PIO TX queues strips when no extra PIO resources are free") {
    RpPioTxPeripheralMock& primary = resetTxMock();
    fl::vector<fl::shared_ptr<RpPioTxPeripheralMock>> extras;
    ChannelEngineRpPio engine(
        createTxMockPeripheral(), fl::shared_ptr<IRpPioSpiPeripheral>(), "PIO0",
        [&extras]() -> fl::shared_ptr<IRpPioTxPeripheral> {
            auto mock = fl::make_shared<RpPioTxPeripheralMock>();
            mock->configureOk = false;  // every other SM / DMA is taken
            extras.push_back(mock);
            return mock;
        });
    fl::vector_psram<u8> bytes;
    bytes.push_back(0x42);
    auto first = makeChannel(5, bytes);
    auto second = makeChannel(9, bytes);
    engine.enqueue(first);
    engine.enqueue(second);
    engine.show();
    FL_CHECK_EQ(primary.startCalls, 1);
    FL_CHECK_EQ(primary.lastConfig.tx_pin, 5);
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);
    const int extraConfigures = extras.empty() ? 0 : extras[0]->configureCalls;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);
    // No retry storm while the frame waits for the busy strip.
    FL_CHECK_EQ(extras.empty() ? 0 : extras[0]->configureCalls, extraConfigures);

    primary.dmaBusy = false;
    primary.terminal = true;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    primary.timeUs += 1000;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);
    FL_CHECK_EQ(primary.startCalls, 2);
    FL_CHECK_EQ(primary.lastConfig.tx_pin, 9);
    primary.dmaBusy = false;
    primary.timeUs += 1000;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    primary.timeUs += 1000;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(first->isInUse());
    FL_CHECK_FALSE(second->isInUse());
}

FL_TEST_CASE("RP PIO TX selects four and eight lane batches only for full runs") {
    for (u8 lanes = 4; lanes <= 8; lanes = static_cast<u8>(lanes * 2)) {
        RpPioTxPeripheralMock& peripheral = resetTxMock();
        ChannelEngineRpPio engine(createTxMockPeripheral());
        fl::vector_psram<u8> bytes;
        bytes.push_back(0xFF);
        for (u8 lane = 0; lane < lanes; ++lane) {
            engine.enqueue(makeChannel(12 + lane, bytes));
        }
        engine.show();
        FL_CHECK_EQ(peripheral.lastConfig.lane_count, lanes);
        // 4 lanes: one 32-bit column; 8 lanes: two words, planes 7..4 first.
        const size_t words = lanes == 8 ? 2 : 1;
        FL_CHECK_EQ(peripheral.transferCount, words);
        FL_REQUIRE_EQ(peripheral.capturedWords.size(), words);
        for (size_t index = 0; index < words; ++index) {
            FL_CHECK_EQ(peripheral.capturedWords[index], 0xFFFFFFFFu);
        }
    }
}

FL_TEST_CASE("RP PIO TX packs multi-lane planes MSB-plane and lane-0 first") {
    for (u8 lanes = 4; lanes <= 8; lanes = static_cast<u8>(lanes * 2)) {
        RpPioTxPeripheralMock& peripheral = resetTxMock();
        ChannelEngineRpPio engine(createTxMockPeripheral());
        for (u8 lane = 0; lane < lanes; ++lane) {
            fl::vector_psram<u8> bytes;
            // lane 0: MSB set (first plane, top bit); last lane: LSB set.
            bytes.push_back(lane == 0 ? 0x80 : lane == lanes - 1 ? 0x01 : 0x00);
            engine.enqueue(makeChannel(12 + lane, bytes));
        }
        engine.show();
        if (lanes == 4) {
            FL_REQUIRE_EQ(peripheral.capturedWords.size(), static_cast<size_t>(1));
            FL_CHECK_EQ(peripheral.capturedWords[0], 0x80000001u);
        } else {
            FL_REQUIRE_EQ(peripheral.capturedWords.size(), static_cast<size_t>(2));
            FL_CHECK_EQ(peripheral.capturedWords[0], 0x80000000u);  // planes 7..4
            FL_CHECK_EQ(peripheral.capturedWords[1], 0x00000001u);  // planes 3..0
        }
    }
}

FL_TEST_CASE("RP FLEX_IO PIO SPI sends arbitrary pin pairs") {
    resetTxMock();
    RpPioSpiPeripheralMock& spi = resetSpiMock();
    ChannelEngineRpPio engine(createTxMockPeripheral(), createSpiMockPeripheral());
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
    FL_CHECK_EQ(spi.lastConfig.mosi_pin, 5);
    FL_CHECK_EQ(spi.lastConfig.sck_pin, 9);
    FL_CHECK_EQ(spi.lastConfig.clock_hz, 4000000u);
    FL_REQUIRE_EQ(spi.capturedWords.size(), static_cast<size_t>(2));
    FL_CHECK_EQ(spi.capturedWords[0], 0xA5000000u);
    FL_CHECK_EQ(spi.capturedWords[1], 0x5A000000u);
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);
    spi.dmaBusy = false;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    spi.terminal = true;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(channel->isInUse());
    FL_CHECK_EQ(spi.deinitializeCalls, 1);
}

FL_TEST_CASE("RP PIO instances preserve distinct concrete driver names") {
    resetTxMock();
    const fl::shared_ptr<IRpPioSpiPeripheral> no_spi;
    auto pio0 = fl::make_shared<ChannelEngineRpPio>(
        createTxMockPeripheral(), no_spi, "PIO0");
    auto pio1 = fl::make_shared<ChannelEngineRpPio>(
        createTxMockPeripheral(), no_spi, "PIO1");
    auto pio2 = fl::make_shared<ChannelEngineRpPio>(
        createTxMockPeripheral(), no_spi, "PIO2");

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
    resetTxMock();
    resetSpiMock();
    ChannelEngineRpPio engine(createTxMockPeripheral(), createSpiMockPeripheral());
    fl::vector_psram<u8> bytes;
    bytes.push_back(0x01);
    FL_CHECK_FALSE(engine.canHandle(makeSpiChannel(3, 2, 4000000, bytes)));
    FL_CHECK(engine.canHandle(makeSpiChannel(5, 9, 4000000, bytes)));
}

}  // FL_TEST_FILE
