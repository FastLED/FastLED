/// @file channel_bus_manager.cpp
/// @brief Tests for ChannelBusManager priority-based engine selection

#include "fl/channels/bus_manager.h"
#include "fl/channels/data.h"
#include "fl/channels/config.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/spi.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/channels/engine.h"
#include "fl/chipsets/led_timing.h"
#include "fl/slice.h"
#include "fl/stl/allocator.h"
#include "fl/stl/string.h"
#include "fl/stl/cstdio.h"

namespace channel_bus_manager_test {

using namespace fl;

// Test helper for capturing debug output
namespace test_helper {
    static fl::string captured_output;

    void capture_print(const char* str) {
        captured_output += str;
    }

    void clear_capture() {
        captured_output.clear();
    }

    fl::string get_capture() {
        return captured_output;
    }
}

/// Simple fake engine for testing - no mocks needed
/// Tracks transmission calls without actually transmitting
class FakeEngine : public IChannelEngine {
public:
    FakeEngine(const char* name, bool shouldFail = false,
               bool supportsClockless = true, bool supportsSpi = false)
        : mName(name), mShouldFail(shouldFail),
          mCapabilities(supportsClockless, supportsSpi) {
    }

    ~FakeEngine() override {
    }

    // Test accessors
    int getTransmitCount() const { return mTransmitCount; }
    int getLastChannelCount() const { return mLastChannelCount; }
    fl::string getName() const override { return mName; }
    Capabilities getCapabilities() const override { return mCapabilities; }
    void reset() { mTransmitCount = 0; mLastChannelCount = 0; }
    void setShouldFail(bool shouldFail) { mShouldFail = shouldFail; }

    bool canHandle(const ChannelDataPtr& data) const override {
        (void)data;
        return true;  // Test engine accepts all channel types
    }

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
    Capabilities mCapabilities;
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

FL_TEST_CASE("ChannelBusManager - Basic initialization") {
    ChannelBusManager manager;

    // Should be in READY state with no engines
    FL_CHECK(manager.poll() == IChannelEngine::EngineState::READY);
}

FL_TEST_CASE("ChannelBusManager - Add single engine") {
    ChannelBusManager manager;
    auto engine = fl::make_shared<FakeEngine>("Engine1");

    manager.addEngine(100, engine);

    auto channelData = createDummyChannelData();
    manager.enqueue(channelData);

    // Poll before show to ensure clean state
    FL_CHECK(manager.poll() == IChannelEngine::EngineState::READY);

    // Now test actual transmission
    manager.show();

    // Poll after show - should still be READY (fake engine completes instantly)
    FL_CHECK(manager.poll() == IChannelEngine::EngineState::READY);

    // Verify engine was actually used
    FL_CHECK(engine->getTransmitCount() == 1);
}

FL_TEST_CASE("ChannelBusManager - Priority selection (highest priority)") {
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
    FL_CHECK(highEngine->getTransmitCount() == 1);
    FL_CHECK(midEngine->getTransmitCount() == 0);
    FL_CHECK(lowEngine->getTransmitCount() == 0);
}

FL_TEST_CASE("ChannelBusManager - Multiple channels in one frame") {
    ChannelBusManager manager;
    auto engine = fl::make_shared<FakeEngine>("TestEngine");

    manager.addEngine(100, engine);

    // Enqueue multiple channel data
    manager.enqueue(createDummyChannelData(1));
    manager.enqueue(createDummyChannelData(2));
    manager.enqueue(createDummyChannelData(3));

    manager.show();

    // Should batch all channels into one transmission
    FL_CHECK(engine->getTransmitCount() == 1);
    FL_CHECK(engine->getLastChannelCount() == 3);
}

FL_TEST_CASE("ChannelBusManager - Frame reset") {
    ChannelBusManager manager;
    auto highEngine = fl::make_shared<FakeEngine>("HighPriority");
    auto lowEngine = fl::make_shared<FakeEngine>("LowPriority");

    manager.addEngine(100, highEngine);
    manager.addEngine(50, lowEngine);

    // First frame
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(highEngine->getTransmitCount() == 1);

    // Simulate frame end event
    manager.onEndFrame();

    // Second frame - should still use high priority engine
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(highEngine->getTransmitCount() == 2);
    FL_CHECK(lowEngine->getTransmitCount() == 0);
}

FL_TEST_CASE("ChannelBusManager - No engines available") {
    ChannelBusManager manager;

    // Try to enqueue without any engines
    auto channelData = createDummyChannelData();
    manager.enqueue(channelData);
    manager.show();

    // Should not crash - manager handles gracefully
    FL_CHECK(manager.poll() == IChannelEngine::EngineState::READY);
}

FL_TEST_CASE("ChannelBusManager - Null engine ignored") {
    ChannelBusManager manager;

    // Add null engine - should be ignored
    manager.addEngine(100, fl::shared_ptr<IChannelEngine>(nullptr));

    auto channelData = createDummyChannelData();
    manager.enqueue(channelData);
    manager.show();

    // Should handle gracefully (no crash)
    FL_CHECK(manager.poll() == IChannelEngine::EngineState::READY);
}

FL_TEST_CASE("ChannelBusManager - Poll forwards to active engine") {
    ChannelBusManager manager;
    auto engine = fl::make_shared<FakeEngine>("TestEngine");

    manager.addEngine(100, engine);

    // Before any enqueue, should be READY
    FL_CHECK(manager.poll() == IChannelEngine::EngineState::READY);

    // After enqueue and show, should still be READY (fake engine returns READY)
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(manager.poll() == IChannelEngine::EngineState::READY);
}

FL_TEST_CASE("ChannelBusManager - Multiple frames with same engine") {
    ChannelBusManager manager;
    auto engine = fl::make_shared<FakeEngine>("TestEngine");

    manager.addEngine(100, engine);

    // Frame 1
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(engine->getTransmitCount() == 1);

    // Frame 2
    manager.onEndFrame();
    manager.enqueue(createDummyChannelData());
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(engine->getTransmitCount() == 2);
    FL_CHECK(engine->getLastChannelCount() == 2);

    // Frame 3
    manager.onEndFrame();
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(engine->getTransmitCount() == 3);
    FL_CHECK(engine->getLastChannelCount() == 1);
}

FL_TEST_CASE("ChannelBusManager - Priority ordering with equal priorities") {
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
    FL_CHECK(totalTransmits == 1);
}

FL_TEST_CASE("ChannelBusManager - Empty show() does nothing") {
    ChannelBusManager manager;
    auto engine = fl::make_shared<FakeEngine>("TestEngine");

    manager.addEngine(100, engine);

    // Call show() without enqueuing anything
    manager.show();

    // Should not transmit
    FL_CHECK(engine->getTransmitCount() == 0);
}

FL_TEST_CASE("ChannelBusManager - Driver enable/disable") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);

