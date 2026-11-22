/// @file draining_state.cpp
/// @brief Tests that ChannelManager correctly propagates DRAINING state
///
/// Bug: The rename from ChannelEngine→ChannelDriver changed all
/// EngineState::DRAINING returns to DriverState::BUSY in concrete drivers.
/// Additionally, ChannelManager::poll() never propagated DRAINING state -
/// it only checked for BUSY and ERROR, treating DRAINING as READY.
///
/// The state machine contract is: READY → BUSY → DRAINING → READY
/// - BUSY: Driver is actively processing (encoding, queuing to hardware)
/// - DRAINING: All data submitted to hardware DMA, CPU is free
/// - READY: Hardware idle, safe to start next frame
///
/// onEndFrame() calls waitForReadyOrDraining() which should return as soon
/// as the driver reaches DRAINING (DMA in-flight, CPU free). If DRAINING is
/// lost (mapped to BUSY), onEndFrame() blocks for the full transmission
/// duration, starving WebSockets and other tasks.

#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/channels/manager.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

namespace draining_state_test {

using namespace fl;
using DriverState = IChannelDriver::DriverState;

/// Mock driver that lets tests control what poll() returns.
class StatefulMockDriver : public IChannelDriver {
public:
    explicit StatefulMockDriver(const char* name) : mName(name) {}

    // Controllable state
    DriverState::Value mState = DriverState::READY;
    int showCount = 0;
    int enqueueCount = 0;
    int pollCount = 0;

    bool canHandle(const ChannelDataPtr&) const override { return true; }
    void enqueue(ChannelDataPtr) override { enqueueCount++; }
    void show() override { showCount++; }
    DriverState poll() override { pollCount++; return DriverState(mState); }
    fl::string getName() const override { return mName; }

