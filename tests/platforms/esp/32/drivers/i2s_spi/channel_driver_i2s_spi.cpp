/// @file channel_driver_i2s_spi.cpp
/// @brief Unit tests for I2S parallel SPI channel driver

#ifdef FASTLED_STUB_IMPL

#include "test.h"
#include "platforms/esp/32/drivers/i2s_spi/channel_driver_i2s_spi.h"
#include "platforms/esp/32/drivers/i2s_spi/i2s_spi_peripheral_mock.h"
#include "fl/channels/data.h"
#include "fl/channels/config.h"
#include "fl/chipsets/spi.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/vector.h"
#include "fl/stl/thread.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::detail;

namespace {

void resetMockState() {
    auto &mock = I2sSpiPeripheralMock::instance();
    mock.reset();
}

ChannelDataPtr createSpiTestChannelData(int dataPin, int clockPin,
                                        size_t numLeds) {
    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiConfig{dataPin, clockPin, encoder};

    fl::vector_psram<u8> data;
    size_t frameSize = 4 + (numLeds * 4) + 4;
    data.resize(frameSize);

    data[0] = data[1] = data[2] = data[3] = 0x00;
    for (size_t i = 0; i < numLeds; i++) {
        data[4 + i * 4 + 0] = 0xE0 | 31;
        data[4 + i * 4 + 1] = static_cast<u8>(i % 256);
        data[4 + i * 4 + 2] = static_cast<u8>((i * 2) % 256);
        data[4 + i * 4 + 3] = static_cast<u8>((i * 3) % 256);
    }
    size_t endIdx = 4 + numLeds * 4;
    data[endIdx] = data[endIdx + 1] = data[endIdx + 2] = data[endIdx + 3] =
        0xFF;

    return ChannelData::create(spiConfig, fl::move(data));
}

ChannelDataPtr createClocklessTestChannelData(int pin, size_t numLeds) {
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    fl::vector_psram<u8> data;
    data.resize(numLeds * 3);
    for (size_t i = 0; i < numLeds; i++) {
        data[i * 3 + 0] = static_cast<u8>(i % 256);
        data[i * 3 + 1] = static_cast<u8>((i * 2) % 256);
        data[i * 3 + 2] = static_cast<u8>((i * 3) % 256);
    }
    return ChannelData::create(pin, timing, fl::move(data));
}

fl::shared_ptr<II2sSpiPeripheral> createMockPeripheral() {
    class MockWrapper : public II2sSpiPeripheral {
      public:
        bool initialize(const I2sSpiConfig &config) FL_NOEXCEPT override {
            return I2sSpiPeripheralMock::instance().initialize(config);
        }
        void deinitialize() FL_NOEXCEPT override {
            I2sSpiPeripheralMock::instance().deinitialize();
        }
        bool isInitialized() const FL_NOEXCEPT override {
            return I2sSpiPeripheralMock::instance().isInitialized();
        }
        u8 *allocateBuffer(size_t size_bytes) FL_NOEXCEPT override {
            return I2sSpiPeripheralMock::instance().allocateBuffer(size_bytes);
        }
        void freeBuffer(u8 *buffer) FL_NOEXCEPT override {
            I2sSpiPeripheralMock::instance().freeBuffer(buffer);
        }
        bool transmit(const u8 *buffer,
                      size_t size_bytes) FL_NOEXCEPT override {
            return I2sSpiPeripheralMock::instance().transmit(buffer,
                                                            size_bytes);
        }
        bool waitTransmitDone(u32 timeout_ms) FL_NOEXCEPT override {
            return I2sSpiPeripheralMock::instance().waitTransmitDone(
                timeout_ms);
        }
        bool isBusy() const FL_NOEXCEPT override {
            return I2sSpiPeripheralMock::instance().isBusy();
        }
        bool registerTransmitCallback(void *callback,
                                      void *user_ctx) FL_NOEXCEPT override {
            return I2sSpiPeripheralMock::instance().registerTransmitCallback(
                callback, user_ctx);
        }
        const I2sSpiConfig &getConfig() const FL_NOEXCEPT override {
            return I2sSpiPeripheralMock::instance().getConfig();
        }
        u64 getMicroseconds() FL_NOEXCEPT override {
            return I2sSpiPeripheralMock::instance().getMicroseconds();
        }
        void delay(u32 ms) FL_NOEXCEPT override {
            I2sSpiPeripheralMock::instance().delay(ms);
        }
    };

    return fl::make_shared<MockWrapper>();
}

} // anonymous namespace

