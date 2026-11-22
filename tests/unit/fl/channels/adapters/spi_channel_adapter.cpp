/// @file test_spi_channel_adapter.cpp
/// @brief Unit tests for SpiChannelEngineAdapter
///
/// Tests the SPI hardware controller adapter that wraps SpiHw1/2/4/8/16 controllers
/// for use with the modern ChannelManager API.

#include "test.h"
#include "FastLED.h"
#include "fl/channels/adapters/spi_channel_adapter.h"
#include "fl/channels/config.h"
#include "fl/channels/data.h"
#include "fl/chipsets/spi.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/shared/spi_hw_base.h"
#include "platforms/shared/spi_hw_1.h"
#include "platforms/shared/spi_dma_stream.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

/// @brief Simple mock SPI hardware controller for testing
/// Simulates SpiHwBase without actual hardware transmission
class MockSpiHw : public SpiHwBase {
public:
    explicit MockSpiHw(uint8_t laneCount = 1, const char* name = "MOCK_SPI", int priority = 5)
        : mLaneCount(laneCount), mName(name) {
        (void)priority;  // Unused, just for API compatibility
    }

    // SpiHwBase interface
    bool begin(const void* config) override {
        mBeginCalled = true;
        mClockSpeedHz = static_cast<const SpiHw1::Config*>(config)->clock_speed_hz;
        mClockSpeeds.push_back(mClockSpeedHz);
        mInitialized = true;
        return mBeginReturnValue;
    }

    void end() override {
        mEndCalled = true;
        mInitialized = false;
    }

    DMABuffer acquireDMABuffer(size_t size) override {
        mAcquireBufferCalled = true;
        if (size > mDmaBuffer.size()) {
            mDmaBuffer.resize(size);
        }
        // Return a DMABuffer constructed with size
        return DMABuffer(size);
    }

    bool transmit(TransmitMode mode = TransmitMode::ASYNC) override {
        mTransmitCalled = true;
        mTransmitCount++;
        mTransmitMode = mode;
        if (mTransmitReturnValue && mode == TransmitMode::ASYNC) {
            mBusy = true;
        }
        return mTransmitReturnValue;
    }

    bool supportsStreaming() const override { return mStreaming; }

    bool transmitStream(const ChannelDataPtr& source) override {
        ++mStreamCount;
        if (!mTransmitReturnValue) return false;
        mStreamSource = source;
        mBusy = true;
        return true;
    }

    void setStreamCompletionCallback(void (*callback)(void*), void* context) override {
        mCompletion = callback;
        mCompletionContext = context;
    }

    bool waitComplete(uint32_t timeout_ms = fl::numeric_limits<uint32_t>::max()) override {
        (void)timeout_ms;
        mWaitCompleteCalled = true;
        mBusy = false;
        return mWaitCompleteReturnValue;
    }

    bool isBusy() const override {
        return mBusy;
    }

    bool isInitialized() const override {
        return mInitialized;
    }

    int getBusId() const override {
        return mBusId;
    }

    const char* getName() const override {
        return mName;
    }

    uint8_t getLaneCount() const override {
        return mLaneCount;
    }

    // Test accessors and configuration
    void setBusId(int id) { mBusId = id; }
    void setBeginReturnValue(bool value) { mBeginReturnValue = value; }
    void setTransmitReturnValue(bool value) { mTransmitReturnValue = value; }
    void setWaitCompleteReturnValue(bool value) { mWaitCompleteReturnValue = value; }
    void completeTransmission() {
        mBusy = false;
        mStreamSource.reset();
        if (mCompletion) mCompletion(mCompletionContext);
    }
    void setStreaming(bool value) { mStreaming = value; }
    int getStreamCount() const { return mStreamCount; }
    bool wasAcquireBufferCalled() const { return mAcquireBufferCalled; }
    bool hasCompletionCallback() const { return mCompletion != nullptr; }

