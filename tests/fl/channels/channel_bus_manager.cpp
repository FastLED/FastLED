/// @file channel_bus_manager.cpp
/// @brief Tests for ChannelBusManager priority-based engine selection

#include "test.h"
#include "fl/channels/channel_bus_manager.h"
#include "fl/channels/channel_data.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "ftl/shared_ptr.h"
#include "ftl/vector.h"
#include "ftl/move.h"
#include "fl/dbg.h"

namespace channel_bus_manager_test {

using namespace fl;

/// Simple fake engine for testing - no mocks needed
/// Tracks transmission calls without actually transmitting
class FakeEngine : public IChannelEngine {
public:
    FakeEngine(const char* name, bool shouldFail = false)
        : mName(name), mShouldFail(shouldFail) {
    }

    ~FakeEngine() override {
    }

    // Test accessors
    int getTransmitCount() const { return mTransmitCount; }
    int getLastChannelCount() const { return mLastChannelCount; }
    const char* getName() const { return mName; }
    void reset() { mTransmitCount = 0; mLastChannelCount = 0; }
    void setShouldFail(bool shouldFail) { mShouldFail = shouldFail; }

    void enqueue(ChannelDataPtr channelData) override {
        if (channelData) {
            mEnqueuedChannels.push_back(channelData);
        }
    }

    void show() override {
        if (!mEnqueuedChannels.empty()) {
            mTransmittingChannels = fl::move(mEnqueuedChannels);
            mEnqueuedChannels.clear();
            beginTransmission(fl::span<const ChannelDataPtr>(mTransmittingChannels.data(), mTransmittingChannels.size()));
        }
    }

    EngineState poll() override {
        if (mShouldFail) {
            return EngineState::ERROR;
        }
        // Fake implementation: always return READY (transmission completes instantly)
        if (!mTransmittingChannels.empty()) {
            mTransmittingChannels.clear();
        }
        return EngineState::READY;
    }

private:
    void beginTransmission(fl::span<const ChannelDataPtr> channels) {
        mTransmitCount++;
        mLastChannelCount = static_cast<int>(channels.size());

        if (mShouldFail) {
            mLastError = fl::string("Engine ") + mName + " failed";
        }
    }

