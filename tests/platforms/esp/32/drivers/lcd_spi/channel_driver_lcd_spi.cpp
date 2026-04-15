/// @file channel_driver_lcd_spi.cpp
/// @brief Unit tests for LCD_CAM SPI channel driver

#ifdef FASTLED_STUB_IMPL

#include "test.h"
#include "platforms/esp/32/drivers/lcd_spi/channel_driver_lcd_spi.h"
#include "platforms/esp/32/drivers/lcd_spi/lcd_spi_peripheral_mock.h"
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

} // anonymous namespace

//=============================================================================
// Creation
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdSpi - creation") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    FL_CHECK(driver.getName() == "LCD_SPI");
}

FL_TEST_CASE("ChannelDriverLcdSpi - initial state is READY") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// canHandle
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdSpi - canHandle accepts SPI chipsets") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    auto spiData = createSpiTestChannelData(5, 18, 10);
    FL_CHECK(driver.canHandle(spiData));
}

FL_TEST_CASE("ChannelDriverLcdSpi - canHandle rejects clockless chipsets") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    auto clocklessData = createClocklessTestChannelData(5, 10);
    FL_CHECK_FALSE(driver.canHandle(clocklessData));
}

FL_TEST_CASE("ChannelDriverLcdSpi - canHandle rejects null") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    FL_CHECK_FALSE(driver.canHandle(nullptr));
}

//=============================================================================
// Async Lifecycle
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdSpi - show is non-blocking") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    auto channelData = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(channelData);
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);

    auto &mock = LcdSpiPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() == 1);
    FL_CHECK(mock.isBusy());
}

FL_TEST_CASE("ChannelDriverLcdSpi - poll transitions DRAINING to READY") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    auto channelData = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(channelData);
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);

    auto &mock = LcdSpiPeripheralMock::instance();
    mock.simulateTransmitComplete();
    FL_CHECK_FALSE(mock.isBusy());

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("ChannelDriverLcdSpi - channels released after async completion") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    auto channelData = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(channelData);
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);

    auto &mock = LcdSpiPeripheralMock::instance();
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("ChannelDriverLcdSpi - empty enqueue does not transmit") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    driver.show();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    auto &mock = LcdSpiPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() == 0);
}

//=============================================================================
// Single Channel Transmission
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdSpi - single channel transmission") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    auto channelData = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(channelData);
    driver.show();

    auto &mock = LcdSpiPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() == 1);

    // 48 bytes * 8 bits/byte * 2 bytes/word = 768
    const auto &history = mock.getTransmitHistory();
    FL_REQUIRE(history.size() == 1);
    FL_CHECK(history[0].size_bytes == 48 * 8 * sizeof(u16));

    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// Multi-Channel
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdSpi - multi-channel transmission") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    auto ch1 = createSpiTestChannelData(5, 18, 10);
    auto ch2 = createSpiTestChannelData(6, 18, 10);

    driver.enqueue(ch1);
    driver.enqueue(ch2);
    driver.show();

    auto &mock = LcdSpiPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() == 1);

    const auto &history = mock.getTransmitHistory();
    FL_REQUIRE(history.size() == 1);
    // max(48,48) bytes * 8 words/byte * 2 bytes/word = 768
    FL_CHECK(history[0].size_bytes == 48 * 8 * sizeof(u16));

    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// Transpose Verification
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdSpi - transpose correctness") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiConfig{5, 18, encoder};

    fl::vector_psram<u8> data;
    data.resize(12);
    data[0] = data[1] = data[2] = data[3] = 0x00;
    data[4] = 0xFF; data[5] = 0xAA; data[6] = 0x55; data[7] = 0xFF;
    data[8] = data[9] = data[10] = data[11] = 0xFF;

    auto channelData = ChannelData::create(spiConfig, fl::move(data));
    driver.enqueue(channelData);
    driver.show();

    auto &mock = LcdSpiPeripheralMock::instance();
    FL_REQUIRE(mock.getTransmitCount() == 1);

    fl::span<const u16> transmitted = mock.getLastTransmitData();
    FL_REQUIRE(transmitted.size() == 96); // 12 bytes * 8

    // First 4 bytes are 0x00 -> 32 words all 0x0000
    for (size_t i = 0; i < 32; i++) {
        FL_CHECK(transmitted[i] == 0x0000);
    }
    // Byte 4 is 0xFF -> 8 words all with bit 0 set
    for (size_t i = 32; i < 40; i++) {
        FL_CHECK(transmitted[i] == 0x0001);
    }
    // Byte 5 is 0xAA (10101010) -> alternating
    FL_CHECK(transmitted[40] == 0x0001); FL_CHECK(transmitted[41] == 0x0000);
    FL_CHECK(transmitted[42] == 0x0001); FL_CHECK(transmitted[43] == 0x0000);
    FL_CHECK(transmitted[44] == 0x0001); FL_CHECK(transmitted[45] == 0x0000);
    FL_CHECK(transmitted[46] == 0x0001); FL_CHECK(transmitted[47] == 0x0000);
    // Last 4 bytes are 0xFF
    for (size_t i = 64; i < 96; i++) {
        FL_CHECK(transmitted[i] == 0x0001);
    }
}