    // By default, all engines should be enabled
    FL_CHECK(manager.isDriverEnabled("RMT") == true);
    FL_CHECK(manager.isDriverEnabled("SPI") == true);

    // SPI should be selected (higher priority)
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(spiEngine->getTransmitCount() == 1);
    FL_CHECK(rmtEngine->getTransmitCount() == 0);

    // Reset for next test
    spiEngine->reset();
    rmtEngine->reset();
    manager.onEndFrame();

    // Disable SPI - should fall back to RMT
    manager.setDriverEnabled("SPI", false);
    FL_CHECK(manager.isDriverEnabled("SPI") == false);
    FL_CHECK(manager.isDriverEnabled("RMT") == true);

    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(rmtEngine->getTransmitCount() == 1);
    FL_CHECK(spiEngine->getTransmitCount() == 0);

    // Reset for next test
    spiEngine->reset();
    rmtEngine->reset();
    manager.onEndFrame();

    // Re-enable SPI - should go back to SPI
    manager.setDriverEnabled("SPI", true);
    FL_CHECK(manager.isDriverEnabled("SPI") == true);

    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(spiEngine->getTransmitCount() == 1);
    FL_CHECK(rmtEngine->getTransmitCount() == 0);
}

FL_TEST_CASE("ChannelBusManager - Disable all drivers") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);

    // Disable both engines
    manager.setDriverEnabled("RMT", false);
    manager.setDriverEnabled("SPI", false);

    // Try to transmit - should handle gracefully
    manager.enqueue(createDummyChannelData());
    manager.show();

    // Neither engine should be used
    FL_CHECK(rmtEngine->getTransmitCount() == 0);
    FL_CHECK(spiEngine->getTransmitCount() == 0);

    // Should still be in READY state (no crash)
    FL_CHECK(manager.poll() == IChannelEngine::EngineState::READY);
}

FL_TEST_CASE("ChannelBusManager - Replacement engine can be disabled/enabled") {
    ChannelBusManager manager;
    // Both engines have the SAME name from getName()
    auto rmt1 = fl::make_shared<FakeEngine>("RMT");
    auto rmt2 = fl::make_shared<FakeEngine>("RMT");

    manager.addEngine(100, rmt1);
    manager.addEngine(50, rmt2);  // Replaces rmt1 (same name)

    // Disable RMT name - should disable the replacement engine (rmt2)
    manager.setDriverEnabled("RMT", false);

    manager.enqueue(createDummyChannelData());
    manager.show();

    FL_CHECK(rmt1->getTransmitCount() == 0);  // rmt1 was replaced
    FL_CHECK(rmt2->getTransmitCount() == 0);  // rmt2 is disabled

    // Re-enable RMT - should use the replacement engine (rmt2)
    manager.setDriverEnabled("RMT", true);

    manager.enqueue(createDummyChannelData());
    manager.show();

    FL_CHECK(rmt1->getTransmitCount() == 0);  // Still replaced
    FL_CHECK(rmt2->getTransmitCount() == 1);  // rmt2 is the active engine
}

FL_TEST_CASE("ChannelBusManager - Query non-existent driver name") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");

    manager.addEngine(10, rmtEngine);

    // Query PARLIO when only RMT is registered
    FL_CHECK(manager.isDriverEnabled("PARLIO") == false);

    // Disable PARLIO (even though it doesn't exist) - should not crash
    manager.setDriverEnabled("PARLIO", false);

    // RMT should still work
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(rmtEngine->getTransmitCount() == 1);
}

FL_TEST_CASE("ChannelBusManager - Immediate effect of setDriverEnabled") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);

    // First transmission - should use SPI
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(spiEngine->getTransmitCount() == 1);

    spiEngine->reset();
    rmtEngine->reset();

    // Disable SPI mid-frame (without calling onEndFrame)
    manager.setDriverEnabled("SPI", false);

    // Next transmission should immediately use RMT (no onEndFrame needed)
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(rmtEngine->getTransmitCount() == 1);
    FL_CHECK(spiEngine->getTransmitCount() == 0);
}