//=============================================================================
// Creation
//=============================================================================

FL_TEST_CASE("ChannelDriverI2sSpi - creation") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);
    FL_CHECK(driver.getName() == "I2S_SPI");
}

FL_TEST_CASE("ChannelDriverI2sSpi - initial state is READY") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// canHandle
//=============================================================================

FL_TEST_CASE("ChannelDriverI2sSpi - canHandle accepts SPI chipsets") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    auto spiData = createSpiTestChannelData(5, 18, 10);
    FL_CHECK(driver.canHandle(spiData));
}

FL_TEST_CASE("ChannelDriverI2sSpi - canHandle rejects clockless chipsets") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    auto clocklessData = createClocklessTestChannelData(5, 10);
    FL_CHECK_FALSE(driver.canHandle(clocklessData));
}

FL_TEST_CASE("ChannelDriverI2sSpi - canHandle rejects null") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);
    FL_CHECK_FALSE(driver.canHandle(nullptr));
}

//=============================================================================
// Async Lifecycle
//=============================================================================

FL_TEST_CASE("ChannelDriverI2sSpi - show is non-blocking") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    auto channelData = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(channelData);
    driver.show();

    // show() returns immediately — DMA still in progress
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);

    auto &mock = I2sSpiPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() == 1);
    FL_CHECK(mock.isBusy());
}

FL_TEST_CASE("ChannelDriverI2sSpi - poll transitions DRAINING to READY") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    auto channelData = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(channelData);
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);

    // Simulate DMA completion
    auto &mock = I2sSpiPeripheralMock::instance();
    mock.simulateTransmitComplete();
    FL_CHECK_FALSE(mock.isBusy());

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("ChannelDriverI2sSpi - channels released after async completion") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    auto channelData = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(channelData);
    driver.show();

    // While DRAINING, channel is in use
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);

    // Complete DMA and poll to release
    auto &mock = I2sSpiPeripheralMock::instance();
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("ChannelDriverI2sSpi - empty enqueue does not transmit") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    driver.show();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    auto &mock = I2sSpiPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() == 0);
}

//=============================================================================
// Single Channel Transmission
//=============================================================================

FL_TEST_CASE("ChannelDriverI2sSpi - single channel transmission") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    auto channelData = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(channelData);
    driver.show();

    auto &mock = I2sSpiPeripheralMock::instance();
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK(mock.getTransmitCount() == 1);
}

//=============================================================================
// Multi-Channel
//=============================================================================

FL_TEST_CASE("ChannelDriverI2sSpi - multi-channel transmission") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    auto ch1 = createSpiTestChannelData(5, 18, 10);
    auto ch2 = createSpiTestChannelData(6, 18, 10);
    auto ch3 = createSpiTestChannelData(7, 18, 10);

    driver.enqueue(ch1);
    driver.enqueue(ch2);
    driver.enqueue(ch3);
    driver.show();

    auto &mock = I2sSpiPeripheralMock::instance();
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK(mock.getTransmitCount() == 1);

    const auto &history = mock.getTransmitHistory();
    FL_REQUIRE(history.size() == 1);
    // Each channel has 4 + 10*4 + 4 = 48 bytes, interleaved: 48 * 3 = 144
    FL_CHECK(history[0].size_bytes == 48 * 3);
}

//=============================================================================
// Interleave Data Correctness
//=============================================================================

FL_TEST_CASE("ChannelDriverI2sSpi - interleave correctness single lane") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiConfig{5, 18, encoder};

    fl::vector_psram<u8> data;
    data.resize(12);
    data[0] = 0x00; data[1] = 0x00; data[2] = 0x00; data[3] = 0x00;
    data[4] = 0xFF; data[5] = 0xAA; data[6] = 0x55; data[7] = 0xCC;
    data[8] = 0xFF; data[9] = 0xFF; data[10] = 0xFF; data[11] = 0xFF;

    auto channelData = ChannelData::create(spiConfig, fl::move(data));
    driver.enqueue(channelData);
    driver.show();

    auto &mock = I2sSpiPeripheralMock::instance();
    FL_REQUIRE(mock.getTransmitCount() == 1);

    fl::span<const u8> transmitted = mock.getLastTransmitData();
    FL_REQUIRE(transmitted.size() == 12);
    FL_CHECK(transmitted[0] == 0x00);
    FL_CHECK(transmitted[4] == 0xFF);
    FL_CHECK(transmitted[5] == 0xAA);
    FL_CHECK(transmitted[6] == 0x55);
    FL_CHECK(transmitted[7] == 0xCC);
    FL_CHECK(transmitted[8] == 0xFF);
}