FL_TEST_CASE("ChannelDriverLcdSpi - transpose two lanes") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    SpiEncoder encoder = SpiEncoder::apa102();

    SpiChipsetConfig cfg0{5, 18, encoder};
    SpiChipsetConfig cfg1{6, 18, encoder};
    fl::vector_psram<u8> d0, d1;
    d0.resize(1); d0[0] = 0xFF;
    d1.resize(1); d1[0] = 0x00;

    driver.enqueue(ChannelData::create(cfg0, fl::move(d0)));
    driver.enqueue(ChannelData::create(cfg1, fl::move(d1)));
    driver.show();

    auto &mock = LcdSpiPeripheralMock::instance();
    fl::span<const u16> tx = mock.getLastTransmitData();
    FL_REQUIRE(tx.size() == 8);

    for (size_t i = 0; i < 8; i++) {
        FL_CHECK(tx[i] == 0x0001); // only lane 0 bit set
    }
}

FL_TEST_CASE("ChannelDriverLcdSpi - transpose two lanes both active") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    SpiEncoder encoder = SpiEncoder::apa102();

    SpiChipsetConfig cfg0{5, 18, encoder};
    SpiChipsetConfig cfg1{6, 18, encoder};
    fl::vector_psram<u8> d0, d1;
    d0.resize(1); d0[0] = 0xFF;
    d1.resize(1); d1[0] = 0xFF;

    driver.enqueue(ChannelData::create(cfg0, fl::move(d0)));
    driver.enqueue(ChannelData::create(cfg1, fl::move(d1)));
    driver.show();

    auto &mock = LcdSpiPeripheralMock::instance();
    fl::span<const u16> tx = mock.getLastTransmitData();
    FL_REQUIRE(tx.size() == 8);

    for (size_t i = 0; i < 8; i++) {
        FL_CHECK(tx[i] == 0x0003); // bits 0 and 1
    }
}

FL_TEST_CASE("ChannelDriverLcdSpi - transpose alternating lanes") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    SpiEncoder encoder = SpiEncoder::apa102();

    SpiChipsetConfig cfg0{5, 18, encoder};
    SpiChipsetConfig cfg1{6, 18, encoder};
    fl::vector_psram<u8> d0, d1;
    d0.resize(1); d0[0] = 0xAA; // 10101010
    d1.resize(1); d1[0] = 0x55; // 01010101

    driver.enqueue(ChannelData::create(cfg0, fl::move(d0)));
    driver.enqueue(ChannelData::create(cfg1, fl::move(d1)));
    driver.show();

    auto &mock = LcdSpiPeripheralMock::instance();
    fl::span<const u16> tx = mock.getLastTransmitData();
    FL_REQUIRE(tx.size() == 8);

    FL_CHECK(tx[0] == 0x0001); FL_CHECK(tx[1] == 0x0002);
    FL_CHECK(tx[2] == 0x0001); FL_CHECK(tx[3] == 0x0002);
    FL_CHECK(tx[4] == 0x0001); FL_CHECK(tx[5] == 0x0002);
    FL_CHECK(tx[6] == 0x0001); FL_CHECK(tx[7] == 0x0002);
}

