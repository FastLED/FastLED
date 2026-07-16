/// @file channel_engine_rp_uart.cpp
/// @brief Host tests for RP UART DMA state and ownership boundaries.

#include "test.h"

#include "fl/channels/data.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"
// The engine implementation is pure C++ against IRpUartPeripheral and is
// compiled into the library on every platform, so the lifecycle contract is
// testable without a Pico SDK installation through the public header alone.
#include "platforms/arm/rp/rpcommon/channel_engine_rp_uart.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

class RpUartPeripheralMock final : public IRpUartPeripheral {
  public:
    bool configure(const RpUartConfig& config) FL_NO_EXCEPT override {
        lastConfig = config;
        achievedBaud = static_cast<u32>(
            static_cast<i32>(config.baud_rate) + baudOffset);
        ++configureCalls;
        return configureOk;
    }
    u32 actualBaudRate() const FL_NO_EXCEPT override { return achievedBaud; }
    bool startTxDma(const u8* data, size_t size) FL_NO_EXCEPT override {
        ++startCalls;
        bytes = size;
        dmaBusy = startOk;
        firstByte = size == 0 ? 0 : data[0];
        return startOk;
    }
    bool isDmaBusy() const FL_NO_EXCEPT override { return dmaBusy; }
    bool isWireBusy() const FL_NO_EXCEPT override { return wireBusy; }
    bool hasError() const FL_NO_EXCEPT override { return error; }
    u32 nowMicros() const FL_NO_EXCEPT override { return timeUs; }
    void abort() FL_NO_EXCEPT override { ++abortCalls; dmaBusy = false; }
    void deinitialize() FL_NO_EXCEPT override { ++deinitializeCalls; }

    RpUartConfig lastConfig;
    bool configureOk = true;
    bool startOk = true;
    bool dmaBusy = false;
    bool wireBusy = false;
    bool error = false;
    int configureCalls = 0;
    int startCalls = 0;
    int abortCalls = 0;
    int deinitializeCalls = 0;
    size_t bytes = 0;
    u8 firstByte = 0;
    u32 timeUs = 0;
    u32 achievedBaud = 0;
    i32 baudOffset = 0;
};

ChannelDataPtr makeWs2812Channel(int pin, u8 value) {
    fl::vector_psram<u8> bytes;
    bytes.push_back(value);
    return ChannelData::create(pin, makeTimingConfig<TIMING_WS2812_800KHZ>(),
                               fl::move(bytes));
}

}  // namespace

FL_TEST_CASE("RP UART accepts only hardware-UART TX pins") {
    auto peripheral = fl::make_shared<RpUartPeripheralMock>();
    ChannelEngineRpUart uart0(peripheral, 0);
    ChannelEngineRpUart uart1(peripheral, 1);
    FL_CHECK(uart0.canHandle(makeWs2812Channel(0, 0)));
    FL_CHECK(uart0.canHandle(makeWs2812Channel(28, 0)));
    FL_CHECK_FALSE(uart0.canHandle(makeWs2812Channel(8, 0)));
    FL_CHECK(uart1.canHandle(makeWs2812Channel(8, 0)));
    FL_CHECK_FALSE(uart1.canHandle(makeWs2812Channel(0, 0)));
}

FL_TEST_CASE("RP UART encoder represents every byte and preserves DMA versus wire drain") {
    auto peripheral = fl::make_shared<RpUartPeripheralMock>();
    ChannelEngineRpUart engine(peripheral, 0);
    for (int value = 0; value < 256; ++value) {
        auto channel = makeWs2812Channel(0, static_cast<u8>(value));
        engine.enqueue(channel);
        FL_CHECK_FALSE(channel->isInUse());
        engine.show();
        FL_REQUIRE(channel->isInUse());
        FL_REQUIRE_EQ(peripheral->lastConfig.uart_index, 0);
        FL_REQUIRE_EQ(peripheral->lastConfig.tx_pin, 0);
        FL_REQUIRE_EQ(peripheral->bytes, static_cast<size_t>(4));
        FL_REQUIRE_EQ(engine.poll(), IChannelDriver::DriverState::BUSY);
        peripheral->dmaBusy = false;
        peripheral->wireBusy = true;
        FL_REQUIRE_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
        FL_REQUIRE(channel->isInUse());
        peripheral->wireBusy = false;
        FL_REQUIRE_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
        peripheral->timeUs += 1000;
        FL_REQUIRE_EQ(engine.poll(), IChannelDriver::DriverState::READY);
        FL_REQUIRE_FALSE(channel->isInUse());
    }
}