FL_TEST_CASE("ChannelBusManager - Query driver info") {
    ChannelBusManager manager;

    // Empty manager
    FL_CHECK(manager.getDriverCount() == 0);
    auto emptyInfo = manager.getDriverInfos();
    FL_CHECK(emptyInfo.size() == 0);

    // Add named engines
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");
    auto parlioEngine = fl::make_shared<FakeEngine>("PARLIO");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);
    manager.addEngine(100, parlioEngine);

    // Check count
    FL_CHECK(manager.getDriverCount() == 3);

    // Get info (returns span, no allocation!)
    auto info = manager.getDriverInfos();
    FL_CHECK(info.size() == 3);

    // Verify all names are present (sorted by priority descending)
    bool hasRMT = false;
    bool hasSPI = false;
    bool hasPARLIO = false;

    for (const auto& p : info) {
        if (p.name == "RMT") hasRMT = true;
        if (p.name == "SPI") hasSPI = true;
        if (p.name == "PARLIO") hasPARLIO = true;
    }

    FL_CHECK(hasRMT == true);
    FL_CHECK(hasSPI == true);
    FL_CHECK(hasPARLIO == true);
}

FL_TEST_CASE("ChannelBusManager - Query with unnamed engines rejected") {
    ChannelBusManager manager;

    auto namedEngine = fl::make_shared<FakeEngine>("Named");
    auto unnamedEngine = fl::make_shared<FakeEngine>("");  // Empty name from getName()

    manager.addEngine(10, namedEngine);
    manager.addEngine(20, unnamedEngine);  // Rejected (empty getName())

    // Count should be 1 (unnamed engine was rejected)
    FL_CHECK(manager.getDriverCount() == 1);

    // Info includes only the named engine
    auto info = manager.getDriverInfos();
    FL_CHECK(info.size() == 1);

    FL_CHECK(info[0].priority == 10);
    FL_CHECK(info[0].name == "Named");
}

FL_TEST_CASE("ChannelBusManager - Duplicate names cause replacement") {
    ChannelBusManager manager;

    // Both engines have the SAME name from getName()
    auto rmt1 = fl::make_shared<FakeEngine>("RMT");
    auto rmt2 = fl::make_shared<FakeEngine>("RMT");

    manager.addEngine(100, rmt1);
    manager.addEngine(50, rmt2);  // Replaces first engine (same name)

    // Count should be 1 (second engine replaced the first)
    FL_CHECK(manager.getDriverCount() == 1);

    // Info should include only the replacement engine
    auto info = manager.getDriverInfos();
    FL_CHECK(info.size() == 1);
    FL_CHECK(info[0].name == "RMT");
    FL_CHECK(info[0].priority == 50);  // Second engine's priority

    // Retrieved engine should be rmt2, not rmt1
    IChannelEngine* retrieved = manager.getEngineByName("RMT");
    FL_CHECK(retrieved == rmt2.get());
}

FL_TEST_CASE("ChannelBusManager - Query full driver state") {
    ChannelBusManager manager;

    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");
    auto parlioEngine = fl::make_shared<FakeEngine>("PARLIO");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);
    manager.addEngine(100, parlioEngine);

    // Get full info (span, no allocation!)
    auto info = manager.getDriverInfos();
    FL_CHECK(info.size() == 3);

    // Should be sorted by priority descending (PARLIO=100, SPI=50, RMT=10)
    FL_CHECK(info[0].name == "PARLIO");
    FL_CHECK(info[0].priority == 100);
    FL_CHECK(info[0].enabled == true);

    FL_CHECK(info[1].name == "SPI");
    FL_CHECK(info[1].priority == 50);
    FL_CHECK(info[1].enabled == true);

    FL_CHECK(info[2].name == "RMT");
    FL_CHECK(info[2].priority == 10);
    FL_CHECK(info[2].enabled == true);

    // Disable SPI and check state
    manager.setDriverEnabled("SPI", false);
    info = manager.getDriverInfos();

    FL_CHECK(info[0].enabled == true);   // PARLIO still enabled
    FL_CHECK(info[1].enabled == false);  // SPI disabled
    FL_CHECK(info[2].enabled == true);   // RMT still enabled
}

FL_TEST_CASE("ChannelBusManager - Span validity") {
    ChannelBusManager manager;

    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);

    // Get span (no allocation)
    auto info = manager.getDriverInfos();
    FL_CHECK(info.size() == 2);

    // Verify we can iterate multiple times (span is stable)
    int count = 0;
    for (const auto& p : info) {
        count++;
        FL_CHECK(p.priority > 0);
    }
    FL_CHECK(count == 2);

    // Get span again - should work fine
    auto info2 = manager.getDriverInfos();
    FL_CHECK(info2.size() == 2);
    FL_CHECK(info2[0].name == "SPI");  // Higher priority
    FL_CHECK(info2[1].name == "RMT");
}