FL_TEST_CASE("ChannelDriverLcdSpi - transpose unequal channel sizes") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    SpiEncoder encoder = SpiEncoder::apa102();

    SpiChipsetConfig cfg0{5, 18, encoder};
    SpiChipsetConfig cfg1{6, 18, encoder};
    fl::vector_psram<u8> d0, d1;
    d0.resize(2); d0[0] = 0xFF; d0[1] = 0xFF;
    d1.resize(1); d1[0] = 0xFF;

    driver.enqueue(ChannelData::create(cfg0, fl::move(d0)));
    driver.enqueue(ChannelData::create(cfg1, fl::move(d1)));
    driver.show();

    auto &mock = LcdSpiPeripheralMock::instance();
    fl::span<const u16> tx = mock.getLastTransmitData();
    FL_REQUIRE(tx.size() == 16); // 2 bytes * 8

    // Byte 0: both 0xFF -> 0x0003
    for (size_t i = 0; i < 8; i++) {
        FL_CHECK(tx[i] == 0x0003);
    }
    // Byte 1: lane0=0xFF, lane1=0x00 (padded) -> 0x0001
    for (size_t i = 8; i < 16; i++) {
        FL_CHECK(tx[i] == 0x0001);
    }
}

FL_TEST_CASE("ChannelDriverLcdSpi - transpose three lanes") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    SpiEncoder encoder = SpiEncoder::apa102();

    SpiChipsetConfig cfg0{5, 18, encoder};
    SpiChipsetConfig cfg1{6, 18, encoder};
    SpiChipsetConfig cfg2{7, 18, encoder};
    fl::vector_psram<u8> d0, d1, d2;
    d0.resize(1); d0[0] = 0xFF;
    d1.resize(1); d1[0] = 0xFF;
    d2.resize(1); d2[0] = 0xFF;

    driver.enqueue(ChannelData::create(cfg0, fl::move(d0)));
    driver.enqueue(ChannelData::create(cfg1, fl::move(d1)));
    driver.enqueue(ChannelData::create(cfg2, fl::move(d2)));
    driver.show();

    auto &mock = LcdSpiPeripheralMock::instance();
    fl::span<const u16> tx = mock.getLastTransmitData();
    FL_REQUIRE(tx.size() == 8);

    for (size_t i = 0; i < 8; i++) {
        FL_CHECK(tx[i] == 0x0007); // bits 0,1,2
    }
}

//=============================================================================
// Re-initialization
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdSpi - re-init on buffer growth") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    SpiEncoder encoder = SpiEncoder::apa102();

    // First show: 1 byte -> 8 words
    {
        SpiChipsetConfig cfg{5, 18, encoder};
        fl::vector_psram<u8> d;
        d.resize(1); d[0] = 0xFF;
        driver.enqueue(ChannelData::create(cfg, fl::move(d)));
        driver.show();
        mock.simulateTransmitComplete();
        FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    }

    FL_CHECK(mock.getTransmitCount() == 1);
    FL_CHECK(mock.getLastTransmitData().size() == 8);

    // Second show: 4 bytes -> 32 words (bigger buffer)
    {
        SpiChipsetConfig cfg{5, 18, encoder};
        fl::vector_psram<u8> d;
        d.resize(4);
        d[0] = 0xFF; d[1] = 0x00; d[2] = 0xFF; d[3] = 0x00;
        driver.enqueue(ChannelData::create(cfg, fl::move(d)));
        driver.show();
    }

    FL_CHECK(mock.getTransmitCount() == 2);
    fl::span<const u16> tx = mock.getLastTransmitData();
    FL_REQUIRE(tx.size() == 32);

    for (size_t i = 0; i < 8; i++) {
        FL_CHECK(tx[i] == 0x0001);
    }
    for (size_t i = 8; i < 16; i++) {
        FL_CHECK(tx[i] == 0x0000);
    }
}

