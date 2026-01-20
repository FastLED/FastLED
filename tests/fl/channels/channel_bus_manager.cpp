/// @file channel_bus_manager.cpp
/// @brief Tests for ChannelBusManager priority-based engine selection

#include "fl/channels/bus_manager.h"
#include "fl/channels/data.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/channels/engine.h"
#include "fl/chipsets/led_timing.h"
#include "fl/slice.h"
#include "fl/stl/allocator.h"
#include "fl/stl/string.h"

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
    const char* getName() const override { return mName; }
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

TEST_CASE("ChannelBusManager - Driver enable/disable") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");

    // By default, all engines should be enabled
    CHECK(manager.isDriverEnabled("RMT") == true);
    CHECK(manager.isDriverEnabled("SPI") == true);

    // SPI should be selected (higher priority)
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(spiEngine->getTransmitCount() == 1);
    CHECK(rmtEngine->getTransmitCount() == 0);

    // Reset for next test
    spiEngine->reset();
    rmtEngine->reset();
    manager.onEndFrame();

    // Disable SPI - should fall back to RMT
    manager.setDriverEnabled("SPI", false);
    CHECK(manager.isDriverEnabled("SPI") == false);
    CHECK(manager.isDriverEnabled("RMT") == true);

    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(rmtEngine->getTransmitCount() == 1);
    CHECK(spiEngine->getTransmitCount() == 0);

    // Reset for next test
    spiEngine->reset();
    rmtEngine->reset();
    manager.onEndFrame();

    // Re-enable SPI - should go back to SPI
    manager.setDriverEnabled("SPI", true);
    CHECK(manager.isDriverEnabled("SPI") == true);

    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(spiEngine->getTransmitCount() == 1);
    CHECK(rmtEngine->getTransmitCount() == 0);
}

TEST_CASE("ChannelBusManager - Disable all drivers") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");

    // Disable both engines
    manager.setDriverEnabled("RMT", false);
    manager.setDriverEnabled("SPI", false);

    // Try to transmit - should handle gracefully
    manager.enqueue(createDummyChannelData());
    manager.show();

    // Neither engine should be used
    CHECK(rmtEngine->getTransmitCount() == 0);
    CHECK(spiEngine->getTransmitCount() == 0);

    // Should still be in READY state (no crash)
    CHECK(manager.poll() == IChannelEngine::EngineState::READY);
}

TEST_CASE("ChannelBusManager - Multiple engines with same name") {
    ChannelBusManager manager;
    auto rmt1 = fl::make_shared<FakeEngine>("RMT1");
    auto rmt2 = fl::make_shared<FakeEngine>("RMT2");

    manager.addEngine(100, rmt1, "RMT");
    manager.addEngine(50, rmt2, "RMT");

    // Disable RMT name - should disable BOTH engines with that name
    manager.setDriverEnabled("RMT", false);

    manager.enqueue(createDummyChannelData());
    manager.show();

    CHECK(rmt1->getTransmitCount() == 0);
    CHECK(rmt2->getTransmitCount() == 0);

    // Re-enable RMT - should use highest priority RMT engine
    manager.setDriverEnabled("RMT", true);

    manager.enqueue(createDummyChannelData());
    manager.show();

    CHECK(rmt1->getTransmitCount() == 1);  // Higher priority
    CHECK(rmt2->getTransmitCount() == 0);
}

TEST_CASE("ChannelBusManager - Query non-existent driver name") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");

    manager.addEngine(10, rmtEngine, "RMT");

    // Query PARLIO when only RMT is registered
    CHECK(manager.isDriverEnabled("PARLIO") == false);

    // Disable PARLIO (even though it doesn't exist) - should not crash
    manager.setDriverEnabled("PARLIO", false);

    // RMT should still work
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(rmtEngine->getTransmitCount() == 1);
}

TEST_CASE("ChannelBusManager - Immediate effect of setDriverEnabled") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");

    // First transmission - should use SPI
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(spiEngine->getTransmitCount() == 1);

    spiEngine->reset();
    rmtEngine->reset();

    // Disable SPI mid-frame (without calling onEndFrame)
    manager.setDriverEnabled("SPI", false);

    // Next transmission should immediately use RMT (no onEndFrame needed)
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(rmtEngine->getTransmitCount() == 1);
    CHECK(spiEngine->getTransmitCount() == 0);
}

TEST_CASE("ChannelBusManager - Query driver info") {
    ChannelBusManager manager;

    // Empty manager
    CHECK(manager.getDriverCount() == 0);
    auto emptyInfo = manager.getDriverInfos();
    CHECK(emptyInfo.size() == 0);

    // Add named engines
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");
    auto parlioEngine = fl::make_shared<FakeEngine>("PARLIO");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");
    manager.addEngine(100, parlioEngine, "PARLIO");

    // Check count
    CHECK(manager.getDriverCount() == 3);

    // Get info (returns span, no allocation!)
    auto info = manager.getDriverInfos();
    CHECK(info.size() == 3);

    // Verify all names are present (sorted by priority descending)
    bool hasRMT = false;
    bool hasSPI = false;
    bool hasPARLIO = false;

    for (const auto& p : info) {
        if (p.name == "RMT") hasRMT = true;
        if (p.name == "SPI") hasSPI = true;
        if (p.name == "PARLIO") hasPARLIO = true;
    }

    CHECK(hasRMT == true);
    CHECK(hasSPI == true);
    CHECK(hasPARLIO == true);
}