FL_TEST_CASE("ChannelBusManager - setExclusiveDriver with valid name") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");
    auto parlioEngine = fl::make_shared<FakeEngine>("PARLIO");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);
    manager.addEngine(100, parlioEngine);

    // All drivers should be enabled by default
    FL_CHECK(manager.isDriverEnabled("RMT") == true);
    FL_CHECK(manager.isDriverEnabled("SPI") == true);
    FL_CHECK(manager.isDriverEnabled("PARLIO") == true);

    // Set SPI as exclusive driver
    bool result = manager.setExclusiveDriver("SPI");
    FL_CHECK(result == true);

    // Only SPI should be enabled
    FL_CHECK(manager.isDriverEnabled("SPI") == true);
    FL_CHECK(manager.isDriverEnabled("RMT") == false);
    FL_CHECK(manager.isDriverEnabled("PARLIO") == false);

    // Verify SPI is actually used
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(spiEngine->getTransmitCount() == 1);
    FL_CHECK(rmtEngine->getTransmitCount() == 0);
    FL_CHECK(parlioEngine->getTransmitCount() == 0);
}

FL_TEST_CASE("ChannelBusManager - setExclusiveDriver with invalid name") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);

    // Try to set non-existent driver as exclusive
    bool result = manager.setExclusiveDriver("NONEXISTENT");
    FL_CHECK(result == false);

    // All drivers should be disabled (defensive behavior)
    FL_CHECK(manager.isDriverEnabled("RMT") == false);
    FL_CHECK(manager.isDriverEnabled("SPI") == false);

    // No transmission should occur
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(rmtEngine->getTransmitCount() == 0);
    FL_CHECK(spiEngine->getTransmitCount() == 0);
}

FL_TEST_CASE("ChannelBusManager - setExclusiveDriver with nullptr") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);

    // nullptr should disable all drivers
    bool result = manager.setExclusiveDriver(nullptr);
    FL_CHECK(result == false);

    // All drivers should be disabled
    FL_CHECK(manager.isDriverEnabled("RMT") == false);
    FL_CHECK(manager.isDriverEnabled("SPI") == false);

    // No transmission should occur
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(rmtEngine->getTransmitCount() == 0);
    FL_CHECK(spiEngine->getTransmitCount() == 0);
}

FL_TEST_CASE("ChannelBusManager - setExclusiveDriver with empty string") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);

    // Empty string should disable all drivers
    bool result = manager.setExclusiveDriver("");
    FL_CHECK(result == false);

    // All drivers should be disabled
    FL_CHECK(manager.isDriverEnabled("RMT") == false);
    FL_CHECK(manager.isDriverEnabled("SPI") == false);
}

FL_TEST_CASE("ChannelBusManager - setExclusiveDriver forward compatibility") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);

    // Set RMT as exclusive
    manager.setExclusiveDriver("RMT");
    FL_CHECK(manager.isDriverEnabled("RMT") == true);
    FL_CHECK(manager.isDriverEnabled("SPI") == false);

    // Simulate adding a new driver (future scenario)
    auto parlioEngine = fl::make_shared<FakeEngine>("PARLIO");
    manager.addEngine(100, parlioEngine);

    // New driver should be auto-disabled (not matching "RMT")
    FL_CHECK(manager.isDriverEnabled("PARLIO") == false);
    FL_CHECK(manager.isDriverEnabled("RMT") == true);

    // Only RMT should be used (not the new higher-priority PARLIO)
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(rmtEngine->getTransmitCount() == 1);
    FL_CHECK(spiEngine->getTransmitCount() == 0);
    FL_CHECK(parlioEngine->getTransmitCount() == 0);
}

FL_TEST_CASE("ChannelBusManager - setExclusiveDriver immediate effect") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);

    // First transmission - should use SPI (higher priority)
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(spiEngine->getTransmitCount() == 1);

    spiEngine->reset();
    rmtEngine->reset();

    // Set RMT as exclusive mid-frame (without calling onEndFrame)
    manager.setExclusiveDriver("RMT");

    // Next transmission should immediately use RMT (no onEndFrame needed)
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(rmtEngine->getTransmitCount() == 1);
    FL_CHECK(spiEngine->getTransmitCount() == 0);
}

FL_TEST_CASE("ChannelBusManager - setExclusiveDriver with replaced engine") {
    ChannelBusManager manager;
    // Both RMT engines have the SAME name from getName()
    auto rmt1 = fl::make_shared<FakeEngine>("RMT");
    auto rmt2 = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");

    manager.addEngine(100, rmt1);
    manager.addEngine(50, rmt2);  // Replaces rmt1 (same name)
    manager.addEngine(25, spiEngine);

    // Set RMT as exclusive - should enable only the replacement RMT engine (rmt2)
    manager.setExclusiveDriver("RMT");

    FL_CHECK(manager.isDriverEnabled("RMT") == true);
    FL_CHECK(manager.isDriverEnabled("SPI") == false);

    // Should use the replacement RMT engine (rmt2, priority 50)
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(rmt1->getTransmitCount() == 0);  // rmt1 was replaced
    FL_CHECK(rmt2->getTransmitCount() == 1);  // rmt2 is the active engine
    FL_CHECK(spiEngine->getTransmitCount() == 0);
}