FL_TEST_CASE("RP UART releases in-flight channels on peripheral failure") {
    auto peripheral = fl::make_shared<RpUartPeripheralMock>();
    ChannelEngineRpUart engine(peripheral, 1);
    auto channel = makeWs2812Channel(8, 0xAA);
    engine.enqueue(channel);
    engine.show();
    FL_REQUIRE(channel->isInUse());
    peripheral->error = true;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::ERROR);
    FL_CHECK_FALSE(channel->isInUse());
    FL_CHECK(peripheral->abortCalls > 0);
    FL_CHECK(peripheral->deinitializeCalls > 0);
}

FL_TEST_CASE("RP UART keeps a second frame pending until its own show") {
    auto peripheral = fl::make_shared<RpUartPeripheralMock>();
    ChannelEngineRpUart engine(peripheral, 0);
    auto first = makeWs2812Channel(0, 0x55);
    auto second = makeWs2812Channel(0, 0xAA);
    engine.enqueue(first);
    engine.show();
    FL_REQUIRE(first->isInUse());
    engine.enqueue(second);
    FL_REQUIRE_FALSE(second->isInUse());
    peripheral->dmaBusy = false;
    FL_REQUIRE_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    peripheral->timeUs += 1000;
    FL_REQUIRE_EQ(engine.poll(), IChannelDriver::DriverState::READY);
    FL_REQUIRE_FALSE(first->isInUse());
    FL_REQUIRE_FALSE(second->isInUse());
    engine.show();
    FL_CHECK(second->isInUse());
}

FL_TEST_CASE("RP UART start failure clears ownership and reports error on poll") {
    auto peripheral = fl::make_shared<RpUartPeripheralMock>();
    peripheral->startOk = false;
    ChannelEngineRpUart engine(peripheral, 1);
    auto channel = makeWs2812Channel(8, 0xFF);
    engine.enqueue(channel);
    engine.show();
    FL_CHECK(channel->isInUse());
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::ERROR);
    FL_CHECK_FALSE(channel->isInUse());
    FL_CHECK(peripheral->abortCalls > 0);
    FL_CHECK(peripheral->deinitializeCalls > 0);
}

FL_TEST_CASE("RP UART rejects an out-of-margin achieved baud rate") {
    auto peripheral = fl::make_shared<RpUartPeripheralMock>();
    peripheral->baudOffset = -50000;
    ChannelEngineRpUart engine(peripheral, 1);
    auto channel = makeWs2812Channel(8, 0xFF);
    engine.enqueue(channel);
    engine.show();
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::ERROR);
    FL_CHECK_FALSE(channel->isInUse());
}

FL_TEST_CASE("RP UART reports a failed second start after preserving its latch interval") {
    auto peripheral = fl::make_shared<RpUartPeripheralMock>();
    ChannelEngineRpUart engine(peripheral, 0);
    auto first = makeWs2812Channel(0, 0x00);
    auto second = makeWs2812Channel(0, 0xFF);
    engine.enqueue(first);
    engine.enqueue(second);
    engine.show();
    peripheral->dmaBusy = false;
    FL_REQUIRE_EQ(engine.poll(), IChannelDriver::DriverState::DRAINING);
    peripheral->timeUs += 1000;
    peripheral->configureOk = false;
    FL_CHECK_EQ(engine.poll(), IChannelDriver::DriverState::ERROR);
    FL_CHECK_FALSE(first->isInUse());
    FL_CHECK_FALSE(second->isInUse());
}

}  // FL_TEST_FILE
