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
/// `enableAllDrivers()` both must be findable in the manager registry and
/// must be the same singleton instances exposed by `BusTraits<B>::instance()`.
///
/// `ChannelManager` is a process-singleton, so every test case begins by
/// clearing the registry and asserting it is empty — otherwise a later case
/// could pass purely on residue from an earlier one (e.g. the "forwards"
/// test would succeed even if `FastLED.enableAllDrivers()` were a no-op).

#include "fl/channels/all_drivers.h"
#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/manager.h"
#include "fl/stl/string.h"
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

}  // namespace

FL_TEST_CASE("enableAllDrivers() registers Bus::STUB and Bus::BIT_BANG on host") {
    auto& mgr = freshManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    fl::enableAllDrivers();

    // Bus::BIT_BANG is always-on (no platform guard) and must register on
    // every build, including the host test runner. The driver's getName()
    // returns "BITBANG" (no underscore) — the manager keys on that string.
    auto bitbang = mgr.getDriverByName(fl::string::from_literal("BITBANG"));
    FL_REQUIRE(bitbang != nullptr);
    FL_CHECK(bitbang.get() == &fl::BusTraits<fl::Bus::BIT_BANG>::instance());

    // Bus::STUB is host-only — guarded by FL_IS_STUB / FL_IS_WASM in
    // all_drivers.h. Tests run with FL_IS_STUB defined, so it must register.
    auto stub = mgr.getDriverByName(fl::string::from_literal("STUB"));
    FL_REQUIRE(stub != nullptr);
    FL_CHECK(stub.get() == &fl::BusTraits<fl::Bus::STUB>::instance());
}

FL_TEST_CASE("FastLED.enableAllDrivers() forwards to fl::enableAllDrivers()") {
    auto& mgr = freshManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    // Drive the member-function entry point on an empty registry. If the
    // member did nothing, the lookups below would return null because the
    // precondition above guarantees no prior registration leaked in.
    FastLED.enableAllDrivers();

    auto bitbang = mgr.getDriverByName(fl::string::from_literal("BITBANG"));
    FL_REQUIRE(bitbang != nullptr);
    FL_CHECK(bitbang.get() == &fl::BusTraits<fl::Bus::BIT_BANG>::instance());

    auto stub = mgr.getDriverByName(fl::string::from_literal("STUB"));
    FL_REQUIRE(stub != nullptr);
    FL_CHECK(stub.get() == &fl::BusTraits<fl::Bus::STUB>::instance());
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

    // The driver identities must not change either — same singletons.
    auto bitbang = mgr.getDriverByName(fl::string::from_literal("BITBANG"));
    FL_REQUIRE(bitbang != nullptr);
    FL_CHECK(bitbang.get() == &fl::BusTraits<fl::Bus::BIT_BANG>::instance());
}

}  // FL_TEST_FILE