//=============================================================================
// Error Handling
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdSpi - transmit failure handling") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);

    auto &mock = LcdSpiPeripheralMock::instance();
    mock.setTransmitFailure(true);

    auto channelData = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(channelData);
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// Multiple Cycles
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdSpi - async multi-cycle") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

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

FL_TEST_CASE("ChannelDriverLcdSpi - second show waits for first") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    auto ch1 = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(ch1);
    driver.show();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);

    mock.simulateTransmitComplete();

    auto ch2 = createSpiTestChannelData(5, 18, 10);
    driver.enqueue(ch2);
    driver.show();
    FL_CHECK(mock.getTransmitCount() == 2);

    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

//=============================================================================
// Buffer growth without peripheral reinit
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdSpi - buffer growth without peripheral reinit") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    SpiEncoder encoder = SpiEncoder::apa102();

    // First show: small data (1 byte)
    {
        SpiChipsetConfig cfg{5, 18, encoder};
        fl::vector_psram<u8> d;
        d.resize(1); d[0] = 0xFF;
        driver.enqueue(ChannelData::create(cfg, fl::move(d)));
        driver.show();
        mock.simulateTransmitComplete();
        FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    }

    FL_CHECK(mock.getTransmitCount() == 1);
    size_t deinitsBefore = mock.getDeinitCount();

    // Second show: larger data (48 bytes, same lane count)
    // This should NOT deinitialize the peripheral — only reallocate the buffer.
    {
        SpiChipsetConfig cfg{5, 18, encoder};
        fl::vector_psram<u8> d;
        d.resize(48);
        for (size_t i = 0; i < 48; i++) d[i] = static_cast<u8>(i);
        driver.enqueue(ChannelData::create(cfg, fl::move(d)));
        driver.show();
    }

    FL_CHECK(mock.getTransmitCount() == 2);
    FL_CHECK(mock.getDeinitCount() == deinitsBefore);

    fl::span<const u16> tx = mock.getLastTransmitData();
    FL_CHECK(tx.size() == 48 * 8);
}

FL_TEST_CASE("ChannelDriverLcdSpi - lane count change triggers reinit") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    SpiEncoder encoder = SpiEncoder::apa102();

    // First show: 1 lane
    {
        SpiChipsetConfig cfg{5, 18, encoder};
        fl::vector_psram<u8> d;
        d.resize(1); d[0] = 0xFF;
        driver.enqueue(ChannelData::create(cfg, fl::move(d)));
        driver.show();
        mock.simulateTransmitComplete();
        FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    }

    size_t deinitsBefore = mock.getDeinitCount();

    // Second show: 2 lanes — peripheral MUST be reinitialized
    {
        SpiChipsetConfig cfg0{5, 18, encoder};
        SpiChipsetConfig cfg1{6, 18, encoder};
        fl::vector_psram<u8> d0, d1;
        d0.resize(1); d0[0] = 0xFF;
        d1.resize(1); d1[0] = 0xFF;
        driver.enqueue(ChannelData::create(cfg0, fl::move(d0)));
        driver.enqueue(ChannelData::create(cfg1, fl::move(d1)));
        driver.show();
    }

    FL_CHECK(mock.getTransmitCount() == 2);
    FL_CHECK(mock.getDeinitCount() == deinitsBefore + 1);

    fl::span<const u16> tx = mock.getLastTransmitData();
    FL_REQUIRE(tx.size() == 8);
    for (size_t i = 0; i < 8; i++) {
        FL_CHECK(tx[i] == 0x0003); // bits 0 and 1
    }
}

//=============================================================================
// ISR-Driven Chunked Streaming Tests (Issue #2258)
//=============================================================================