FL_TEST_CASE("ChannelBusManager - setExclusiveDriver switch between drivers") {
    ChannelBusManager manager;
    auto rmtEngine = fl::make_shared<FakeEngine>("RMT");
    auto spiEngine = fl::make_shared<FakeEngine>("SPI");
    auto parlioEngine = fl::make_shared<FakeEngine>("PARLIO");

    manager.addEngine(10, rmtEngine);
    manager.addEngine(50, spiEngine);
    manager.addEngine(100, parlioEngine);

    // Test 1: Set RMT exclusive
    manager.setExclusiveDriver("RMT");
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(rmtEngine->getTransmitCount() == 1);
    FL_CHECK(spiEngine->getTransmitCount() == 0);
    FL_CHECK(parlioEngine->getTransmitCount() == 0);

    // Reset counters
    rmtEngine->reset();
    spiEngine->reset();
    parlioEngine->reset();

    // Test 2: Switch to SPI exclusive
    manager.setExclusiveDriver("SPI");
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(rmtEngine->getTransmitCount() == 0);
    FL_CHECK(spiEngine->getTransmitCount() == 1);
    FL_CHECK(parlioEngine->getTransmitCount() == 0);

    // Reset counters
    rmtEngine->reset();
    spiEngine->reset();
    parlioEngine->reset();

    // Test 3: Switch to PARLIO exclusive
    manager.setExclusiveDriver("PARLIO");
    manager.enqueue(createDummyChannelData());
    manager.show();
    FL_CHECK(rmtEngine->getTransmitCount() == 0);
    FL_CHECK(spiEngine->getTransmitCount() == 0);
    FL_CHECK(parlioEngine->getTransmitCount() == 1);
}

// ============================================================================
// SPI Routing Integration Tests
// ============================================================================
// Tests for correct routing between SpiChannelEngineAdapter (true SPI chipsets)
// and ChannelEngineSpi (clockless-over-SPI chipsets)

/// @brief Mock engine that accepts only SPI chipsets (mimics SpiChannelEngineAdapter)
class FakeSpiHardwareEngine : public IChannelEngine {
public:
    FakeSpiHardwareEngine(const char* name, int priority)
        : mName(name) {
        (void)priority;  // Unused, just for API compatibility
    }

    ~FakeSpiHardwareEngine() override {}

    Capabilities getCapabilities() const override {
        return Capabilities(false, true);  // SPI only
    }

    bool canHandle(const ChannelDataPtr& data) const override {
        if (!data) {
            return false;
        }
        // Accept ONLY true SPI chipsets (APA102, SK9822)
        return data->isSpi();
    }

    void enqueue(ChannelDataPtr channelData) override {
        if (channelData) {
            mEnqueuedChannels.push_back(channelData);
        }
    }

    void show() override {
        if (!mEnqueuedChannels.empty()) {
            mTransmitCount++;
            mLastChannelCount = static_cast<int>(mEnqueuedChannels.size());
            mEnqueuedChannels.clear();
        }
    }

    EngineState poll() override {
        return EngineState::READY;
    }

    fl::string getName() const override { return mName; }
    int getTransmitCount() const { return mTransmitCount; }
    int getLastChannelCount() const { return mLastChannelCount; }
    void reset() { mTransmitCount = 0; mLastChannelCount = 0; }

private:
    const char* mName;
    int mTransmitCount = 0;
    int mLastChannelCount = 0;
    fl::vector<ChannelDataPtr> mEnqueuedChannels;
};

/// @brief Mock engine that accepts only clockless chipsets (mimics ChannelEngineSpi)
class FakeClocklessEngine : public IChannelEngine {
public:
    FakeClocklessEngine(const char* name, int priority)
        : mName(name) {
        (void)priority;  // Unused, just for API compatibility
    }

    ~FakeClocklessEngine() override {}

    Capabilities getCapabilities() const override {
        return Capabilities(true, false);  // Clockless only
    }

    bool canHandle(const ChannelDataPtr& data) const override {
        if (!data) {
            return false;
        }
        // Accept ONLY clockless chipsets (WS2812, SK6812)
        // Reject true SPI chipsets (APA102, SK9822)
        return !data->isSpi();
    }

    void enqueue(ChannelDataPtr channelData) override {
        if (channelData) {
            mEnqueuedChannels.push_back(channelData);
        }
    }

    void show() override {
        if (!mEnqueuedChannels.empty()) {
            mTransmitCount++;
            mLastChannelCount = static_cast<int>(mEnqueuedChannels.size());
            mEnqueuedChannels.clear();
        }
    }

    EngineState poll() override {
        return EngineState::READY;
    }

