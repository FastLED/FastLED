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

#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/channels/manager.h"
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

} // namespace draining_state_test

} // FL_TEST_FILE