FL_TEST_CASE("ChannelDriverLcdSpi - single chunk with ISR callback") {
    // Small data that fits in one chunk — ISR fires once, marks complete
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    // 4 bytes = 32 u16 words. With default chunk size (2048 bytes), fits
    // in one chunk.
    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig cfg{5, 18, encoder};
    fl::vector_psram<u8> d;
    d.resize(4);
    d[0] = 0xFF; d[1] = 0x00; d[2] = 0xAA; d[3] = 0x55;
    driver.enqueue(ChannelData::create(cfg, fl::move(d)));
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);
    FL_CHECK(mock.getTransmitCount() == 1);

    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    // Verify: exactly 1 transmit (no ISR-initiated follow-up)
    FL_CHECK(mock.getTransmitCount() == 1);
}

FL_TEST_CASE("ChannelDriverLcdSpi - multi-chunk streaming") {
    // Force small chunk size to create multiple chunks
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    // 16 source bytes, 4 bytes per chunk = 4 chunks
    driver.setChunkInputBytesForTest(4);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig cfg{5, 18, encoder};
    fl::vector_psram<u8> d;
    d.resize(16);
    for (size_t i = 0; i < 16; i++) {
        d[i] = static_cast<u8>(i);
    }
    driver.enqueue(ChannelData::create(cfg, fl::move(d)));
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);
    // First chunk submitted by beginTransmission
    FL_CHECK(mock.getTransmitCount() == 1);

    // simulateTransmitComplete drains the entire ISR callback chain
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    // 4 chunks total: 1 initial + 3 from ISR callbacks
    FL_CHECK(mock.getTransmitCount() == 4);
}

FL_TEST_CASE("ChannelDriverLcdSpi - chunk transpose fidelity") {
    // Verify chunked transpose produces identical output to full-buffer
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    // Force 4 bytes per chunk
    driver.setChunkInputBytesForTest(4);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig cfg{5, 18, encoder};
    fl::vector_psram<u8> d;
    d.resize(12);
    d[0] = 0x00; d[1] = 0xFF; d[2] = 0xAA; d[3] = 0x55;
    d[4] = 0x0F; d[5] = 0xF0; d[6] = 0x33; d[7] = 0xCC;
    d[8] = 0x01; d[9] = 0x80; d[10] = 0x7E; d[11] = 0xBD;

    driver.enqueue(ChannelData::create(cfg, fl::move(d)));
    driver.show();
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    // 3 chunks of 4 bytes each
    FL_CHECK(mock.getTransmitCount() == 3);

    // Concatenate all chunk outputs
    const auto &history = mock.getTransmitHistory();
    fl::vector<u16> combined;
    for (const auto &record : history) {
        for (const auto &word : record.buffer_copy) {
            combined.push_back(word);
        }
    }

    // Compare against single-shot reference
    FL_REQUIRE(combined.size() == 12 * 8); // 96 words

    // Byte 0 = 0x00 → 8 words of 0x0000
    for (size_t i = 0; i < 8; i++) {
        FL_CHECK(combined[i] == 0x0000);
    }
    // Byte 1 = 0xFF → 8 words of 0x0001 (lane 0 set)
    for (size_t i = 8; i < 16; i++) {
        FL_CHECK(combined[i] == 0x0001);
    }
    // Byte 2 = 0xAA (10101010) → alternating
    FL_CHECK(combined[16] == 0x0001); FL_CHECK(combined[17] == 0x0000);
    FL_CHECK(combined[18] == 0x0001); FL_CHECK(combined[19] == 0x0000);
    FL_CHECK(combined[20] == 0x0001); FL_CHECK(combined[21] == 0x0000);
    FL_CHECK(combined[22] == 0x0001); FL_CHECK(combined[23] == 0x0000);
    // Byte 3 = 0x55 (01010101) → alternating (opposite)
    FL_CHECK(combined[24] == 0x0000); FL_CHECK(combined[25] == 0x0001);
    FL_CHECK(combined[26] == 0x0000); FL_CHECK(combined[27] == 0x0001);
    FL_CHECK(combined[28] == 0x0000); FL_CHECK(combined[29] == 0x0001);
    FL_CHECK(combined[30] == 0x0000); FL_CHECK(combined[31] == 0x0001);
}