FL_TEST_CASE("ChannelDriverI2sSpi - interleave correctness two lanes") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    SpiEncoder encoder = SpiEncoder::apa102();

    SpiChipsetConfig spiCfg0{5, 18, encoder};
    fl::vector_psram<u8> data0;
    data0.resize(4);
    data0[0] = 0x11; data0[1] = 0x22; data0[2] = 0x33; data0[3] = 0x44;
    auto ch0 = ChannelData::create(spiCfg0, fl::move(data0));

    SpiChipsetConfig spiCfg1{6, 18, encoder};
    fl::vector_psram<u8> data1;
    data1.resize(4);
    data1[0] = 0xAA; data1[1] = 0xBB; data1[2] = 0xCC; data1[3] = 0xDD;
    auto ch1 = ChannelData::create(spiCfg1, fl::move(data1));

    driver.enqueue(ch0);
    driver.enqueue(ch1);
    driver.show();

    auto &mock = I2sSpiPeripheralMock::instance();
    FL_REQUIRE(mock.getTransmitCount() == 1);

    fl::span<const u8> tx = mock.getLastTransmitData();
    FL_REQUIRE(tx.size() == 8);
    FL_CHECK(tx[0] == 0x11); FL_CHECK(tx[1] == 0xAA);
    FL_CHECK(tx[2] == 0x22); FL_CHECK(tx[3] == 0xBB);
    FL_CHECK(tx[4] == 0x33); FL_CHECK(tx[5] == 0xCC);
    FL_CHECK(tx[6] == 0x44); FL_CHECK(tx[7] == 0xDD);
}

FL_TEST_CASE("ChannelDriverI2sSpi - interleave unequal channel sizes") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    SpiEncoder encoder = SpiEncoder::apa102();

    SpiChipsetConfig spiCfg0{5, 18, encoder};
    fl::vector_psram<u8> data0;
    data0.resize(4);
    data0[0] = 0xAA; data0[1] = 0xBB; data0[2] = 0xCC; data0[3] = 0xDD;
    auto ch0 = ChannelData::create(spiCfg0, fl::move(data0));

    SpiChipsetConfig spiCfg1{6, 18, encoder};
    fl::vector_psram<u8> data1;
    data1.resize(2);
    data1[0] = 0x11; data1[1] = 0x22;
    auto ch1 = ChannelData::create(spiCfg1, fl::move(data1));

    driver.enqueue(ch0);
    driver.enqueue(ch1);
    driver.show();

    auto &mock = I2sSpiPeripheralMock::instance();
    fl::span<const u8> tx = mock.getLastTransmitData();
    FL_REQUIRE(tx.size() == 8);
    FL_CHECK(tx[0] == 0xAA); FL_CHECK(tx[1] == 0x11);
    FL_CHECK(tx[2] == 0xBB); FL_CHECK(tx[3] == 0x22);
    FL_CHECK(tx[4] == 0xCC); FL_CHECK(tx[5] == 0x00); // padded
    FL_CHECK(tx[6] == 0xDD); FL_CHECK(tx[7] == 0x00); // padded
}