    bool wasBeginCalled() const { return mBeginCalled; }
    bool wasTransmitCalled() const { return mTransmitCalled; }
    bool wasWaitCompleteCalled() const { return mWaitCompleteCalled; }
    int getTransmitCount() const { return mTransmitCount; }
    u32 getClockSpeedHz() const { return mClockSpeedHz; }
    const fl::vector<u32>& getClockSpeeds() const { return mClockSpeeds; }
    TransmitMode getLastTransmitMode() const { return mTransmitMode; }

    void reset() {
        mBeginCalled = false;
        mEndCalled = false;
        mAcquireBufferCalled = false;
        mTransmitCalled = false;
        mWaitCompleteCalled = false;
        mTransmitCount = 0;
        mBusy = false;
        mTransmitMode = TransmitMode::ASYNC;
    }

private:
    uint8_t mLaneCount;
    const char* mName;
    int mBusId = -1;
    bool mInitialized = false;
    bool mBusy = false;
    bool mStreaming = false;
    int mStreamCount = 0;
    ChannelDataPtr mStreamSource;
    void (*mCompletion)(void*) = nullptr;
    void* mCompletionContext = nullptr;

    // Test tracking
    bool mBeginCalled = false;
    bool mEndCalled = false;
    bool mAcquireBufferCalled = false;
    bool mTransmitCalled = false;
    bool mWaitCompleteCalled = false;
    int mTransmitCount = 0;
    u32 mClockSpeedHz = 0;
    fl::vector<u32> mClockSpeeds;

    // Configurable return values
    bool mBeginReturnValue = true;
    bool mTransmitReturnValue = true;
    bool mWaitCompleteReturnValue = true;

    // State
    TransmitMode mTransmitMode = TransmitMode::ASYNC;
    fl::vector<uint8_t> mDmaBuffer;
};

/// @brief Create SPI channel data (APA102, SK9822, etc.)
ChannelDataPtr createSpiChannelData(int dataPin = 5, int clockPin = 18,
                                    Esp32SpiBus spiBus = Esp32SpiBus::AUTO,
                                    u32 clockSpeedHz = 6000000) {
    SpiEncoder encoder = SpiEncoder::apa102(clockSpeedHz);
    SpiChipsetConfig spiConfig{dataPin, clockPin, encoder, spiBus};
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

// ============================================================================
// Factory Creation Tests
// ============================================================================

FL_TEST_CASE("SpiChannelEngineAdapter - Create with valid controllers") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    FL_CHECK(adapter != nullptr);
    FL_CHECK_EQ(adapter->getName(), fl::string("SPI_SINGLE"));
    FL_CHECK_EQ(adapter->getPriority(), 5);
}

FL_TEST_CASE("SpiChannelEngineAdapter - Create with empty controllers returns nullptr") {
    fl::vector<fl::shared_ptr<SpiHwBase>> controllers;
    fl::vector<int> priorities;
    fl::vector<const char*> names;

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_UNIFIED");

    FL_CHECK(adapter == nullptr);
}

FL_TEST_CASE("SpiChannelEngineAdapter - Create with mismatched vector sizes returns nullptr") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5, 9};  // Size mismatch
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_UNIFIED");

    FL_CHECK(adapter == nullptr);
}

FL_TEST_CASE("SpiChannelEngineAdapter - Create with null adapter name returns nullptr") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, nullptr);

    FL_CHECK(adapter == nullptr);
}

FL_TEST_CASE("SpiChannelEngineAdapter - Create with empty adapter name returns nullptr") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "");

    FL_CHECK(adapter == nullptr);
}

FL_TEST_CASE("SpiChannelEngineAdapter - Create with null controller skips it") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {nullptr, spiHw1};
    fl::vector<int> priorities = {9, 5};
    fl::vector<const char*> names = {"NULL", "SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_UNIFIED");

    // Should create adapter with only the valid controller
    FL_CHECK(adapter != nullptr);
    FL_CHECK_EQ(adapter->getPriority(), 5);  // Only SPI2 registered
}