FL_TEST_CASE("ChannelDriverLcdSpi - multi-chunk two lanes") {
    // Multi-lane with small chunk size to exercise lane transposition
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    driver.setChunkInputBytesForTest(2);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig cfg0{5, 18, encoder};
    SpiChipsetConfig cfg1{6, 18, encoder};

    fl::vector_psram<u8> d0, d1;
    d0.resize(4); d1.resize(4);
    d0[0] = 0xFF; d0[1] = 0xFF; d0[2] = 0xFF; d0[3] = 0xFF;
    d1[0] = 0x00; d1[1] = 0x00; d1[2] = 0xFF; d1[3] = 0xFF;

    driver.enqueue(ChannelData::create(cfg0, fl::move(d0)));
    driver.enqueue(ChannelData::create(cfg1, fl::move(d1)));
    driver.show();
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    // 4 bytes / 2 per chunk = 2 chunks
    FL_CHECK(mock.getTransmitCount() == 2);

    // Concatenate
    const auto &history = mock.getTransmitHistory();
    fl::vector<u16> combined;
    for (const auto &rec : history) {
        for (const auto &w : rec.buffer_copy) {
            combined.push_back(w);
        }
    }
    FL_REQUIRE(combined.size() == 32); // 4 bytes * 8

    // Chunk 0 (bytes 0-1): lane0=0xFF,0xFF  lane1=0x00,0x00
    // Only lane 0 active
    for (size_t i = 0; i < 16; i++) {
        FL_CHECK(combined[i] == 0x0001);
    }
    // Chunk 1 (bytes 2-3): lane0=0xFF,0xFF  lane1=0xFF,0xFF
    // Both lanes active
    for (size_t i = 16; i < 32; i++) {
        FL_CHECK(combined[i] == 0x0003);
    }
}

FL_TEST_CASE("ChannelDriverLcdSpi - ring wrap 3+ chunks") {
    // 6 chunks to wrap the ring buffer (3 slots) twice
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    driver.setChunkInputBytesForTest(2);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig cfg{5, 18, encoder};
    fl::vector_psram<u8> d;
    d.resize(12); // 12 / 2 = 6 chunks
    for (size_t i = 0; i < 12; i++) {
        d[i] = static_cast<u8>(0xFF); // all ones for easy verification
    }
    driver.enqueue(ChannelData::create(cfg, fl::move(d)));
    driver.show();
    mock.simulateTransmitComplete();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK(mock.getTransmitCount() == 6);

    // All chunks should produce the same output (all 0xFF → all 0x0001)
    const auto &history = mock.getTransmitHistory();
    for (size_t c = 0; c < history.size(); c++) {
        FL_REQUIRE(history[c].buffer_copy.size() == 16); // 2 bytes * 8
        for (size_t i = 0; i < 16; i++) {
            FL_CHECK(history[c].buffer_copy[i] == 0x0001);
        }
    }
}

FL_TEST_CASE("ChannelDriverLcdSpi - multi-cycle chunked reuse") {
    // Multiple show() calls reuse ring buffers
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    driver.setChunkInputBytesForTest(4);

    SpiEncoder encoder = SpiEncoder::apa102();

    for (int cycle = 0; cycle < 3; cycle++) {
        SpiChipsetConfig cfg{5, 18, encoder};
        fl::vector_psram<u8> d;
        d.resize(8); // 2 chunks per cycle
        for (size_t i = 0; i < 8; i++) {
            d[i] = static_cast<u8>(cycle * 16 + i);
        }
        driver.enqueue(ChannelData::create(cfg, fl::move(d)));
        driver.show();
        mock.simulateTransmitComplete();
        FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    }

    // 3 cycles × 2 chunks = 6 total transmits
    FL_CHECK(mock.getTransmitCount() == 6);
}