    Capabilities getCapabilities() const override {
        return Capabilities(true, false);
    }

private:
    fl::string mName;
};

FL_TEST_CASE("ChannelManager::poll propagates each state correctly") {
    auto driver = fl::make_shared<StatefulMockDriver>("DRAIN_POLL");
    ChannelManager& mgr = ChannelManager::instance();
    mgr.addDriver(5000, driver);

    // Step through the full lifecycle: READY → BUSY → DRAINING → READY
    // Each mgr.poll() call is a single poll - no looping, no timeouts.

    driver->mState = DriverState::READY;
    FL_CHECK_EQ(static_cast<int>(mgr.poll().state),
                static_cast<int>(DriverState::READY));

    driver->mState = DriverState::BUSY;
    FL_CHECK_EQ(static_cast<int>(mgr.poll().state),
                static_cast<int>(DriverState::BUSY));

    driver->mState = DriverState::DRAINING;
    FL_CHECK_EQ(static_cast<int>(mgr.poll().state),
                static_cast<int>(DriverState::DRAINING));

    driver->mState = DriverState::READY;
    FL_CHECK_EQ(static_cast<int>(mgr.poll().state),
                static_cast<int>(DriverState::READY));

    // Verify poll was called exactly 4 times (once per mgr.poll())
    FL_CHECK_EQ(driver->pollCount, 4);

    mgr.removeDriver(driver);
}

FL_TEST_CASE("waitForReadyOrDraining returns immediately on DRAINING") {
    auto driver = fl::make_shared<StatefulMockDriver>("DRAIN_WAIT");
    ChannelManager& mgr = ChannelManager::instance();
    mgr.addDriver(5001, driver);

    // DRAINING → should return immediately (true)
    driver->mState = DriverState::DRAINING;
    FL_CHECK(mgr.waitForReadyOrDraining(100));

    // READY → should return immediately (true)
    driver->mState = DriverState::READY;
    FL_CHECK(mgr.waitForReadyOrDraining(100));

    // BUSY → should timeout (false) - DMA not yet submitted
    driver->mState = DriverState::BUSY;
    FL_CHECK_FALSE(mgr.waitForReadyOrDraining(50));

    driver->mState = DriverState::READY;
    mgr.removeDriver(driver);
}

FL_TEST_CASE("waitForReady blocks on DRAINING but not on READY") {
    auto driver = fl::make_shared<StatefulMockDriver>("DRAIN_READY");
    ChannelManager& mgr = ChannelManager::instance();
    mgr.addDriver(5002, driver);

    // READY → waitForReady returns immediately
    driver->mState = DriverState::READY;
    FL_CHECK(mgr.waitForReady(100));

    // DRAINING → waitForReady should block (DMA still in-flight, not safe
    // to start next frame). With short timeout, it should return false.
    driver->mState = DriverState::DRAINING;
    FL_CHECK_FALSE(mgr.waitForReady(50));

    // BUSY → waitForReady should also block
    driver->mState = DriverState::BUSY;
    FL_CHECK_FALSE(mgr.waitForReady(50));

    driver->mState = DriverState::READY;
    mgr.removeDriver(driver);
}

FL_TEST_CASE("IChannelDriver waits distinguish READY DRAINING and BUSY") {
    StatefulMockDriver driver("DIRECT_WAIT");

    driver.mState = DriverState::READY;
    FL_CHECK(driver.waitForReady(10));
    FL_CHECK_EQ(driver.pollCount, 1);

    driver.pollCount = 0;
    FL_CHECK(driver.waitForReadyOrDraining(10));
    FL_CHECK_EQ(driver.pollCount, 1);

    driver.pollCount = 0;
    driver.mState = DriverState::DRAINING;
    FL_CHECK(driver.waitForReadyOrDraining(10));
    FL_CHECK_EQ(driver.pollCount, 1);
    FL_CHECK_FALSE(driver.waitForReady(10));

    driver.mState = DriverState::BUSY;
    FL_CHECK_FALSE(driver.waitForReady(10));
    FL_CHECK_FALSE(driver.waitForReadyOrDraining(10));
}

FL_TEST_CASE("poll propagates worst state across multiple drivers") {
    auto driverA = fl::make_shared<StatefulMockDriver>("MULTI_A");
    auto driverB = fl::make_shared<StatefulMockDriver>("MULTI_B");
    ChannelManager& mgr = ChannelManager::instance();
    mgr.addDriver(6000, driverA);
    mgr.addDriver(6001, driverB);

    // Both READY → READY
    driverA->mState = DriverState::READY;
    driverB->mState = DriverState::READY;
    FL_CHECK_EQ(static_cast<int>(mgr.poll().state),
                static_cast<int>(DriverState::READY));

    // One DRAINING, one READY → DRAINING (worst wins)
    driverA->mState = DriverState::DRAINING;
    driverB->mState = DriverState::READY;
    FL_CHECK_EQ(static_cast<int>(mgr.poll().state),
                static_cast<int>(DriverState::DRAINING));

    // One BUSY, one DRAINING → BUSY (worst wins)
    driverA->mState = DriverState::DRAINING;
    driverB->mState = DriverState::BUSY;
    FL_CHECK_EQ(static_cast<int>(mgr.poll().state),
                static_cast<int>(DriverState::BUSY));

    // One BUSY, one READY → BUSY
    driverA->mState = DriverState::BUSY;
    driverB->mState = DriverState::READY;
    FL_CHECK_EQ(static_cast<int>(mgr.poll().state),
                static_cast<int>(DriverState::BUSY));

    driverA->mState = DriverState::READY;
    driverB->mState = DriverState::READY;
    mgr.removeDriver(driverA);
    mgr.removeDriver(driverB);
}

FL_TEST_CASE("isDriverEnabled(driver) matches by instance, falls back to name (#4799)") {
    ChannelManager mgr;
    auto a = fl::make_shared<StatefulMockDriver>("BY_PTR_A");
    auto b = fl::make_shared<StatefulMockDriver>("BY_PTR_B");
    mgr.addDriver(10, a);
    mgr.addDriver(20, b);
    FL_CHECK(mgr.isDriverEnabled(*a));
    mgr.setDriverEnabled("BY_PTR_A", false);
    FL_CHECK_FALSE(mgr.isDriverEnabled(*a));
    FL_CHECK(mgr.isDriverEnabled(*b));
    // An unregistered instance with a registered name uses that name's state.
    StatefulMockDriver lookalike("BY_PTR_A");
    FL_CHECK_FALSE(mgr.isDriverEnabled(lookalike));
}

/// Driver whose completion is asynchronous: show() starts a transfer that
/// reports BUSY, then DRAINING, for a set number of polls before READY.
class CountingAsyncDriver : public IChannelDriver {
public:
    explicit CountingAsyncDriver(const char* name) : mName(name) {}

    int pollCount = 0;
    int enqueueCount = 0;
    int busyPolls = 0;      // polls left reporting BUSY
    int drainingPolls = 0;  // polls left reporting DRAINING after BUSY
    int showBusyPolls = 0;      // loaded into busyPolls by show()
    int showDrainingPolls = 0;  // loaded into drainingPolls by show()