FL_TEST_CASE("SpiChannelEngineAdapter - Create with all null controllers returns nullptr") {
    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {nullptr, nullptr};
    fl::vector<int> priorities = {9, 5};
    fl::vector<const char*> names = {"NULL1", "NULL2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_UNIFIED");

    FL_CHECK(adapter == nullptr);
}

// ============================================================================
// Chipset Routing Tests (CRITICAL)
// ============================================================================

FL_TEST_CASE("SpiChannelEngineAdapter - canHandle accepts APA102 (true SPI)") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    // Create APA102 channel data
    auto data = createSpiChannelData(5, 18);

    FL_CHECK(adapter->canHandle(data));
}

FL_TEST_CASE("SpiChannelEngineAdapter - canHandle accepts SK9822 (true SPI)") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    // Create SK9822 channel data
    SpiEncoder encoder = SpiEncoder::sk9822();
    SpiChipsetConfig spiConfig{5, 18, encoder};
    fl::vector_psram<uint8_t> channelData = {0x00, 0xFF};
    auto data = ChannelData::create(spiConfig, fl::move(channelData));

    FL_CHECK(adapter->canHandle(data));
}

FL_TEST_CASE("SpiChannelEngineAdapter - canHandle rejects WS2812 (clockless)") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    // Create WS2812 channel data
    auto data = createClocklessChannelData(5);

    FL_CHECK_FALSE(adapter->canHandle(data));
}

FL_TEST_CASE("SpiChannelEngineAdapter - canHandle rejects null channel data") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    FL_CHECK_FALSE(adapter->canHandle(nullptr));
}

// ============================================================================
// Priority Tests
// ============================================================================

FL_TEST_CASE("SpiChannelEngineAdapter - getPriority returns highest priority") {
    auto spiHw16 = fl::make_shared<MockSpiHw>(16, "SPI_HEXADECA", 9);
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1, spiHw16};
    fl::vector<int> priorities = {5, 9};
    fl::vector<const char*> names = {"SPI2", "SPI_HEXADECA"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_UNIFIED");

    FL_CHECK_EQ(adapter->getPriority(), 9);  // Should return highest (SpiHw16)
}

FL_TEST_CASE("SpiChannelEngineAdapter - getPriority with single controller") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    FL_CHECK_EQ(adapter->getPriority(), 5);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

FL_TEST_CASE("SpiChannelEngineAdapter - Initial state is READY") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("SpiChannelEngineAdapter - Enqueue adds to enqueued list") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    auto data = createSpiChannelData(5, 18);
    adapter->enqueue(data);

    // Should return DRAINING (data enqueued but not transmitted yet)
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::DRAINING);
}

FL_TEST_CASE("SpiChannelEngineAdapter - Enqueue null data is ignored") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    adapter->enqueue(nullptr);

    // Should still be READY
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("SpiChannelEngineAdapter - Enqueue non-SPI data is rejected") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    auto data = createClocklessChannelData(5);  // WS2812 data
    adapter->enqueue(data);

    // Should still be READY (data rejected)
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("SpiChannelEngineAdapter - Show triggers transmission") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);
    spiHw1->setBusId(2);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    auto data = createSpiChannelData(5, 18);
    adapter->enqueue(data);
    adapter->show();

    // Mock SPI controller should have been called
    FL_CHECK(spiHw1->wasTransmitCalled());
}

FL_TEST_CASE("SpiChannelEngineAdapter - Transmission completes synchronously") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);
    spiHw1->setBusId(2);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    auto data = createSpiChannelData(5, 18);
    adapter->enqueue(data);
    adapter->show();

    // Current implementation is synchronous - show() calls waitComplete() before returning
    // Note: transmit() sets mBusy=true, but waitComplete() clears it immediately
    // TODO: When async support is added, this test should verify BUSY state
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::READY);

    // Verify transmission machinery was invoked
    FL_CHECK(spiHw1->wasTransmitCalled());
    FL_CHECK(spiHw1->wasWaitCompleteCalled());
}

