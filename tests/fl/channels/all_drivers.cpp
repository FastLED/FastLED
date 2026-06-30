/// @file all_drivers.cpp
/// @brief Sanity test for fl::enableAllDrivers() on the host build.

#include "fl/channels/all_drivers.h"
#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/manager.h"
#include "fl/stl/string.h"
#include "platforms/shared/bitbang/bus_traits.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

fl::ChannelManager& freshManager() {
    auto& mgr = fl::ChannelManager::instance();
    mgr.clearAllDrivers();
    return mgr;
}

void checkHostDriversRegistered(fl::ChannelManager& mgr) {
    auto bitbang = mgr.getDriverByName(fl::string::from_literal("BIT_BANG"));
    FL_REQUIRE(bitbang != nullptr);
    FL_CHECK(bitbang->getName() == fl::string::from_literal("BIT_BANG"));
    auto caps = bitbang->getCapabilities();
    FL_CHECK(caps.supportsClockless);
    FL_CHECK(caps.supportsSpi);
}

}  // namespace

FL_TEST_CASE("enableAllDrivers() registers Bus::BIT_BANG on host") {
    auto& mgr = freshManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    fl::enableAllDrivers();

    checkHostDriversRegistered(mgr);
}

FL_TEST_CASE("FastLED.enableAllDrivers() forwards to fl::enableAllDrivers()") {
    auto& mgr = freshManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    FastLED.enableAllDrivers();

    checkHostDriversRegistered(mgr);
}

FL_TEST_CASE("enableAllDrivers() is idempotent") {
    auto& mgr = freshManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    fl::enableAllDrivers();
    auto count_after_first = mgr.getDriverCount();
    FL_REQUIRE(count_after_first > 0);

    fl::enableAllDrivers();
    auto count_after_second = mgr.getDriverCount();

    FL_CHECK(count_after_first == count_after_second);
    checkHostDriversRegistered(mgr);
}

}  // FL_TEST_FILE