    bool canHandle(const ChannelDataPtr&) const override { return true; }
    void enqueue(ChannelDataPtr) override { enqueueCount++; }
    void show() override {
        busyPolls = showBusyPolls;
        drainingPolls = showDrainingPolls;
    }
    DriverState poll() override {
        pollCount++;
        if (busyPolls > 0) {
            --busyPolls;
            return DriverState(DriverState::BUSY);
        }
        if (drainingPolls > 0) {
            --drainingPolls;
            return DriverState(DriverState::DRAINING);
        }
        return DriverState(DriverState::READY);
    }
    bool idle() const { return busyPolls == 0 && drainingPolls == 0; }
    fl::string getName() const override { return mName; }
    Capabilities getCapabilities() const override {
        return Capabilities(true, false);
    }

private:
    fl::string mName;
};

FL_TEST_CASE("onBeginFrame skips the poll after onEndFrame saw every driver READY #5381") {
    ChannelManager mgr;
    auto driver = fl::make_shared<CountingAsyncDriver>("READY_CACHE");
    mgr.addDriver(10, driver);

    mgr.onEndFrame();
    FL_CHECK_EQ(driver->pollCount, 1);  // onEndFrame observed READY
    mgr.onBeginFrame();
    FL_CHECK_EQ(driver->pollCount, 1);  // no second, redundant poll

    // Nothing created work since, so a repeated frame start polls neither.
    mgr.onBeginFrame();
    FL_CHECK_EQ(driver->pollCount, 1);
}

FL_TEST_CASE("show() leaving a driver DRAINING makes onBeginFrame poll until READY #5381") {
    ChannelManager mgr;
    auto driver = fl::make_shared<CountingAsyncDriver>("DRAIN_CACHE");
    mgr.addDriver(10, driver);

    mgr.onEndFrame();  // READY: caches all-ready
    driver->showDrainingPolls = 3;
    mgr.onEndFrame();  // show() starts a transfer; returns on DRAINING
    FL_CHECK_EQ(driver->pollCount, 2);
    FL_CHECK_FALSE(driver->idle());

    mgr.onBeginFrame();
    FL_CHECK(driver->idle());          // no frame starts while it drains
    FL_CHECK_EQ(driver->pollCount, 5);  // 2 more DRAINING polls, then READY
}

FL_TEST_CASE("a BUSY driver is polled until READY even when it settles async #5381") {
    ChannelManager mgr;
    auto driver = fl::make_shared<CountingAsyncDriver>("BUSY_CACHE");
    mgr.addDriver(10, driver);

    driver->showBusyPolls = 2;
    driver->showDrainingPolls = 2;
    mgr.onEndFrame();  // BUSY, BUSY, DRAINING -> returns
    FL_CHECK_EQ(driver->pollCount, 3);

    // The transfer completes in an ISR, without the manager noticing.
    driver->drainingPolls = 0;
    mgr.onBeginFrame();
    FL_CHECK_EQ(driver->pollCount, 4);  // polled once and saw READY

    // Only that READY poll seeds the cache; the DRAINING one did not.
    mgr.onBeginFrame();
    FL_CHECK_EQ(driver->pollCount, 4);
}

FL_TEST_CASE("driver registration and removal invalidate the cached all-ready state #5381") {
    ChannelManager mgr;
    auto first = fl::make_shared<CountingAsyncDriver>("REG_FIRST");
    mgr.addDriver(10, first);
    mgr.onEndFrame();

    // A newly registered driver may already be transmitting.
    auto late = fl::make_shared<CountingAsyncDriver>("REG_LATE");
    late->busyPolls = 2;
    mgr.addDriver(20, late);
    mgr.onBeginFrame();
    FL_CHECK(late->idle());
    FL_CHECK_EQ(late->pollCount, 3);

    mgr.onEndFrame();
    const int before = first->pollCount;
    FL_CHECK(mgr.removeDriver(late));
    mgr.onBeginFrame();
    FL_CHECK_EQ(first->pollCount, before + 1);
}

FL_TEST_CASE("a channel enqueue invalidates the cached all-ready state #5381") {
    ChannelManager& mgr = ChannelManager::instance();
    auto driver = fl::make_shared<CountingAsyncDriver>("ENQ_CACHE");
    mgr.addDriver(90000, driver);
    CRGB leds[4] = {};
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    // The highest priority wins AUTO dispatch.
    auto channel = Channel::create(
        ChannelConfig(4, timing, fl::span<CRGB>(leds, 4), RGB));
    FL_REQUIRE(channel != nullptr);
    auto cleanup = fl::make_scope_exit([&]() {
        channel->removeFromDrawList();
        mgr.removeDriver(driver);
    });

    mgr.onEndFrame();
    // Enqueue outside a frame: showLeds() would wrap it in frame events.
    void* frame = channel->beginShowLeds(4);
    channel->showLedsInternal(255);
    channel->endShowLeds(frame);
    FL_REQUIRE_EQ(driver->enqueueCount, 1);
    const int before = driver->pollCount;
    mgr.onBeginFrame();
    FL_CHECK_EQ(driver->pollCount, before + 1);
}

} // namespace draining_state_test

} // FL_TEST_FILE