FL_TEST_CASE("SpiChannelEngineAdapter - streaming is asynchronous and preserves channel lifetime #3179") {
    auto hw = fl::make_shared<MockSpiHw>(1, "SPI2");
    hw->setBusId(2);
    hw->setStreaming(true);
    auto adapter = SpiChannelEngineAdapter::create({hw}, {5}, {"SPI2"}, "SPI");
    auto first = createSpiChannelData(5, 18, Esp32SpiBus::HOST2);
    auto second = createSpiChannelData(5, 18, Esp32SpiBus::HOST2);
    adapter->enqueue(first);
    adapter->enqueue(second);
    adapter->show();

    FL_CHECK_EQ(hw->getStreamCount(), 1);
    FL_CHECK_FALSE(hw->wasAcquireBufferCalled());
    FL_CHECK_FALSE(hw->wasWaitCompleteCalled());
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::BUSY);
    FL_CHECK_TRUE(first->isInUse());

    hw->completeTransmission();
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::BUSY);
    FL_CHECK_EQ(hw->getStreamCount(), 2);
    FL_CHECK_FALSE(first->isInUse());
    FL_CHECK_TRUE(second->isInUse());
    hw->completeTransmission();
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(second->isInUse());
}

FL_TEST_CASE("SpiChannelEngineAdapter - failed streaming submission releases channel and permits retry #3179") {
    auto hw = fl::make_shared<MockSpiHw>(1, "SPI2");
    hw->setBusId(2);
    hw->setStreaming(true);
    hw->setTransmitReturnValue(false);
    auto adapter = SpiChannelEngineAdapter::create({hw}, {5}, {"SPI2"}, "SPI");
    auto data = createSpiChannelData(5, 18, Esp32SpiBus::HOST2);
    adapter->enqueue(data);
    adapter->show();
    FL_CHECK_EQ(hw->getStreamCount(), 1);
    FL_CHECK_FALSE(hw->wasAcquireBufferCalled());
    FL_CHECK_FALSE(data->isInUse());
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::READY);

    // A new frame retries the backend rather than permanently poisoning it.
    hw->setTransmitReturnValue(true);
    adapter->enqueue(data);
    adapter->show();
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::BUSY);
    hw->completeTransmission();
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("SPI DMA streaming preserves an encoded frame across every chunk boundary #3179") {
    // Includes exact chunks, final single-byte tails, the 1000-pixel APA102
    // workload, and the native backend's maximum frame size.
    for (size_t bytes : {size_t(1), size_t(511), size_t(512), size_t(513),
                         size_t(4068), size_t(65536)}) {
        fl::vector<u8> frame(bytes);
        for (size_t i = 0; i < bytes; ++i) frame[i] = static_cast<u8>((i * 37) ^ (i >> 8));
        fl::vector<u8> received;
        u8 staging[detail::SpiDmaStreamPlan::kChunkBytes] = {};
        const size_t count = detail::SpiDmaStreamPlan::chunkCount(bytes);
        for (size_t index = 0; index < count; ++index) {
            FL_REQUIRE(detail::SpiDmaStreamPlan::copyChunk(frame, index, staging));
            const size_t chunk = detail::SpiDmaStreamPlan::chunkSize(bytes, index);
            received.insert(received.end(), staging, staging + chunk);
        }
        FL_CHECK_EQ(received, frame);
        FL_CHECK_FALSE(detail::SpiDmaStreamPlan::copyChunk(frame, count, staging));
    }
    FL_CHECK_EQ(detail::SpiDmaStreamPlan::chunkCount(0), 0);
    FL_CHECK_EQ(detail::SpiDmaStreamPlan::chunkCount(65537), 0);
    const u8 frame[8] = {};
    u8 tooSmall[7] = {};
    FL_CHECK_FALSE(detail::SpiDmaStreamPlan::copyChunk(frame, 0, tooSmall));
}