TEST_CASE("ChannelBusManager - Query with unnamed engines") {
    ChannelBusManager manager;

    auto namedEngine = fl::make_shared<FakeEngine>("Named");
    auto unnamedEngine = fl::make_shared<FakeEngine>("Unnamed");

    manager.addEngine(10, namedEngine, "Named");
    manager.addEngine(20, unnamedEngine, nullptr);  // Unnamed

    // Count includes both
    CHECK(manager.getDriverCount() == 2);

    // Info includes both (unnamed has empty string)
    auto info = manager.getDriverInfos();
    CHECK(info.size() == 2);

    // Higher priority first (20 > 10)
    CHECK(info[0].priority == 20);
    CHECK(info[0].name == "");  // Unnamed

    CHECK(info[1].priority == 10);
    CHECK(info[1].name == "Named");
}

TEST_CASE("ChannelBusManager - Query with duplicate names") {
    ChannelBusManager manager;

    auto rmt1 = fl::make_shared<FakeEngine>("RMT1");
    auto rmt2 = fl::make_shared<FakeEngine>("RMT2");

    manager.addEngine(100, rmt1, "RMT");
    manager.addEngine(50, rmt2, "RMT");

    // Count should be 2 (two engines)
    CHECK(manager.getDriverCount() == 2);

    // Info should include both "RMT" entries (duplicates allowed)
    auto info = manager.getDriverInfos();
    CHECK(info.size() == 2);
    CHECK(info[0].name == "RMT");
    CHECK(info[0].priority == 100);
    CHECK(info[1].name == "RMT");
    CHECK(info[1].priority == 50);
}

TEST_CASE("ChannelBusManager - Query full driver state") {
    ChannelBusManager manager;

    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");
    auto parlioEngine = fl::make_shared<FakeEngine>("PARLIO");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");
    manager.addEngine(100, parlioEngine, "PARLIO");

    // Get full info (span, no allocation!)
    auto info = manager.getDriverInfos();
    CHECK(info.size() == 3);

    // Should be sorted by priority descending (PARLIO=100, SPI=50, RMT=10)
    CHECK(info[0].name == "PARLIO");
    CHECK(info[0].priority == 100);
    CHECK(info[0].enabled == true);

    CHECK(info[1].name == "SPI");
    CHECK(info[1].priority == 50);
    CHECK(info[1].enabled == true);

    CHECK(info[2].name == "RMT");
    CHECK(info[2].priority == 10);
    CHECK(info[2].enabled == true);

    // Disable SPI and check state
    manager.setDriverEnabled("SPI", false);
    info = manager.getDriverInfos();

    CHECK(info[0].enabled == true);   // PARLIO still enabled
    CHECK(info[1].enabled == false);  // SPI disabled
    CHECK(info[2].enabled == true);   // RMT still enabled
}

TEST_CASE("ChannelBusManager - Span validity") {
    ChannelBusManager manager;

    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");

    // Get span (no allocation)
    auto info = manager.getDriverInfos();
    CHECK(info.size() == 2);

    // Verify we can iterate multiple times (span is stable)
    int count = 0;
    for (const auto& p : info) {
        count++;
        CHECK(p.priority > 0);
    }
    CHECK(count == 2);

    // Get span again - should work fine
    auto info2 = manager.getDriverInfos();
    CHECK(info2.size() == 2);
    CHECK(info2[0].name == "SPI");  // Higher priority
    CHECK(info2[1].name == "RMT");
}

TEST_CASE("ChannelBusManager - setExclusiveDriver with valid name") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");
    auto parlioEngine = fl::make_shared<FakeEngine>("PARLIO");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");
    manager.addEngine(100, parlioEngine, "PARLIO");

    // All drivers should be enabled by default
    CHECK(manager.isDriverEnabled("RMT") == true);
    CHECK(manager.isDriverEnabled("SPI") == true);
    CHECK(manager.isDriverEnabled("PARLIO") == true);

    // Set SPI as exclusive driver
    bool result = manager.setExclusiveDriver("SPI");
    CHECK(result == true);

    // Only SPI should be enabled
    CHECK(manager.isDriverEnabled("SPI") == true);
    CHECK(manager.isDriverEnabled("RMT") == false);
    CHECK(manager.isDriverEnabled("PARLIO") == false);

    // Verify SPI is actually used
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(spiEngine->getTransmitCount() == 1);
    CHECK(rmtEngine->getTransmitCount() == 0);
    CHECK(parlioEngine->getTransmitCount() == 0);
}

