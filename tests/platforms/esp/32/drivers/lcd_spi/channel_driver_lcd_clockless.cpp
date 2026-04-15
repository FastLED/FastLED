/// @file channel_driver_lcd_clockless.cpp
/// @brief Unit tests for LCD_CAM clockless channel driver

#ifdef FASTLED_STUB_IMPL

#include "test.h"
#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_clockless.h"
#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_mock.h"
#include "fl/channels/data.h"
#include "fl/channels/config.h"
#include "fl/channels/wave3.h"
#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/vector.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::detail;

namespace {

void resetMockState() {
    auto &mock = LcdSpiPeripheralMock::instance();
    mock.reset();
}

fl::shared_ptr<ILcdSpiPeripheral> createMockPeripheral() {
    class MockWrapper : public ILcdSpiPeripheral {
      public:
        bool initialize(const LcdSpiConfig &config) FL_NOEXCEPT override {
            return LcdSpiPeripheralMock::instance().initialize(config);
        }
        void deinitialize() FL_NOEXCEPT override {
            LcdSpiPeripheralMock::instance().deinitialize();
        }
        bool isInitialized() const FL_NOEXCEPT override {
            return LcdSpiPeripheralMock::instance().isInitialized();
        }
        u16 *allocateBuffer(size_t size_bytes) FL_NOEXCEPT override {
            return LcdSpiPeripheralMock::instance().allocateBuffer(size_bytes);
        }
        void freeBuffer(u16 *buffer) FL_NOEXCEPT override {
            LcdSpiPeripheralMock::instance().freeBuffer(buffer);
        }
        bool transmit(const u16 *buffer,
                      size_t size_bytes) FL_NOEXCEPT override {
            return LcdSpiPeripheralMock::instance().transmit(buffer,
                                                             size_bytes);
        }
        bool waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT override {
            return LcdSpiPeripheralMock::instance().waitTransmitDone(
                timeout_ms);
        }
        bool isBusy() const FL_NOEXCEPT override {
            return LcdSpiPeripheralMock::instance().isBusy();
        }
        bool registerTransmitCallback(void *callback,
                                      void *user_ctx) FL_NOEXCEPT override {
            return LcdSpiPeripheralMock::instance().registerTransmitCallback(
                callback, user_ctx);
        }
        const LcdSpiConfig &getConfig() const FL_NOEXCEPT override {
            return LcdSpiPeripheralMock::instance().getConfig();
        }
        u64 getMicroseconds() FL_NOEXCEPT override {
            return LcdSpiPeripheralMock::instance().getMicroseconds();
        }
        void delay(u32 ms) FL_NOEXCEPT override {
            LcdSpiPeripheralMock::instance().delay(ms);
        }
    };

    return fl::make_shared<MockWrapper>();
}

ChannelDataPtr createClocklessChannelData(int pin, size_t numLeds) {
    // WS2812 timing (eligible for wave3)
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    fl::vector_psram<u8> data;
    data.resize(numLeds * 3); // RGB
    for (size_t i = 0; i < numLeds * 3; i++) {
        data[i] = static_cast<u8>(i % 256);
    }
    return ChannelData::create(pin, timing, fl::move(data));
}

} // anonymous namespace

//=============================================================================
// Creation
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdClockless - creation") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    FL_CHECK(driver.getName() == "LCD_CLOCKLESS");
}

FL_TEST_CASE("ChannelDriverLcdClockless - initial state is READY") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// canHandle
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdClockless - canHandle accepts clockless") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);

    auto data = createClocklessChannelData(5, 10);
    FL_CHECK(driver.canHandle(data));
}

FL_TEST_CASE("ChannelDriverLcdClockless - canHandle rejects SPI") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiCfg{5, 18, encoder};
    fl::vector_psram<u8> d;
    d.resize(48);
    auto spiData = ChannelData::create(spiCfg, fl::move(d));
    FL_CHECK_FALSE(driver.canHandle(spiData));
}

FL_TEST_CASE("ChannelDriverLcdClockless - canHandle rejects null") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    FL_CHECK_FALSE(driver.canHandle(nullptr));
}