FL_TEST_CASE("ChannelDriverLcdSpi - error mid-stream aborts cleanly") {
    // Inject failure on the 2nd chunk — stream should abort
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    driver.setChunkInputBytesForTest(4);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig cfg{5, 18, encoder};
    fl::vector_psram<u8> d;
    d.resize(12); // 3 chunks
    for (size_t i = 0; i < 12; i++) d[i] = 0xFF;
    driver.enqueue(ChannelData::create(cfg, fl::move(d)));
    driver.show();

    FL_CHECK(mock.getTransmitCount() == 1);

    // Inject failure before completing first chunk
    mock.setTransmitFailure(true);
    mock.simulateTransmitComplete();

    // ISR tried to submit chunk 2 but transmit failed → stream aborted
    // The initial transmit succeeded, ISR callback's transmit failed
    FL_CHECK(mock.getTransmitCount() == 1); // only the initial one succeeded

    // Driver should transition to READY after stream abort
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("ChannelDriverLcdSpi - asymmetric lanes with chunking") {
    // Lane 0: 8 bytes, Lane 1: 4 bytes
    // With chunk size 4, lane 1 runs out mid-stream
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    driver.setChunkInputBytesForTest(4);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig cfg0{5, 18, encoder};
    SpiChipsetConfig cfg1{6, 18, encoder};

    fl::vector_psram<u8> d0, d1;
    d0.resize(8); d1.resize(4);
    for (size_t i = 0; i < 8; i++) d0[i] = 0xFF;
    for (size_t i = 0; i < 4; i++) d1[i] = 0xFF;

    driver.enqueue(ChannelData::create(cfg0, fl::move(d0)));
    driver.enqueue(ChannelData::create(cfg1, fl::move(d1)));
    driver.show();
    mock.simulateTransmitComplete();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    // max(8, 4) = 8 bytes, 8/4 = 2 chunks
    FL_CHECK(mock.getTransmitCount() == 2);

    const auto &history = mock.getTransmitHistory();

    // Chunk 0 (bytes 0-3): both lanes 0xFF → 0x0003
    FL_REQUIRE(history[0].buffer_copy.size() == 32);
    for (size_t i = 0; i < 32; i++) {
        FL_CHECK(history[0].buffer_copy[i] == 0x0003);
    }

    // Chunk 1 (bytes 4-7): lane0=0xFF, lane1=0x00 (past end) → 0x0001
    FL_REQUIRE(history[1].buffer_copy.size() == 32);
    for (size_t i = 0; i < 32; i++) {
        FL_CHECK(history[1].buffer_copy[i] == 0x0001);
    }
}

FL_TEST_CASE("ChannelDriverLcdSpi - single byte chunk") {
    // Extreme case: 1 byte per chunk
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    driver.setChunkInputBytesForTest(1);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig cfg{5, 18, encoder};
    fl::vector_psram<u8> d;
    d.resize(4);
    d[0] = 0xFF; d[1] = 0x00; d[2] = 0xAA; d[3] = 0x55;
    driver.enqueue(ChannelData::create(cfg, fl::move(d)));
    driver.show();
    mock.simulateTransmitComplete();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK(mock.getTransmitCount() == 4); // 4 chunks of 1 byte each

    const auto &history = mock.getTransmitHistory();

    // Chunk 0: 0xFF → all bits set → 8 words of 0x0001
    FL_REQUIRE(history[0].buffer_copy.size() == 8);
    for (size_t i = 0; i < 8; i++) {
        FL_CHECK(history[0].buffer_copy[i] == 0x0001);
    }
    // Chunk 1: 0x00 → no bits set → 8 words of 0x0000
    FL_REQUIRE(history[1].buffer_copy.size() == 8);
    for (size_t i = 0; i < 8; i++) {
        FL_CHECK(history[1].buffer_copy[i] == 0x0000);
    }
}

FL_TEST_CASE("ChannelDriverLcdSpi - chunk size larger than data") {
    // Chunk size > data size → single chunk, same as default behavior
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdSpi driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    driver.setChunkInputBytesForTest(1000);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig cfg{5, 18, encoder};
    fl::vector_psram<u8> d;
    d.resize(8);
    for (size_t i = 0; i < 8; i++) d[i] = 0xFF;
    driver.enqueue(ChannelData::create(cfg, fl::move(d)));
    driver.show();

    FL_CHECK(mock.getTransmitCount() == 1);
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK(mock.getTransmitCount() == 1);
}

#endif // FASTLED_STUB_IMPL

} // FL_TEST_FILE
