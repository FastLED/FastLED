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



FL_TEST_CASE("ChannelBusManager - poll() returns aggregate state") {
    ChannelBusManager manager;

    // With no engines, should return READY
    FL_CHECK(manager.poll().state == IChannelEngine::EngineState::READY);

    // Add an engine
    auto engine = fl::make_shared<FakeEngine>("TEST_POLL");
    manager.addEngine(100, engine);

    // Should return READY (fake engine is always ready)
    FL_CHECK(manager.poll().state == IChannelEngine::EngineState::READY);

    // Cleanup
    manager.removeEngine(engine);
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
    auto retrieved = manager.getEngineByName("RMT");
    FL_CHECK(retrieved.get() == rmt2.get());
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
