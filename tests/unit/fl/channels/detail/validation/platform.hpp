// tests/fl/channels/detail/validation/platform.cpp
//
// Unit tests for platform validation

#include "test.h"
#include "fl/channels/detail/validation/platform.h"
#include "fl/channels/manager.h"
#include "fl/channels/driver.h"

using namespace fl;

namespace {

class StubDriver : public IChannelDriver {
public:
    StubDriver(const char* name) : mName(fl::string::from_literal(name)) {}

    void enqueue(ChannelDataPtr) override {}
    void show() override {}
    DriverState poll() override { return DriverState::READY; }
    fl::string getName() const override { return mName; }
    Capabilities getCapabilities() const override { return Capabilities(true, false); }
    bool canHandle(const ChannelDataPtr&) const override { return true; }

private:
    fl::string mName;
};

} // namespace

FL_TEST_CASE("validateExpectedEngines - no drivers returns false") {
    channelManager().clearAllDrivers();
    FL_CHECK_FALSE(fl::validation::validateExpectedEngines());
}

FL_TEST_CASE("validateExpectedEngines - with driver returns true") {
    channelManager().clearAllDrivers();
    auto driver = fl::make_shared<StubDriver>("TEST_DRIVER");
    channelManager().addDriver(100, driver);

    FL_CHECK(fl::validation::validateExpectedEngines());

    channelManager().clearAllDrivers();
}

FL_TEST_CASE("validateExpectedEngines - multiple drivers returns true") {
    channelManager().clearAllDrivers();
    channelManager().addDriver(100, fl::make_shared<StubDriver>("DRIVER_A"));
    channelManager().addDriver(200, fl::make_shared<StubDriver>("DRIVER_B"));

    FL_CHECK(fl::validation::validateExpectedEngines());

    channelManager().clearAllDrivers();
}