TEST_CASE("ChannelBusManager - setExclusiveDriver with invalid name") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");

    // Try to set non-existent driver as exclusive
    bool result = manager.setExclusiveDriver("NONEXISTENT");
    CHECK(result == false);

    // All drivers should be disabled (defensive behavior)
    CHECK(manager.isDriverEnabled("RMT") == false);
    CHECK(manager.isDriverEnabled("SPI") == false);

    // No transmission should occur
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(rmtEngine->getTransmitCount() == 0);
    CHECK(spiEngine->getTransmitCount() == 0);
}

TEST_CASE("ChannelBusManager - setExclusiveDriver with nullptr") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");

    // nullptr should disable all drivers
    bool result = manager.setExclusiveDriver(nullptr);
    CHECK(result == false);

    // All drivers should be disabled
    CHECK(manager.isDriverEnabled("RMT") == false);
    CHECK(manager.isDriverEnabled("SPI") == false);

    // No transmission should occur
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(rmtEngine->getTransmitCount() == 0);
    CHECK(spiEngine->getTransmitCount() == 0);
}

TEST_CASE("ChannelBusManager - setExclusiveDriver with empty string") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");

    // Empty string should disable all drivers
    bool result = manager.setExclusiveDriver("");
    CHECK(result == false);

    // All drivers should be disabled
    CHECK(manager.isDriverEnabled("RMT") == false);
    CHECK(manager.isDriverEnabled("SPI") == false);
}

TEST_CASE("ChannelBusManager - setExclusiveDriver forward compatibility") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");

    // Set RMT as exclusive
    manager.setExclusiveDriver("RMT");
    CHECK(manager.isDriverEnabled("RMT") == true);
    CHECK(manager.isDriverEnabled("SPI") == false);

    // Simulate adding a new driver (future scenario)
    auto parlioEngine = fl::make_shared<FakeEngine>("PARLIO");
    manager.addEngine(100, parlioEngine, "PARLIO");

    // New driver should be auto-disabled (not matching "RMT")
    CHECK(manager.isDriverEnabled("PARLIO") == false);
    CHECK(manager.isDriverEnabled("RMT") == true);

    // Only RMT should be used (not the new higher-priority PARLIO)
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(rmtEngine->getTransmitCount() == 1);
    CHECK(spiEngine->getTransmitCount() == 0);
    CHECK(parlioEngine->getTransmitCount() == 0);
}

TEST_CASE("ChannelBusManager - setExclusiveDriver immediate effect") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");

    // First transmission - should use SPI (higher priority)
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(spiEngine->getTransmitCount() == 1);

    spiEngine->reset();
    rmtEngine->reset();

    // Set RMT as exclusive mid-frame (without calling onEndFrame)
    manager.setExclusiveDriver("RMT");

    // Next transmission should immediately use RMT (no onEndFrame needed)
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(rmtEngine->getTransmitCount() == 1);
    CHECK(spiEngine->getTransmitCount() == 0);
}

TEST_CASE("ChannelBusManager - setExclusiveDriver with duplicate names") {
    ChannelBusManager manager;
    auto rmt1 = fl::make_shared<FakeEngine>("RMT1");
    auto rmt2 = fl::make_shared<FakeEngine>("RMT2");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(100, rmt1, "RMT");
    manager.addEngine(50, rmt2, "RMT");
    manager.addEngine(25, spiEngine, "SPI");

    // Set RMT as exclusive - should enable BOTH engines with name "RMT"
    manager.setExclusiveDriver("RMT");

    CHECK(manager.isDriverEnabled("RMT") == true);
    CHECK(manager.isDriverEnabled("SPI") == false);

    // Should use highest priority RMT engine (priority 100)
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(rmt1->getTransmitCount() == 1);
    CHECK(rmt2->getTransmitCount() == 0);
    CHECK(spiEngine->getTransmitCount() == 0);
}

TEST_CASE("ChannelBusManager - setExclusiveDriver switch between drivers") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");
    auto parlioEngine = fl::make_shared<FakeEngine>("PARLIO");

    manager.addEngine(10, rmtEngine, "RMT");
    manager.addEngine(50, spiEngine, "SPI");
    manager.addEngine(100, parlioEngine, "PARLIO");

    // Test 1: Set RMT exclusive
    manager.setExclusiveDriver("RMT");
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(rmtEngine->getTransmitCount() == 1);
    CHECK(spiEngine->getTransmitCount() == 0);
    CHECK(parlioEngine->getTransmitCount() == 0);

    // Reset counters
    rmtEngine->reset();
    spiEngine->reset();
    parlioEngine->reset();

    // Test 2: Switch to SPI exclusive
    manager.setExclusiveDriver("SPI");
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(rmtEngine->getTransmitCount() == 0);
    CHECK(spiEngine->getTransmitCount() == 1);
    CHECK(parlioEngine->getTransmitCount() == 0);

    // Reset counters
    rmtEngine->reset();
    spiEngine->reset();
    parlioEngine->reset();

    // Test 3: Switch to PARLIO exclusive
    manager.setExclusiveDriver("PARLIO");
    manager.enqueue(createDummyChannelData());
    manager.show();
    CHECK(rmtEngine->getTransmitCount() == 0);
    CHECK(spiEngine->getTransmitCount() == 0);
    CHECK(parlioEngine->getTransmitCount() == 1);
}

} // namespace channel_bus_manager_test