FL_TEST_CASE("ChannelDriverI2sSpi - interleave three lanes known data") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    SpiEncoder encoder = SpiEncoder::apa102();

    fl::vector_psram<u8> d0, d1, d2;
    d0.resize(2); d0[0] = 0x10; d0[1] = 0x20;
    d1.resize(2); d1[0] = 0x30; d1[1] = 0x40;
    d2.resize(2); d2[0] = 0x50; d2[1] = 0x60;

    SpiChipsetConfig cfg0{5, 18, encoder};
    SpiChipsetConfig cfg1{6, 18, encoder};
    SpiChipsetConfig cfg2{7, 18, encoder};

    driver.enqueue(ChannelData::create(cfg0, fl::move(d0)));
    driver.enqueue(ChannelData::create(cfg1, fl::move(d1)));
    driver.enqueue(ChannelData::create(cfg2, fl::move(d2)));
    driver.show();

    auto &mock = I2sSpiPeripheralMock::instance();
    fl::span<const u8> tx = mock.getLastTransmitData();
    FL_REQUIRE(tx.size() == 6);
    FL_CHECK(tx[0] == 0x10); FL_CHECK(tx[1] == 0x30); FL_CHECK(tx[2] == 0x50);
    FL_CHECK(tx[3] == 0x20); FL_CHECK(tx[4] == 0x40); FL_CHECK(tx[5] == 0x60);
}

//=============================================================================
// Re-initialization
//=============================================================================

FL_TEST_CASE("ChannelDriverI2sSpi - re-init on lane count change") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);
    auto &mock = I2sSpiPeripheralMock::instance();

    SpiEncoder encoder = SpiEncoder::apa102();

    // First show: 1 lane
    {
        SpiChipsetConfig cfg{5, 18, encoder};
        fl::vector_psram<u8> d;
        d.resize(4);
        d[0] = 0x01; d[1] = 0x02; d[2] = 0x03; d[3] = 0x04;
        driver.enqueue(ChannelData::create(cfg, fl::move(d)));
        driver.show();
        mock.simulateTransmitComplete();
        FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    }

    FL_CHECK(mock.getTransmitCount() == 1);
    fl::span<const u8> tx1 = mock.getLastTransmitData();
    FL_REQUIRE(tx1.size() == 4);
    FL_CHECK(tx1[0] == 0x01);

    // Second show: 2 lanes (buffer grows, re-init needed)
    {
        SpiChipsetConfig cfg0{5, 18, encoder};
        SpiChipsetConfig cfg1{6, 18, encoder};
        fl::vector_psram<u8> d0, d1;
        d0.resize(4); d0[0] = 0xAA; d0[1] = 0xBB; d0[2] = 0xCC; d0[3] = 0xDD;
        d1.resize(4); d1[0] = 0x11; d1[1] = 0x22; d1[2] = 0x33; d1[3] = 0x44;
        driver.enqueue(ChannelData::create(cfg0, fl::move(d0)));
        driver.enqueue(ChannelData::create(cfg1, fl::move(d1)));
        driver.show();
    }

    FL_CHECK(mock.getTransmitCount() == 2);
    fl::span<const u8> tx2 = mock.getLastTransmitData();
    FL_REQUIRE(tx2.size() == 8);
    FL_CHECK(tx2[0] == 0xAA);
    FL_CHECK(tx2[1] == 0x11);
}

//=============================================================================
// Error Handling
//=============================================================================

FL_TEST_CASE("ChannelDriverI2sSpi - transmit failure handling") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);

    auto &mock = I2sSpiPeripheralMock::instance();
    mock.setTransmitFailure(true);

    auto channelData = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(channelData);
    driver.show();

    // Failure recovers to READY immediately (no DMA started)
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// Multiple Cycles
//=============================================================================

FL_TEST_CASE("ChannelDriverI2sSpi - async multi-cycle") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);
    auto &mock = I2sSpiPeripheralMock::instance();

    for (int cycle = 0; cycle < 3; cycle++) {
        auto channelData = createSpiTestChannelData(5, 18, 20);
        driver.enqueue(channelData);
        driver.show();

        FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);

        mock.simulateTransmitComplete();
        FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    }

    FL_CHECK(mock.getTransmitCount() == 3);
}

FL_TEST_CASE("ChannelDriverI2sSpi - second show waits for first") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverI2sSpi driver(peripheral);
    auto &mock = I2sSpiPeripheralMock::instance();

    // First show: DMA in progress
    auto ch1 = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(ch1);
    driver.show();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);

    // Complete first DMA before second show can proceed
    mock.simulateTransmitComplete();

    // Second show: should work because first completed
    auto ch2 = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(ch2);
    driver.show();
    FL_CHECK(mock.getTransmitCount() == 2);

    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

#endif // FASTLED_STUB_IMPL

} // FL_TEST_FILE
