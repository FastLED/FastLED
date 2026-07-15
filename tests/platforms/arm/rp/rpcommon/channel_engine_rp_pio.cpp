/// @file channel_engine_rp_pio.cpp
/// @brief Host lifecycle tests for RP PIO clockless TX.

#include "test.h"

#include "fl/channels/data.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/arm/rp/rpcommon/channel_engine_rp_pio.h"
#include "platforms/arm/rp/rpcommon/channel_engine_rp_pio.cpp.hpp"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

class PioTxMock final : public IRpPioTxPeripheral {
  public:
    bool configure(const RpPioTxConfig& config) FL_NO_EXCEPT override {
        lastConfig = config;
        ++configureCalls;
        return configureOk;
    }
    bool startTxDma(const u32* words, size_t word_count) FL_NO_EXCEPT override {
        ++startCalls;
        firstWord = word_count == 0 ? 0 : words[0];
        wordCount = word_count;
        capturedWords.clear();
        for (size_t index = 0; index < word_count; ++index) {
            capturedWords.push_back(words[index]);
        }
        dmaBusy = startOk;
        return startOk;
    }
    bool isDmaBusy() const FL_NO_EXCEPT override { return dmaBusy; }
    bool isTerminalComplete() const FL_NO_EXCEPT override { return terminal; }
    bool hasError() const FL_NO_EXCEPT override { return error; }
    u32 nowMicros() const FL_NO_EXCEPT override { return timeUs; }
    void abort() FL_NO_EXCEPT override { ++abortCalls; dmaBusy = false; }
    void deinitialize() FL_NO_EXCEPT override { ++deinitializeCalls; }

    RpPioTxConfig lastConfig;
    bool configureOk = true;
    bool startOk = true;
    bool dmaBusy = false;
    bool terminal = false;
    bool error = false;
    int configureCalls = 0;
    int startCalls = 0;
    int abortCalls = 0;
    int deinitializeCalls = 0;
    size_t wordCount = 0;
    u32 firstWord = 0;
    u32 timeUs = 0;
    fl::vector<u32> capturedWords;
};

ChannelDataPtr makeChannel(int pin, const fl::vector_psram<u8>& bytes) {
    fl::vector_psram<u8> copy = bytes;
    return ChannelData::create(pin, makeTimingConfig<TIMING_WS2812_800KHZ>(),
                               fl::move(copy));
}

}  // namespace

FL_TEST_CASE("RP PIO TX waits for terminal state after DMA") {
    auto peripheral = fl::make_shared<PioTxMock>();
    ChannelEngineRpPio engine(peripheral, 0);
    fl::vector_psram<u8> bytes;
    bytes.push_back(0x00);
    bytes.push_back(0xFF);
    bytes.push_back(0xAA);
    bytes.push_back(0x55);
    auto channel = makeChannel(2, bytes);
    engine.enqueue(channel);
    engine.show();
    FL_REQUIRE(channel->isInUse());
    FL_CHECK_EQ(peripheral->wordCount, static_cast<size_t>(4));
    FL_CHECK_EQ(peripheral->firstWord, 0x00000000u);
    FL_REQUIRE_EQ(peripheral->capturedWords.size(), static_cast<size_t>(4));
    FL_CHECK_EQ(peripheral->capturedWords[0], 0x00000000u);
    FL_CHECK_EQ(peripheral->capturedWords[1], 0xFF000000u);
    FL_CHECK_EQ(peripheral->capturedWords[2], 0xAA000000u);
    FL_CHECK_EQ(peripheral->capturedWords[3], 0x55000000u);
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
    auto peripheral = fl::make_shared<PioTxMock>();
    ChannelEngineRpPio engine(peripheral, 1);
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
    FL_CHECK(peripheral->abortCalls > 0);
    FL_CHECK(peripheral->deinitializeCalls > 0);
}

FL_TEST_CASE("RP PIO TX rejects a failed second queued start") {
    auto peripheral = fl::make_shared<PioTxMock>();
    ChannelEngineRpPio engine(peripheral, 0);
    fl::vector_psram<u8> bytes;
    bytes.push_back(0x42);
    auto first = makeChannel(5, bytes);
    auto second = makeChannel(6, bytes);
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

}  // FL_TEST_FILE