    const char* mName;
    bool mShouldFail;
    int mTransmitCount = 0;
    int mLastChannelCount = 0;
    fl::string mLastError;
    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;
};

/// Helper to create dummy channel data
ChannelDataPtr createDummyChannelData(int pin = 1) {
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    fl::vector_psram<uint8_t> data;
    data.push_back(0xFF);
    data.push_back(0x00);
    return ChannelData::create(pin, timing, fl::move(data));
}

TEST_CASE("ChannelBusManager - Basic initialization") {
    ChannelBusManager manager;

    // Should be in READY state with no engines
    CHECK(manager.poll() == IChannelEngine::EngineState::READY);
}

TEST_CASE("ChannelBusManager - Add single engine") {
    ChannelBusManager manager;
    auto engine = fl::make_shared<FakeEngine>("Engine1");

    manager.addEngine(100, engine);

    auto channelData = createDummyChannelData();
    manager.enqueue(channelData);

    // Poll before show to ensure clean state
    CHECK(manager.poll() == IChannelEngine::EngineState::READY);

    // Now test actual transmission
    manager.show();

    // Poll after show - should still be READY (fake engine completes instantly)
    CHECK(manager.poll() == IChannelEngine::EngineState::READY);

    // Verify engine was actually used
    CHECK(engine->getTransmitCount() == 1);
}

TEST_CASE("ChannelBusManager - Priority selection (highest priority)") {
    ChannelBusManager manager;

    // Add engines in mixed priority order
    auto lowEngine = fl::make_shared<FakeEngine>("LowPriority");
    auto highEngine = fl::make_shared<FakeEngine>("HighPriority");
    auto midEngine = fl::make_shared<FakeEngine>("MidPriority");

    manager.addEngine(10, lowEngine);
    manager.addEngine(100, highEngine);
    manager.addEngine(50, midEngine);

    auto channelData = createDummyChannelData();
    manager.enqueue(channelData);
    manager.show();

    // Verify highest priority engine was used
    CHECK(highEngine->getTransmitCount() == 1);
    CHECK(midEngine->getTransmitCount() == 0);
    CHECK(lowEngine->getTransmitCount() == 0);
}

TEST_CASE("ChannelBusManager - Multiple channels in one frame") {
    ChannelBusManager manager;
    auto engine = fl::make_shared<FakeEngine>("TestEngine");

    manager.addEngine(100, engine);

    // Enqueue multiple channel data
    manager.enqueue(createDummyChannelData(1));
    manager.enqueue(createDummyChannelData(2));
    manager.enqueue(createDummyChannelData(3));

    manager.show();

    // Should batch all channels into one transmission
    CHECK(engine->getTransmitCount() == 1);
    CHECK(engine->getLastChannelCount() == 3);
}

TEST_CASE("ChannelBusManager - Frame reset") {
    ChannelBusManager manager;
    auto highEngine = fl::make_shared<FakeEngine>("HighPriority");
    auto lowEngine = fl::make_shared<FakeEngine>("LowPriority");

    manager.addEngine(100, highEngine);
    manager.addEngine(50, lowEngine);

    // First frame
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(highEngine->getTransmitCount() == 1);

    // Simulate frame end event
    manager.onEndFrame();

    // Second frame - should still use high priority engine
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(highEngine->getTransmitCount() == 2);
    CHECK(lowEngine->getTransmitCount() == 0);
}

TEST_CASE("ChannelBusManager - No engines available") {
    ChannelBusManager manager;

    // Try to enqueue without any engines
    auto channelData = createDummyChannelData();
    manager.enqueue(channelData);
    manager.show();

    // Should not crash - manager handles gracefully
    CHECK(manager.poll() == IChannelEngine::EngineState::READY);
}

TEST_CASE("ChannelBusManager - Null engine ignored") {
    ChannelBusManager manager;

    // Add null engine - should be ignored
    manager.addEngine(100, fl::shared_ptr<IChannelEngine>(nullptr));

    auto channelData = createDummyChannelData();
    manager.enqueue(channelData);
    manager.show();

    // Should handle gracefully (no crash)
    CHECK(manager.poll() == IChannelEngine::EngineState::READY);
}

TEST_CASE("ChannelBusManager - Poll forwards to active engine") {
    ChannelBusManager manager;
    auto engine = fl::make_shared<FakeEngine>("TestEngine");

    manager.addEngine(100, engine);

    // Before any enqueue, should be READY
    CHECK(manager.poll() == IChannelEngine::EngineState::READY);

    // After enqueue and show, should still be READY (fake engine returns READY)
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(manager.poll() == IChannelEngine::EngineState::READY);
}

TEST_CASE("ChannelBusManager - Multiple frames with same engine") {
    ChannelBusManager manager;
    auto engine = fl::make_shared<FakeEngine>("TestEngine");

    manager.addEngine(100, engine);

    // Frame 1
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(engine->getTransmitCount() == 1);

    // Frame 2
    manager.onEndFrame();
    manager.enqueue(createDummyChannelData());
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(engine->getTransmitCount() == 2);
    CHECK(engine->getLastChannelCount() == 2);

    // Frame 3
    manager.onEndFrame();
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(engine->getTransmitCount() == 3);
    CHECK(engine->getLastChannelCount() == 1);
}

TEST_CASE("ChannelBusManager - Priority ordering with equal priorities") {
    ChannelBusManager manager;
    auto engine1 = fl::make_shared<FakeEngine>("Engine1");
    auto engine2 = fl::make_shared<FakeEngine>("Engine2");
    auto engine3 = fl::make_shared<FakeEngine>("Engine3");

    // Add engines with same priority - first added should win
    manager.addEngine(100, engine1);
    manager.addEngine(100, engine2);
    manager.addEngine(100, engine3);

    manager.enqueue(createDummyChannelData());
    manager.show();

    // One of them should be selected (stable sort should pick first)
    int totalTransmits = engine1->getTransmitCount() +
                         engine2->getTransmitCount() +
                         engine3->getTransmitCount();
    CHECK(totalTransmits == 1);
}

TEST_CASE("ChannelBusManager - Empty show() does nothing") {
    ChannelBusManager manager;
    auto engine = fl::make_shared<FakeEngine>("TestEngine");

    manager.addEngine(100, engine);

    // Call show() without enqueuing anything
    manager.show();

    // Should not transmit
    CHECK(engine->getTransmitCount() == 0);
}

} // namespace channel_bus_manager_test
