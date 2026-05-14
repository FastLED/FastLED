/// @file all_drivers.cpp
/// @brief Sanity test for fl::enableAllDrivers() on the host build.
///
/// `fl::enableAllDrivers()` is the convenience helper introduced for issue
/// #2428: it registers every driver visible on the current platform with
/// `ChannelManager`, restoring 3.10.3-style runtime driver flexibility for
/// users who opt back in.
///
/// On the host (FL_IS_STUB) the only buses with actual `BusTraits<>`
/// specializations are `Bus::STUB` (the host-only stub driver) and
/// `Bus::BIT_BANG` (always-on portable fallback). After calling
/// `enableAllDrivers()` both must be findable in the manager registry with
/// the expected names and capabilities.
///
/// `ChannelManager` is a process-singleton, so every test case begins by
/// clearing the registry and asserting it is empty. Otherwise a later case
/// could pass purely on residue from an earlier one.

#include "fl/channels/all_drivers.h"
#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/manager.h"
#include "fl/stl/string.h"
#include "platforms/is_platform.h"
#include "platforms/shared/bitbang/bus_traits.h"  // BusTraits<Bus::BIT_BANG>
#if defined(FL_IS_STUB) || defined(FL_IS_WASM)
#include "platforms/stub/bus_traits.h"  // BusTraits<Bus::STUB>
#endif
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

// Reset the shared singleton so each case starts from a known-empty registry.
// Returns the manager reference for convenience.
fl::ChannelManager& freshManager() {
    auto& mgr = fl::ChannelManager::instance();
    mgr.clearAllDrivers();
    return mgr;
}

void checkHostDriversRegistered(fl::ChannelManager& mgr) {
    auto bitbang = mgr.getDriverByName(fl::string::from_literal("BIT_BANG"));
    FL_REQUIRE(bitbang != nullptr);
    FL_CHECK(bitbang->getName() == fl::string::from_literal("BIT_BANG"));
    auto bitbangCaps = bitbang->getCapabilities();
    FL_CHECK(bitbangCaps.supportsClockless);
    FL_CHECK(bitbangCaps.supportsSpi);

    auto stub = mgr.getDriverByName(fl::string::from_literal("STUB"));
    FL_REQUIRE(stub != nullptr);
    FL_CHECK(stub->getName() == fl::string::from_literal("STUB"));
    auto stubCaps = stub->getCapabilities();
    FL_CHECK(stubCaps.supportsClockless);
    FL_CHECK(!stubCaps.supportsSpi);
}

}  // namespace

FL_TEST_CASE("enableAllDrivers() registers Bus::STUB and Bus::BIT_BANG on host") {
    auto& mgr = freshManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    fl::enableAllDrivers();

    checkHostDriversRegistered(mgr);
}

FL_TEST_CASE("FastLED.enableAllDrivers() forwards to fl::enableAllDrivers()") {
    auto& mgr = freshManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    // Drive the member-function entry point on an empty registry. If the
    // member did nothing, the lookups below would return null because the
    // precondition above guarantees no prior registration leaked in.
    FastLED.enableAllDrivers();

    checkHostDriversRegistered(mgr);
}

FL_TEST_CASE("enableAllDrivers() is idempotent") {
    auto& mgr = freshManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    fl::enableAllDrivers();
    auto count_after_first = mgr.getDriverCount();
    FL_REQUIRE(count_after_first > 0);  // at least BIT_BANG + STUB on host

    fl::enableAllDrivers();
    auto count_after_second = mgr.getDriverCount();

    // Manager replaces by name on duplicate `addDriver`, so a second call
    // must leave the count unchanged.
    FL_CHECK(count_after_first == count_after_second);
    checkHostDriversRegistered(mgr);
}

}  // FL_TEST_FILE