    fl::string getName() const override { return mName; }
    int getTransmitCount() const { return mTransmitCount; }
    int getLastChannelCount() const { return mLastChannelCount; }
    void reset() { mTransmitCount = 0; mLastChannelCount = 0; }

private:
    const char* mName;
    int mTransmitCount = 0;
    int mLastChannelCount = 0;
    fl::vector<ChannelDataPtr> mEnqueuedChannels;
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

FL_TEST_CASE("ChannelBusManager - APA102 routes to HW SPI adapter (priority 9)") {
    ChannelBusManager manager;

    // Register HW SPI adapter (priority 9) and clockless engine (priority 2)
    auto hwSpiEngine = fl::make_shared<FakeSpiHardwareEngine>("HW_SPI", 9);
    auto clocklessEngine = fl::make_shared<FakeClocklessEngine>("CLOCKLESS_SPI", 2);

    manager.addEngine(9, hwSpiEngine);
    manager.addEngine(2, clocklessEngine);

    // Create APA102 channel data
    auto data = createSpiChannelData(5, 18);

    manager.enqueue(data);
    manager.show();

    // Verify APA102 routed to HW SPI adapter
    FL_CHECK_EQ(hwSpiEngine->getTransmitCount(), 1);
    FL_CHECK_EQ(clocklessEngine->getTransmitCount(), 0);
}

FL_TEST_CASE("ChannelBusManager - WS2812 routes to clockless engine (priority 2)") {
    ChannelBusManager manager;

    // Register HW SPI adapter (priority 9) and clockless engine (priority 2)
    auto hwSpiEngine = fl::make_shared<FakeSpiHardwareEngine>("HW_SPI", 9);
    auto clocklessEngine = fl::make_shared<FakeClocklessEngine>("CLOCKLESS_SPI", 2);

    manager.addEngine(9, hwSpiEngine);
    manager.addEngine(2, clocklessEngine);

    // Create WS2812 channel data
    auto data = createClocklessChannelData(5);

    manager.enqueue(data);
    manager.show();

    // Verify WS2812 routed to clockless engine
    FL_CHECK_EQ(hwSpiEngine->getTransmitCount(), 0);
    FL_CHECK_EQ(clocklessEngine->getTransmitCount(), 1);
}

FL_TEST_CASE("ChannelBusManager - Mixed APA102 and WS2812 in separate frames") {
    ChannelBusManager manager;

    // Register HW SPI adapter (priority 9) and clockless engine (priority 2)
    auto hwSpiEngine = fl::make_shared<FakeSpiHardwareEngine>("HW_SPI", 9);
    auto clocklessEngine = fl::make_shared<FakeClocklessEngine>("CLOCKLESS_SPI", 2);

    manager.addEngine(9, hwSpiEngine);
    manager.addEngine(2, clocklessEngine);

    // Frame 1: APA102
    auto apa102 = createSpiChannelData(5, 18);
    manager.enqueue(apa102);
    manager.show();

    // Verify APA102 routed to HW SPI
    FL_CHECK_EQ(hwSpiEngine->getTransmitCount(), 1);
    FL_CHECK_EQ(clocklessEngine->getTransmitCount(), 0);

    // Reset for frame 2
    hwSpiEngine->reset();
    clocklessEngine->reset();
    manager.onEndFrame();

    // Frame 2: WS2812
    auto ws2812 = createClocklessChannelData(6);
    manager.enqueue(ws2812);
    manager.show();

    // Verify WS2812 routed to clockless engine
    FL_CHECK_EQ(hwSpiEngine->getTransmitCount(), 0);
    FL_CHECK_EQ(clocklessEngine->getTransmitCount(), 1);
}

FL_TEST_CASE("ChannelBusManager - Priority ordering ensures HW SPI first") {
    ChannelBusManager manager;

    // Register in WRONG order (clockless before HW SPI)
    auto clocklessEngine = fl::make_shared<FakeClocklessEngine>("CLOCKLESS_SPI", 2);
    manager.addEngine(2, clocklessEngine);

    auto hwSpiEngine = fl::make_shared<FakeSpiHardwareEngine>("HW_SPI", 9);
    manager.addEngine(9, hwSpiEngine);

    // Create APA102 channel data
    auto data = createSpiChannelData(5, 18);

    manager.enqueue(data);
    manager.show();

    // Verify APA102 still routes to HW SPI despite registration order
    FL_CHECK_EQ(hwSpiEngine->getTransmitCount(), 1);
    FL_CHECK_EQ(clocklessEngine->getTransmitCount(), 0);
}

FL_TEST_CASE("ChannelBusManager - SK9822 routes to HW SPI adapter") {
    ChannelBusManager manager;

    auto hwSpiEngine = fl::make_shared<FakeSpiHardwareEngine>("HW_SPI", 9);
    auto clocklessEngine = fl::make_shared<FakeClocklessEngine>("CLOCKLESS_SPI", 2);

    manager.addEngine(9, hwSpiEngine);
    manager.addEngine(2, clocklessEngine);

    // Create SK9822 channel data
    SpiEncoder encoder = SpiEncoder::sk9822();
    SpiChipsetConfig spiConfig{5, 18, encoder};
    fl::vector_psram<uint8_t> channelData = {0x00, 0xFF};
    auto data = ChannelData::create(spiConfig, fl::move(channelData));

    manager.enqueue(data);
    manager.show();

    // Verify SK9822 routed to HW SPI adapter
    FL_CHECK_EQ(hwSpiEngine->getTransmitCount(), 1);
    FL_CHECK_EQ(clocklessEngine->getTransmitCount(), 0);
}

FL_TEST_CASE("ChannelBusManager - SK6812 routes to clockless engine") {
    ChannelBusManager manager;

    auto hwSpiEngine = fl::make_shared<FakeSpiHardwareEngine>("HW_SPI", 9);
    auto clocklessEngine = fl::make_shared<FakeClocklessEngine>("CLOCKLESS_SPI", 2);

    manager.addEngine(9, hwSpiEngine);
    manager.addEngine(2, clocklessEngine);

    // Create SK6812 channel data
    auto timing = makeTimingConfig<TIMING_SK6812>();
    fl::vector_psram<uint8_t> channelData = {0xFF, 0x00, 0xAA};
    auto data = ChannelData::create(5, timing, fl::move(channelData));

    manager.enqueue(data);
    manager.show();

    // Verify SK6812 routed to clockless engine
    FL_CHECK_EQ(hwSpiEngine->getTransmitCount(), 0);
    FL_CHECK_EQ(clocklessEngine->getTransmitCount(), 1);
}

FL_TEST_CASE("ChannelBusManager - Capability logging") {
    // Setup output capture (FL_DBG uses println)
    fl::inject_println_handler(test_helper::capture_print);
    test_helper::clear_capture();

    ChannelBusManager manager;

    // Add engines with different capabilities
    auto clocklessEngine = fl::make_shared<FakeEngine>("RMT", false, true, false);  // Clockless only
    auto spiEngine = fl::make_shared<FakeEngine>("HW_SPI", false, false, true);     // SPI only
    auto bothEngine = fl::make_shared<FakeEngine>("BOTH", false, true, true);       // Both

    manager.addEngine(10, clocklessEngine);
    manager.addEngine(50, spiEngine);
    manager.addEngine(100, bothEngine);

    // Get captured output
    fl::string output = test_helper::get_capture();

    // Verify capability strings appear in output
    // RMT should show CLOCKLESS capability
    FL_SUBCASE("RMT shows CLOCKLESS") {
        FL_CHECK(output.find("RMT") != fl::string::npos);
        FL_CHECK(output.find("caps: CLOCKLESS") != fl::string::npos);
    }

    // HW_SPI should show SPI capability
    FL_SUBCASE("HW_SPI shows SPI") {
        FL_CHECK(output.find("HW_SPI") != fl::string::npos);
        FL_CHECK(output.find("caps: SPI") != fl::string::npos);
    }

    // BOTH should show CLOCKLESS|SPI capabilities
    FL_SUBCASE("BOTH shows CLOCKLESS|SPI") {
        FL_CHECK(output.find("BOTH") != fl::string::npos);
        FL_CHECK(output.find("caps: CLOCKLESS|SPI") != fl::string::npos);
    }

    // Cleanup
    fl::clear_io_handlers();
}

FL_TEST_CASE("ChannelBusManager - setDriverPriority re-sorts engines") {
    ChannelBusManager manager;

    // Add engines with initial priorities
    auto engineA = fl::make_shared<FakeEngine>("ENGINE_A", false, true, false);
    auto engineB = fl::make_shared<FakeEngine>("ENGINE_B", false, true, false);
    auto engineC = fl::make_shared<FakeEngine>("ENGINE_C", false, true, false);

    manager.addEngine(1000, engineA);  // Lowest priority
    manager.addEngine(5000, engineB);  // Medium priority
    manager.addEngine(9000, engineC);  // Highest priority

    // Verify initial order (sorted by priority: high to low)
    auto infos = manager.getDriverInfos();
    FL_REQUIRE(infos.size() == 3);
    FL_CHECK(infos[0].name == "ENGINE_C");  // Priority 9000
    FL_CHECK(infos[1].name == "ENGINE_B");  // Priority 5000
    FL_CHECK(infos[2].name == "ENGINE_A");  // Priority 1000
    FL_CHECK_EQ(infos[0].priority, 9000);
    FL_CHECK_EQ(infos[1].priority, 5000);
    FL_CHECK_EQ(infos[2].priority, 1000);

    // Change ENGINE_A to highest priority
    bool result = manager.setDriverPriority("ENGINE_A", 10000);
    FL_CHECK(result);

    // Verify engines re-sorted (ENGINE_A should now be first)
    infos = manager.getDriverInfos();
    FL_REQUIRE(infos.size() == 3);
    FL_CHECK(infos[0].name == "ENGINE_A");  // Priority 10000 (was 1000)
    FL_CHECK(infos[1].name == "ENGINE_C");  // Priority 9000
    FL_CHECK(infos[2].name == "ENGINE_B");  // Priority 5000
    FL_CHECK_EQ(infos[0].priority, 10000);
    FL_CHECK_EQ(infos[1].priority, 9000);
    FL_CHECK_EQ(infos[2].priority, 5000);

    // Change ENGINE_B to medium-high priority
    result = manager.setDriverPriority("ENGINE_B", 9500);
    FL_CHECK(result);

    // Verify re-sorted again
    infos = manager.getDriverInfos();
    FL_REQUIRE(infos.size() == 3);
    FL_CHECK(infos[0].name == "ENGINE_A");  // Priority 10000
    FL_CHECK(infos[1].name == "ENGINE_B");  // Priority 9500 (was 5000)
    FL_CHECK(infos[2].name == "ENGINE_C");  // Priority 9000
    FL_CHECK_EQ(infos[0].priority, 10000);
    FL_CHECK_EQ(infos[1].priority, 9500);
    FL_CHECK_EQ(infos[2].priority, 9000);

    // Verify non-existent engine returns false
    result = manager.setDriverPriority("NONEXISTENT", 5000);
    FL_CHECK_FALSE(result);

    // Verify list unchanged after failed operation
    infos = manager.getDriverInfos();
    FL_REQUIRE(infos.size() == 3);
    FL_CHECK(infos[0].name == "ENGINE_A");
    FL_CHECK(infos[1].name == "ENGINE_B");
    FL_CHECK(infos[2].name == "ENGINE_C");
}

FL_TEST_CASE("ChannelBusManager - priority re-sort affects engine selection order") {
    ChannelBusManager manager;

    // Create engines that both accept clockless channels
    auto lowPriorityEngine = fl::make_shared<FakeEngine>("LOW_PRIORITY", false, true, false);
    auto highPriorityEngine = fl::make_shared<FakeEngine>("HIGH_PRIORITY", false, true, false);

    manager.addEngine(1000, lowPriorityEngine);
    manager.addEngine(9000, highPriorityEngine);

    // Create clockless channel data
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    fl::vector_psram<uint8_t> channelData = {0xFF, 0x00, 0xAA};
    auto data = ChannelData::create(5, timing, fl::move(channelData));

    // First transmission should use high priority engine
    manager.enqueue(data);
    manager.show();
    FL_CHECK_EQ(highPriorityEngine->getTransmitCount(), 1);
    FL_CHECK_EQ(lowPriorityEngine->getTransmitCount(), 0);

    // Reset counters
    highPriorityEngine->reset();
    lowPriorityEngine->reset();

    // Increase low priority engine's priority above high priority engine
    manager.setDriverPriority("LOW_PRIORITY", 10000);

    // Create new channel data for second transmission
    fl::vector_psram<uint8_t> channelData2 = {0xFF, 0x00, 0xAA};
    auto data2 = ChannelData::create(5, timing, fl::move(channelData2));

    // Second transmission should now use formerly-low-priority engine
    manager.enqueue(data2);
    manager.show();
    FL_CHECK_EQ(lowPriorityEngine->getTransmitCount(), 1);  // Now selected first
    FL_CHECK_EQ(highPriorityEngine->getTransmitCount(), 0);
}

FL_TEST_CASE("ChannelBusManager - addEngine rejects engine with empty getName()") {
    ChannelBusManager manager;
    // Engine with empty name from getName()
    auto engine = fl::make_shared<FakeEngine>("");

    // Attempt to add engine with empty getName() (should be rejected)
    size_t countBefore = manager.getDriverCount();
    manager.addEngine(100, engine);
    size_t countAfter = manager.getDriverCount();

    // Engine should NOT be added
    FL_CHECK_EQ(countBefore, countAfter);
}

FL_TEST_CASE("ChannelBusManager - addEngine replaces engine with same name") {
    ChannelBusManager manager;

    // Add first engine with name "REPLACEABLE"
    auto engine1 = fl::make_shared<FakeEngine>("REPLACEABLE", false, true, false);
    manager.addEngine(100, engine1);

    FL_CHECK_EQ(manager.getDriverCount(), 1);

    // Get reference to first engine via name
    IChannelEngine* retrievedEngine1 = manager.getEngineByName("REPLACEABLE");
    FL_CHECK(retrievedEngine1 != nullptr);
    FL_CHECK(retrievedEngine1 == engine1.get());

    // Add second engine with SAME name "REPLACEABLE" (should replace)
    auto engine2 = fl::make_shared<FakeEngine>("REPLACEABLE", false, true, true);
    manager.addEngine(200, engine2);  // Same name, different priority and capabilities

    // Should still have only 1 engine
    FL_CHECK_EQ(manager.getDriverCount(), 1);

    // Retrieved engine should now be engine2
    IChannelEngine* retrievedEngine2 = manager.getEngineByName("REPLACEABLE");
    FL_CHECK(retrievedEngine2 != nullptr);
    FL_CHECK(retrievedEngine2 == engine2.get());
    FL_CHECK(retrievedEngine2 != engine1.get());  // Not the old engine

    // Verify capabilities changed (engine2 supports SPI, engine1 didn't)
    auto caps = retrievedEngine2->getCapabilities();
    FL_CHECK(caps.supportsSpi == true);
}

FL_TEST_CASE("ChannelBusManager - engine replacement waits for READY state") {
    ChannelBusManager manager;

    // Create engine that stays BUSY initially
    class BusyEngine : public FakeEngine {
    public:
        BusyEngine() : FakeEngine("BUSY"), mPollCount(0), mBusyCycles(5) {}

        EngineState poll() override {
            mPollCount++;
            // Stay BUSY for first 5 polls, then become READY
            if (mPollCount < mBusyCycles) {
                return EngineState::BUSY;
            }
            return FakeEngine::poll();  // Return READY
        }

        int getPollCount() const { return mPollCount; }
        void reset() { mPollCount = 0; FakeEngine::reset(); }
        void setBusyCycles(int cycles) { mBusyCycles = cycles; }

    private:
        int mPollCount;
        int mBusyCycles;
    };

    auto busyEngine = fl::make_shared<BusyEngine>();
    manager.addEngine(100, busyEngine);

    // Enqueue some data to make it transmitting
    auto data = createDummyChannelData();
    manager.enqueue(data);
    manager.show();

    // Verify engine is BUSY after transmission starts
    int initialPollCount = busyEngine->getPollCount();
    FL_CHECK(initialPollCount > 0);  // Should have been polled

    // Reset poll count to make it BUSY again for replacement test
    busyEngine->reset();
    busyEngine->setBusyCycles(5);  // Will be BUSY for 5 polls

    // Replace the engine with another engine with SAME name "BUSY"
    // (should wait for READY, polling busyEngine until it becomes READY)
    auto newEngine = fl::make_shared<FakeEngine>("BUSY");
    manager.addEngine(200, newEngine);

    // Verify poll was called multiple times while waiting for READY
    // (busyEngine should have been polled at least 5 times during replacement)
    FL_CHECK(busyEngine->getPollCount() >= 5);

    // Verify replacement succeeded
    IChannelEngine* retrieved = manager.getEngineByName("BUSY");
    FL_CHECK(retrieved == newEngine.get());
}

} // namespace channel_bus_manager_test
