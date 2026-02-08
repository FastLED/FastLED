/// @file test_spi_channel_adapter.cpp
/// @brief Unit tests for SpiChannelEngineAdapter
///
/// Tests the SPI hardware controller adapter that wraps SpiHw1/2/4/8/16 controllers
/// for use with the modern ChannelBusManager API.

#include "test.h"
#include "FastLED.h"
#include "fl/channels/adapters/spi_channel_adapter.h"
#include "fl/channels/config.h"
#include "fl/channels/data.h"
#include "fl/chipsets/spi.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/shared/spi_hw_base.h"

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
        if (mode == TransmitMode::ASYNC) {
            mBusy = true;
        }
        return mTransmitReturnValue;
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
    void completeTransmission() { mBusy = false; }

    bool wasBeginCalled() const { return mBeginCalled; }
    bool wasTransmitCalled() const { return mTransmitCalled; }
    bool wasWaitCompleteCalled() const { return mWaitCompleteCalled; }
    int getTransmitCount() const { return mTransmitCount; }
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

    // Test tracking
    bool mBeginCalled = false;
    bool mEndCalled = false;
    bool mAcquireBufferCalled = false;
    bool mTransmitCalled = false;
    bool mWaitCompleteCalled = false;
    int mTransmitCount = 0;

    // Configurable return values
    bool mBeginReturnValue = true;
    bool mTransmitReturnValue = true;
    bool mWaitCompleteReturnValue = true;

    // State
    TransmitMode mTransmitMode = TransmitMode::ASYNC;
    fl::vector<uint8_t> mDmaBuffer;
};

/// @brief Create SPI channel data (APA102, SK9822, etc.)
ChannelDataPtr createSpiChannelData(int dataPin = 5, int clockPin = 18) {
    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiConfig{dataPin, clockPin, encoder};
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
    FL_CHECK_EQ(fl::string(adapter->getName()), fl::string("SPI_SINGLE"));
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

    FL_CHECK_EQ(adapter->poll(), IChannelEngine::EngineState::READY);
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
    FL_CHECK_EQ(adapter->poll(), IChannelEngine::EngineState::DRAINING);
}

FL_TEST_CASE("SpiChannelEngineAdapter - Enqueue null data is ignored") {
    auto spiHw1 = fl::make_shared<MockSpiHw>(1, "SPI2", 5);

    fl::vector<fl::shared_ptr<SpiHwBase>> controllers = {spiHw1};
    fl::vector<int> priorities = {5};
    fl::vector<const char*> names = {"SPI2"};

    auto adapter = SpiChannelEngineAdapter::create(controllers, priorities, names, "SPI_SINGLE");

    adapter->enqueue(nullptr);

    // Should still be READY
    FL_CHECK_EQ(adapter->poll(), IChannelEngine::EngineState::READY);
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
    FL_CHECK_EQ(adapter->poll(), IChannelEngine::EngineState::READY);
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
    FL_CHECK_EQ(adapter->poll(), IChannelEngine::EngineState::READY);

    // Verify transmission machinery was invoked
    FL_CHECK(spiHw1->wasTransmitCalled());
    FL_CHECK(spiHw1->wasWaitCompleteCalled());
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
    FL_CHECK_EQ(adapter->poll(), IChannelEngine::EngineState::READY);
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
    FL_CHECK_EQ(adapter->poll(), IChannelEngine::EngineState::DRAINING);
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
