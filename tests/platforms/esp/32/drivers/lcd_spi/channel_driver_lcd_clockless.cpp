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
#include "fl/stl/cstring.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::detail;

namespace {

void resetMockState() {
    auto &mock = LcdSpiPeripheralMock::instance();
    mock.reset();
}

fl::shared_ptr<ILcdSpiPeripheral> createMockPeripheral(
    bool failFirstAllocation = false, u8 failInitialization = 0) {
    class MockWrapper : public ILcdSpiPeripheral {
      public:
        MockWrapper(bool failAllocation, u8 failInitialization)
            : mFailAllocation(failAllocation),
              mFailInitialization(failInitialization) {}
        bool initialize(const LcdSpiConfig &config) FL_NO_EXCEPT override {
            if (mFailInitialization > 0 && --mFailInitialization == 0) {
                return false;
            }
            return LcdSpiPeripheralMock::instance().initialize(config);
        }
        void deinitialize() FL_NO_EXCEPT override {
            LcdSpiPeripheralMock::instance().deinitialize();
        }
        bool isInitialized() const FL_NO_EXCEPT override {
            return LcdSpiPeripheralMock::instance().isInitialized();
        }
        u16 *allocateBuffer(size_t size_bytes) FL_NO_EXCEPT override {
            if (mFailAllocation) {
                mFailAllocation = false;
                return nullptr;
            }
            return LcdSpiPeripheralMock::instance().allocateBuffer(size_bytes);
        }
        void freeBuffer(u16 *buffer) FL_NO_EXCEPT override {
            LcdSpiPeripheralMock::instance().freeBuffer(buffer);
        }
        bool transmit(const u16 *buffer,
                      size_t size_bytes) FL_NO_EXCEPT override {
            return LcdSpiPeripheralMock::instance().transmit(buffer,
                                                             size_bytes);
        }
        bool waitTransmitDone(u32 timeout_ms) FL_NO_EXCEPT override {
            return LcdSpiPeripheralMock::instance().waitTransmitDone(
                timeout_ms);
        }
        bool isBusy() const FL_NO_EXCEPT override {
            return LcdSpiPeripheralMock::instance().isBusy();
        }
        bool registerTransmitCallback(void *callback,
                                      void *user_ctx) FL_NO_EXCEPT override {
            return LcdSpiPeripheralMock::instance().registerTransmitCallback(
                callback, user_ctx);
        }
        const LcdSpiConfig &getConfig() const FL_NO_EXCEPT override {
            return LcdSpiPeripheralMock::instance().getConfig();
        }
        u64 getMicroseconds() FL_NO_EXCEPT override {
            return LcdSpiPeripheralMock::instance().getMicroseconds();
        }
        void delay(u32 ms) FL_NO_EXCEPT override {
            LcdSpiPeripheralMock::instance().delay(ms);
        }
      private:
        bool mFailAllocation;
        u8 mFailInitialization;
    };

    return fl::make_shared<MockWrapper>(failFirstAllocation, failInitialization);
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

ChannelDataPtr createClocklessChannelDataBytes(int pin, size_t numBytes) {
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    fl::vector_psram<u8> data;
    data.resize(numBytes);
    for (size_t i = 0; i < numBytes; i++) {
        data[i] = static_cast<u8>(i % 256);
    }
    return ChannelData::create(pin, timing, fl::move(data));
}

fl::vector<u16> collectTransmitWords() {
    const auto &history = LcdSpiPeripheralMock::instance().getTransmitHistory();
    fl::vector<u16> combined;
    for (const auto &record : history) {
        for (const auto &word : record.buffer_copy) {
            combined.push_back(word);
        }
    }
    return combined;
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

// Issue #5001: all strips on the LCD engine share one worker policy.
FL_TEST_CASE("ChannelDriverLcdClockless - worker configuration is shared and immutable") {
    resetMockState();
    ChannelDriverLcdClockless driver(createMockPeripheral());

    WorkerConfig config;
    config.core = Core::Core1;
    config.priority = Priority::High;
    FL_CHECK_EQ(driver.workerDiagnostics().core, -2);
    FL_REQUIRE_TRUE(driver.configureWorker(config));
    FL_CHECK_TRUE(driver.configureWorker(config));
    FL_CHECK_TRUE(driver.configureWorker(WorkerConfig()));

    WorkerConfig conflicting = config;
    conflicting.core = Core::Core0;
    FL_CHECK_FALSE(driver.configureWorker(conflicting));
    conflicting = config;
    conflicting.priority = Priority::Medium;
    FL_CHECK_FALSE(driver.configureWorker(conflicting));

    auto data = createClocklessChannelData(5, 1);
    driver.enqueue(data);
    driver.show();
    FL_CHECK_EQ(driver.workerDiagnostics().core, 1);
    FL_CHECK_EQ(driver.workerDiagnostics().priority, 23);
    // The host stub cannot establish an actual RTOS execution core.
    FL_CHECK_EQ(driver.workerDiagnostics().observedCore, -1);
    FL_CHECK_TRUE(driver.configureWorker(config));
    FL_CHECK_FALSE(driver.configureWorker(conflicting));
    FL_CHECK_EQ(driver.workerDiagnostics().core, 1);
    LcdSpiPeripheralMock::instance().simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("ChannelDriverLcdClockless - invalid worker configuration does not lock policy") {
    resetMockState();
    ChannelDriverLcdClockless driver(createMockPeripheral());

    WorkerConfig config;
    config.core = static_cast<Core>(2);
    FL_CHECK_FALSE(driver.configureWorker(config));
    config.core = Core::Core1;
    config.priority = static_cast<Priority>(255);
    FL_CHECK_FALSE(driver.configureWorker(config));
    config.priority = Priority::Low;
    FL_CHECK_TRUE(driver.configureWorker(config));
}

FL_TEST_CASE("ChannelDriverLcdClockless - named priorities resolve without changing shared policies") {
    struct PriorityCase { Priority policy; int resolved; };
    const PriorityCase cases[] = {
        {Priority::Auto, 23}, {Priority::High, 23},
        {Priority::Medium, 12}, {Priority::Low, 1}
    };
    for (const auto& entry : cases) {
        resetMockState();
        ChannelDriverLcdClockless driver(createMockPeripheral());
        WorkerConfig config;
        config.priority = entry.policy;
        FL_REQUIRE_TRUE(driver.configureWorker(config));
        auto data = createClocklessChannelData(5, 1);
        driver.enqueue(data);
        driver.show();
        FL_CHECK_EQ(driver.workerDiagnostics().priority, entry.resolved);
        FL_CHECK_TRUE(driver.configureWorker(WorkerConfig{}));
        FL_CHECK_EQ(driver.workerDiagnostics().priority, entry.resolved);
        config.priority = entry.resolved == 23 ? Priority::High : entry.policy;
        FL_CHECK_TRUE(driver.configureWorker(config));
        config.priority = entry.resolved == 1 ? Priority::High : Priority::Low;
        FL_CHECK_FALSE(driver.configureWorker(config));
        LcdSpiPeripheralMock::instance().simulateTransmitComplete();
        FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    }
}

FL_TEST_CASE("ChannelDriverLcdClockless - Auto preserves unpinned scheduling and rejects late changes") {
    resetMockState();
    ChannelDriverLcdClockless driver(createMockPeripheral());
    auto data = createClocklessChannelData(5, 1);
    driver.enqueue(data);
    driver.show();

    WorkerConfig config;
    FL_CHECK_EQ(driver.workerDiagnostics().core, -1);
    config.core = Core::Core1;
    FL_CHECK_FALSE(driver.configureWorker(config));
    FL_CHECK_TRUE(driver.configureWorker(WorkerConfig()));
    LcdSpiPeripheralMock::instance().simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("ChannelDriverLcdClockless - Any explicitly retains unpinned scheduling") {
    resetMockState();
    ChannelDriverLcdClockless driver(createMockPeripheral());
    WorkerConfig config;
    config.core = Core::Any;
    FL_REQUIRE_TRUE(driver.configureWorker(config));
    auto data = createClocklessChannelData(5, 1);
    driver.enqueue(data);
    driver.show();
    FL_CHECK_EQ(driver.workerDiagnostics().core, -1);
    LcdSpiPeripheralMock::instance().simulateTransmitComplete();
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

    FL_CHECK(mock.getTransmitCount() == 2);

    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK(mock.getTransmitCount() == 3);
    const auto diagnostics = driver.workerDiagnostics();
    FL_CHECK_EQ(diagnostics.totalChunks, 3);
    FL_CHECK_EQ(diagnostics.submittedChunks, 3);
    FL_CHECK_EQ(diagnostics.completedChunks, 3);
    FL_CHECK_FALSE(diagnostics.streamError);
}

FL_TEST_CASE("ChannelDriverLcdClockless - immediate completion preserves chunk publication") {
    resetMockState();
    auto &mock = LcdSpiPeripheralMock::instance();
    mock.setAutoComplete(true);
    ChannelDriverLcdClockless driver(createMockPeripheral());
    driver.setChunkInputBytesForTest(3);
    auto data = createClocklessChannelData(5, 3);
    driver.enqueue(data);
    driver.show();

    FL_CHECK_EQ(mock.getTransmitCount(), 3);
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    const auto diagnostics = driver.workerDiagnostics();
    FL_CHECK_EQ(diagnostics.totalChunks, 3);
    FL_CHECK_EQ(diagnostics.submittedChunks, 3);
    FL_CHECK_EQ(diagnostics.completedChunks, 3);
    FL_CHECK_FALSE(diagnostics.streamError);
    FL_CHECK_FALSE(data->isInUse());
}

FL_TEST_CASE("ChannelDriverLcdClockless - awkward chunk size rounds up") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    driver.setChunkInputBytesForTest(1024);

    auto data = createClocklessChannelDataBytes(5, 1025);
    driver.enqueue(data);
    driver.show();

    FL_CHECK(mock.getTransmitCount() == 2);

    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK(mock.getTransmitCount() == 2);

    const auto &history = mock.getTransmitHistory();
    FL_REQUIRE(history.size() == 2);
    const size_t dmaBytesPerInputByte = 16 * sizeof(Wave3Byte);
    FL_CHECK(history[0].size_bytes == 513 * dmaBytesPerInputByte);
    FL_CHECK(history[1].size_bytes == 513 * dmaBytesPerInputByte);
}

FL_TEST_CASE("ChannelDriverLcdClockless - wave8 word folding matches reference") {
    const size_t laneCounts[] = {1, 2, 6, 16};
    const ChipsetTimingConfig timings[] = {
        makeTimingConfig<TIMING_WS2812B_V5>(),
        ChipsetTimingConfig(100, 100, 1000, 280),
        ChipsetTimingConfig(700, 300, 200, 280)
    };
    for (const auto &timing : timings) {
        const ChipsetTiming waveform{timing.t1_ns, timing.t2_ns, timing.t3_ns,
                                     timing.reset_us, timing.name};
        FL_REQUIRE_FALSE(canUseWave3(waveform));
        const auto lut = buildWave8ExpansionLUT(waveform);
        for (size_t laneCount : laneCounts) {
            resetMockState();
            ChannelDriverLcdClockless driver(createMockPeripheral());
            driver.setChunkInputBytesForTest(5);
            fl::vector<ChannelDataPtr> sources;
            for (size_t lane = 0; lane < laneCount; ++lane) {
                fl::vector_psram<u8> bytes(21 - (lane % 4) * 3);
                for (size_t i = 0; i < bytes.size(); ++i) {
                    bytes[i] = static_cast<u8>((lane * 17 + i * 7) % 256);
                }
                auto source = ChannelData::create(
                    static_cast<i32>(lane + 1), timing, fl::move(bytes));
                driver.enqueue(source);
                sources.push_back(source);
            }
            driver.show();
            LcdSpiPeripheralMock::instance().simulateTransmitComplete();
            FL_REQUIRE(driver.poll() == IChannelDriver::DriverState::READY);
            const auto actual = collectTransmitWords();
            FL_REQUIRE_EQ(actual.size(), 25 * 64);
            const u8 *dma = reinterpret_cast<const u8 *>(actual.data());
            for (size_t position = 0; position < 25; ++position) {
                u8 lanes[16] = {};
                for (size_t lane = 0; lane < laneCount; ++lane) {
                    const auto &bytes = sources[lane]->getData();
                    if (position < bytes.size()) {
                        lanes[lane] = bytes[position];
                    }
                }
                u8 expected[128];
                wave8Transpose_16(lanes, lut, expected);
                FL_CHECK_EQ(fl::memcmp(dma + position * 128, expected,
                                       sizeof(expected)), 0);
            }
        }
    }
}

FL_TEST_CASE("ChannelDriverLcdClockless - wave3 bit fold matches reference for active lanes") {
    const size_t laneCounts[] = {1, 6, 16};
    for (size_t timingIndex = 0; timingIndex < 2; ++timingIndex) {
        const auto timing = timingIndex == 0
                                ? makeTimingConfig<TIMING_WS2812_800KHZ>()
                                : makeTimingConfig<TIMING_SK6812>();
        const ChipsetTiming waveform{timing.t1_ns, timing.t2_ns, timing.t3_ns,
                                     timing.reset_us, timing.name};
        FL_REQUIRE_TRUE(canUseWave3(waveform));
        const auto lut = buildWave3ExpansionLUT(waveform);
        for (size_t laneCount : laneCounts) {
            resetMockState();
            ChannelDriverLcdClockless driver(createMockPeripheral());
            driver.setChunkInputBytesForTest(5);
            fl::vector<ChannelDataPtr> sources;
            for (size_t lane = 0; lane < laneCount; ++lane) {
                fl::vector_psram<u8> bytes(21 - (lane % 4) * 3);
                for (size_t i = 0; i < bytes.size(); ++i) {
                    bytes[i] = static_cast<u8>((lane * 17 + i * 7) % 256);
                }
                auto source = ChannelData::create(
                    static_cast<i32>(lane + 1), timing, fl::move(bytes));
                driver.enqueue(source);
                sources.push_back(source);
            }
            driver.show();
            LcdSpiPeripheralMock::instance().simulateTransmitComplete();
            FL_REQUIRE(driver.poll() == IChannelDriver::DriverState::READY);
            const auto actual = collectTransmitWords();
            FL_REQUIRE_EQ(actual.size(), 25 * 24);
            const u8 *dma = reinterpret_cast<const u8 *>(actual.data());
            for (size_t position = 0; position < 25; ++position) {
                u8 lanes[16] = {};
                for (size_t lane = 0; lane < laneCount; ++lane) {
                    const auto &bytes = sources[lane]->getData();
                    if (position < bytes.size()) {
                        lanes[lane] = bytes[position];
                    }
                }
                u8 expected[48];
                wave3Transpose_16(lanes, lut, expected);
                FL_CHECK_EQ(fl::memcmp(dma + position * 48, expected,
                                       sizeof(expected)), 0);
            }
        }
    }
}

FL_TEST_CASE("ChannelDriverLcdClockless - default multi-chunk 400 LED fidelity") {
    fl::vector<u16> reference;
    {
        resetMockState();
        auto peripheral = createMockPeripheral();
        ChannelDriverLcdClockless driver(peripheral);
        auto &mock = LcdSpiPeripheralMock::instance();

        driver.setChunkInputBytesForTest(4096);
        auto data = createClocklessChannelData(5, 400);
        driver.enqueue(data);
        driver.show();
        FL_CHECK(mock.getTransmitCount() == 1);
        mock.simulateTransmitComplete();
        FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
        reference = collectTransmitWords();
    }

    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    auto data = createClocklessChannelData(5, 400);
    driver.enqueue(data);
    driver.show();

    FL_CHECK(mock.getTransmitCount() == 2);
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK(mock.getTransmitCount() == 2);

    fl::vector<u16> chunked = collectTransmitWords();
    FL_REQUIRE(chunked.size() == reference.size());
    for (size_t i = 0; i < reference.size(); i++) {
        FL_CHECK(chunked[i] == reference[i]);
    }
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

FL_TEST_CASE("ChannelDriverLcdClockless - reconfigures changed secondary lane pin") {
    resetMockState();
    ChannelDriverLcdClockless driver(createMockPeripheral());
    auto &mock = LcdSpiPeripheralMock::instance();
    auto firstLane = createClocklessChannelData(1, 2);
    auto secondLane = createClocklessChannelData(3, 2);
    driver.enqueue(firstLane);
    driver.enqueue(secondLane);
    driver.show();
    FL_REQUIRE_EQ(mock.getConfig().data_gpios[1], 3);
    FL_CHECK_EQ(mock.getDeinitCount(), 0);
    mock.simulateTransmitComplete();
    FL_REQUIRE(driver.poll() == IChannelDriver::DriverState::READY);

    secondLane = createClocklessChannelData(4, 2);
    driver.enqueue(firstLane);
    driver.enqueue(secondLane);
    driver.show();
    FL_CHECK_EQ(mock.getDeinitCount(), 1);
    FL_CHECK_EQ(mock.getConfig().data_gpios[0], 1);
    FL_CHECK_EQ(mock.getConfig().data_gpios[1], 4);
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("ChannelDriverLcdClockless - recreates bus for growing and shrinking DMA chunks") {
    resetMockState();
    ChannelDriverLcdClockless driver(createMockPeripheral());
    auto &mock = LcdSpiPeripheralMock::instance();
    const size_t ledCounts[] = {170, 700, 170};
    const size_t dmaBytes[] = {24480, 33600, 24480};

    for (size_t frame = 0; frame < 3; ++frame) {
        auto data = createClocklessChannelData(1, ledCounts[frame]);
        driver.enqueue(data);
        driver.show();
        FL_CHECK_EQ(mock.getDeinitCount(), frame);
        FL_CHECK_EQ(mock.getConfig().max_transfer_bytes, dmaBytes[frame]);
        mock.simulateTransmitComplete();
        FL_REQUIRE(driver.poll() == IChannelDriver::DriverState::READY);
        FL_CHECK_FALSE(data->isInUse());
    }
}

FL_TEST_CASE("ChannelDriverLcdClockless - failed bus recreation reports error and retries") {
    resetMockState();
    ChannelDriverLcdClockless driver(createMockPeripheral(false, 2));
    auto &mock = LcdSpiPeripheralMock::instance();
    auto first = createClocklessChannelData(1, 170);
    driver.enqueue(first);
    driver.show();
    mock.simulateTransmitComplete();
    FL_REQUIRE(driver.poll() == IChannelDriver::DriverState::READY);
    const auto completed = driver.workerDiagnostics();
    FL_REQUIRE_EQ(completed.submittedChunks, 1);
    FL_REQUIRE_EQ(completed.completedChunks, 1);
    FL_REQUIRE_EQ(completed.totalChunks, 1);

    auto larger = createClocklessChannelData(1, 700);
    driver.enqueue(larger);
    driver.show();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::ERROR);
    FL_CHECK_EQ(driver.poll().error,
                "LCD_CLOCKLESS: peripheral initialization failed");
    FL_CHECK_FALSE(larger->isInUse());
    const auto failed = driver.workerDiagnostics();
    FL_CHECK_EQ(failed.submittedChunks, 0);
    FL_CHECK_EQ(failed.completedChunks, 0);
    FL_CHECK_EQ(failed.totalChunks, 0);
    FL_CHECK_EQ(failed.core, completed.core);
    FL_CHECK_EQ(failed.priority, completed.priority);
    FL_CHECK_EQ(failed.maxEncodeUs, 0);
    FL_CHECK_EQ(failed.maxSubmitUs, 0);
    FL_CHECK_FALSE(failed.streamError);

    driver.enqueue(larger);
    driver.show();
    FL_CHECK_EQ(mock.getConfig().max_transfer_bytes, 33600);
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(larger->isInUse());
    const auto retried = driver.workerDiagnostics();
    FL_CHECK_EQ(retried.submittedChunks, 3);
    FL_CHECK_EQ(retried.completedChunks, 3);
    FL_CHECK_EQ(retried.totalChunks, 3);
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

FL_TEST_CASE("ChannelDriverLcdClockless - failed allocation reports error until successful retry") {
    resetMockState();
    ChannelDriverLcdClockless driver(createMockPeripheral(true));
    auto data = createClocklessChannelData(5, 5);
    driver.enqueue(data);
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::ERROR);
    FL_CHECK_EQ(driver.poll().error,
                "LCD_CLOCKLESS: DMA ring buffer allocation failed");
    FL_CHECK_FALSE(driver.waitForReady(1));
    FL_CHECK_FALSE(data->isInUse());
    FL_CHECK_EQ(LcdSpiPeripheralMock::instance().getTransmitCount(), 0);

    driver.enqueue(data);
    driver.show();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::DRAINING);
    FL_CHECK_TRUE(data->isInUse());
    LcdSpiPeripheralMock::instance().simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK_TRUE(driver.waitForReady(1));
    FL_CHECK_FALSE(data->isInUse());
}

FL_TEST_CASE("ChannelDriverLcdClockless - transmit failure") {
    resetMockState();
    auto peripheral = createMockPeripheral();
    ChannelDriverLcdClockless driver(peripheral);
    auto &mock = LcdSpiPeripheralMock::instance();

    mock.setTransmitFailure(true);

    auto data = createClocklessChannelData(5, 5);
    driver.enqueue(data);
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::ERROR);
    FL_CHECK_EQ(driver.poll().error, "LCD_CLOCKLESS: initial transmit failed");
    FL_CHECK_FALSE(driver.waitForReady(1));
    FL_CHECK_EQ(driver.workerDiagnostics().submittedChunks, 0);
    FL_CHECK_EQ(mock.getDeinitCount(), 1);
    FL_CHECK_FALSE(mock.isInitialized());
    FL_CHECK_FALSE(data->isInUse());

    mock.setTransmitFailure(false);
    driver.enqueue(data);
    driver.show();
    FL_CHECK_TRUE(mock.isInitialized());
    FL_CHECK_EQ(mock.getDeinitCount(), 1);
    FL_CHECK_EQ(mock.getTransmitCount(), 1);
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(data->isInUse());
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

    FL_CHECK(mock.getTransmitCount() == 2);

    mock.setTransmitFailure(true);
    mock.simulateTransmitComplete();
    FL_CHECK(mock.getTransmitCount() == 2);

    // ISR tried chunk 2 but transmit failed → abort
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK_TRUE(driver.workerDiagnostics().streamError);
    FL_CHECK_EQ(driver.workerDiagnostics().submittedChunks, 2);
    FL_CHECK_EQ(driver.workerDiagnostics().completedChunks, 2);

    mock.setTransmitFailure(false);
    driver.enqueue(data);
    driver.show();
    mock.simulateTransmitComplete();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(driver.workerDiagnostics().streamError);
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