FL_TEST_CASE("SpiChannelEngineAdapter - streaming completion follows manager callback replacement #3179") {
    auto hw = fl::make_shared<MockSpiHw>(1, "SPI2");
    hw->setBusId(2);
    hw->setStreaming(true);
    auto adapter = SpiChannelEngineAdapter::create({hw}, {5}, {"SPI2"}, "SPI");
    fl::i32 first = 0;
    fl::i32 second = 0;
    auto notify = [](void* context) { ++*static_cast<fl::i32*>(context); };
    adapter->setPollNeededCallback({notify, &first});
    adapter->enqueue(createSpiChannelData(5, 18, Esp32SpiBus::HOST2));
    adapter->show();
    adapter->setPollNeededCallback({notify, &second});
    hw->completeTransmission();
    FL_CHECK_EQ(first, 0);
    FL_CHECK_EQ(second, 1);
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::READY);

    adapter->enqueue(createSpiChannelData(5, 18, Esp32SpiBus::HOST2));
    adapter->show();
    adapter->setPollNeededCallback({});
    hw->completeTransmission();
    FL_CHECK_EQ(second, 1);
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::READY);
    adapter.reset();
    FL_CHECK_FALSE(hw->hasCompletionCallback());
}

FL_TEST_CASE("SpiChannelEngineAdapter - Poll returns READY after transmission completes") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);
    spiHw1->setBusId(2);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    auto data = createSpiChannelData(5, 18);
    adapter->enqueue(data);
    adapter->show();

    // Complete transmission
    spiHw1->completeTransmission();

    // Should return READY
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("SpiChannelEngineAdapter - Poll releases channel after completion") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);
    spiHw1->setBusId(2);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    auto data = createSpiChannelData(5, 18);
    adapter->enqueue(data);

    // Data should not be in use yet
    FL_CHECK_FALSE(data->isInUse());

    adapter->show();

    // Data should be marked in use during transmission
    FL_CHECK(data->isInUse());

    // Complete transmission
    spiHw1->completeTransmission();
    adapter->poll();

    // Data should be released
    FL_CHECK_FALSE(data->isInUse());
}

FL_TEST_CASE("SpiChannelEngineAdapter - Poll returns DRAINING when enqueued but not transmitting") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);
    spiHw1->setBusId(2);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    // Enqueue first batch
    auto data1 = createSpiChannelData(5, 18);
    adapter->enqueue(data1);
    adapter->show();

    // Enqueue second batch while first is transmitting
    auto data2 = createSpiChannelData(5, 18);
    adapter->enqueue(data2);

    // Complete first transmission
    spiHw1->completeTransmission();

    // Should return DRAINING (second batch enqueued but not shown)
    FL_CHECK_EQ(adapter->poll(), IChannelDriver::DriverState::DRAINING);
}

FL_TEST_CASE("SpiChannelEngineAdapter - Multiple channels same clock pin") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);
    spiHw1->setBusId(2);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    // Create two channels with same clock pin
    auto data1 = createSpiChannelData(5, 18);
    auto data2 = createSpiChannelData(23, 18);  // Same clock pin 18

    adapter->enqueue(data1);
    adapter->enqueue(data2);
    adapter->show();

    // Both channels should be transmitted (batched together)
    // Mock transmit called once per channel in batch
    FL_CHECK_GT(spiHw1->getTransmitCount(), 0);
}

FL_TEST_CASE("SpiChannelEngineAdapter - explicit SPI host selects matching controller") {
    auto spi2 = fl::make_shared<MockSpiHw>(1, "SPI2", 10);
    auto spi3 = fl::make_shared<MockSpiHw>(1, "SPI3", 5);
    spi2->setBusId(2);
    spi3->setBusId(3);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spi2, spi3};
    fl::vector<int> priorities = {10, 5};
    fl::vector<const char*> names = {"SPI2", "SPI3"};
    auto adapter = SpiChannelEngineAdapter::create(
        controllers, priorities, names, "SPI_UNIFIED");

    adapter->enqueue(createSpiChannelData(5, 18, Esp32SpiBus::HOST3));
    adapter->show();

    FL_CHECK_FALSE(spi2->wasTransmitCalled());
    FL_CHECK_TRUE(spi3->wasTransmitCalled());
}