//=============================================================================
// Wave3 auto-selection (WS2812 is wave3-eligible)
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdClockless - WS2812 selects wave3") {
    // Verify WS2812 timing is wave3-eligible
    ChipsetTiming ws2812;
    ws2812.T1 = 400; ws2812.T2 = 450; ws2812.T3 = 450;
    FL_CHECK(canUseWave3(ws2812));
}

//=============================================================================
// Single chunk transmission
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdClockless - single chunk") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    // 1 LED = 3 bytes source → fits in one chunk (default 90 bytes)
    auto data = createClocklessChannelData(5, 1);
    driver.enqueue(data);
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);
    FL_CHECK(mock.getTransmitCount() == 1);

    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK(mock.getTransmitCount() == 1);
}

//=============================================================================
// Multi-chunk streaming
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdClockless - multi-chunk") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    // Force 3 bytes per chunk (1 LED per chunk)
    driver.setChunkInputBytesForTest(3);

    // 3 LEDs = 9 bytes → 3 chunks
    auto data = createClocklessChannelData(5, 3);
    driver.enqueue(data);
    driver.show();

    FL_CHECK(mock.getTransmitCount() == 1);

    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK(mock.getTransmitCount() == 3);
}

//=============================================================================
// Multi-lane
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdClockless - two lanes") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    auto ch0 = createClocklessChannelData(5, 2);
    auto ch1 = createClocklessChannelData(6, 2);
    driver.enqueue(ch0);
    driver.enqueue(ch1);
    driver.show();

    FL_CHECK(mock.getTransmitCount() == 1);
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// Multi-chunk with ring wrap
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdClockless - ring wrap") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    // 1 byte per chunk, 6 bytes source → 6 chunks (wraps 3-slot ring twice)
    driver.setChunkInputBytesForTest(1);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    fl::vector_psram<u8> d;
    d.resize(6);
    for (size_t i = 0; i < 6; i++) d[i] = 0xFF;
    auto data = ChannelData::create(5, timing, fl::move(d));
    driver.enqueue(data);
    driver.show();
    mock.simulateTransmitComplete();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK(mock.getTransmitCount() == 6);
}

//=============================================================================
// Multi-cycle reuse
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdClockless - multi-cycle reuse") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    driver.setChunkInputBytesForTest(3);

    for (int cycle = 0; cycle < 3; cycle++) {
        auto data = createClocklessChannelData(5, 2); // 6 bytes → 2 chunks
        driver.enqueue(data);
        driver.show();
        mock.simulateTransmitComplete();
        FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    }

    // 3 cycles × 2 chunks = 6
    FL_CHECK(mock.getTransmitCount() == 6);
}

//=============================================================================
// Error handling
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdClockless - transmit failure") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    mock.setTransmitFailure(true);

    auto data = createClocklessChannelData(5, 5);
    driver.enqueue(data);
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("ChannelDriverLcdClockless - error mid-stream") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    driver.setChunkInputBytesForTest(3);

    auto data = createClocklessChannelData(5, 3); // 9 bytes → 3 chunks
    driver.enqueue(data);
    driver.show();

    FL_CHECK(mock.getTransmitCount() == 1);

    mock.setTransmitFailure(true);
    mock.simulateTransmitComplete();

    // ISR tried chunk 2 but transmit failed → abort
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// Empty enqueue
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdClockless - empty enqueue") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    driver.show();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK(mock.getTransmitCount() == 0);
}

//=============================================================================
// DMA output size verification
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdClockless - wave3 DMA output size") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    // 1 LED = 3 source bytes.
    // Wave3: 3 bytes × 48 output bytes/input byte = 144 DMA bytes
    auto data = createClocklessChannelData(5, 1);
    driver.enqueue(data);
    driver.show();

    const auto &history = mock.getTransmitHistory();
    FL_REQUIRE(history.size() == 1);
    FL_CHECK(history[0].size_bytes == 3 * 16 * sizeof(Wave3Byte));
}

#endif // FASTLED_STUB_IMPL

} // FL_TEST_FILE