FL_TEST_CASE("SpiChannelEngineAdapter - distinct SPI hosts work in either registration order") {
    for (bool host3First : {false, true}) {
        auto spi2 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);
        auto spi3 = fl::make_shared<MockSpiHw>(1, "SPI3", 5);
        spi2->setBusId(2);
        spi3->setBusId(3);

        fl::vector<fl::shared_ptr<SpiHwBase>> controllers = host3First
            ? fl::vector<fl::shared_ptr<SpiHwBase>>{spi3, spi2}
            : fl::vector<fl::shared_ptr<SpiHwBase>>{spi2, spi3};
        fl::vector<int> priorities = {5, 5};
        fl::vector<const char*> names = host3First
            ? fl::vector<const char*>{"SPI3", "SPI2"}
            : fl::vector<const char*>{"SPI2", "SPI3"};
        auto adapter = SpiChannelEngineAdapter::create(
            controllers, priorities, names, "SPI_UNIFIED");

        auto stripA = createSpiChannelData(1, 3, Esp32SpiBus::HOST2, 8000000);
        auto stripB = createSpiChannelData(5, 7, Esp32SpiBus::HOST3, 12000000);
        adapter->enqueue(host3First ? stripB : stripA);
        adapter->enqueue(host3First ? stripA : stripB);
        adapter->show();

        FL_CHECK_EQ(spi2->getTransmitCount(), 1);
        FL_CHECK_EQ(spi3->getTransmitCount(), 1);
        FL_CHECK_EQ(spi2->getClockSpeedHz(), 8000000);
        FL_CHECK_EQ(spi3->getClockSpeedHz(), 12000000);
    }
}

FL_TEST_CASE("SpiChannelEngineAdapter - reconfigures explicit host between rates") {
    auto spi2 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);
    spi2->setBusId(2);
    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spi2};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};
    auto adapter = SpiChannelEngineAdapter::create(
        controllers, priorities, names, "SPI_UNIFIED");

    adapter->enqueue(createSpiChannelData(1, 3, Esp32SpiBus::HOST2, 8000000));
    adapter->enqueue(createSpiChannelData(1, 3, Esp32SpiBus::HOST2, 12000000));
    adapter->enqueue(createSpiChannelData(1, 3, Esp32SpiBus::HOST2, 8000000));
    adapter->show();

    FL_CHECK_EQ(spi2->getTransmitCount(), 3);
    FL_CHECK_EQ(spi2->getClockSpeeds().size(), 3);
    FL_CHECK_EQ(spi2->getClockSpeeds()[0], 8000000);
    FL_CHECK_EQ(spi2->getClockSpeeds()[1], 12000000);
    FL_CHECK_EQ(spi2->getClockSpeeds()[2], 8000000);
}

FL_TEST_CASE("SpiChannelEngineAdapter - AUTO preserves controller priority") {
    auto preferred = fl::make_shared<MockSpiHw>(1, "preferred", 10);
    auto fallback = fl::make_shared<MockSpiHw>(1, "fallback", 5);
    preferred->setBusId(2);
    fallback->setBusId(3);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {preferred, fallback};
    fl::vector<int> priorities = {10, 5};
    fl::vector<const char*> names = {"preferred", "fallback"};
    auto adapter = SpiChannelEngineAdapter::create(
        controllers, priorities, names, "SPI_UNIFIED");

    adapter->enqueue(createSpiChannelData());
    adapter->show();

    FL_CHECK_TRUE(preferred->wasTransmitCalled());
    FL_CHECK_FALSE(fallback->wasTransmitCalled());
}

} // FL_TEST_FILE
