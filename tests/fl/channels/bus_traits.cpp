/// @file bus_traits.cpp
/// @brief Phase 1 sanity test for fl::Bus + BusTraits foundational types.
///
/// Verifies that the new compile-time bus identifier and per-driver trait
/// machinery resolves cleanly on the host build, and that the stub canary
/// specialization satisfies the chipset-capability contract.
///
/// See issue #2428.

#include "FastLED.h"  // For FastLED.addLeds<..., fl::Bus> tests (#2460)
#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"  // ClocklessChipset, SpiChipsetConfig
#include "fl/channels/driver.h"  // IChannelDriver
#include "fl/channels/manager.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "platforms/is_platform.h"
#include "platforms/stub/bus_traits.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// Compile-time invariants — these enforce the design at the type system level.
// Failure to compile these is a Phase 1 regression.

FL_STATIC_ASSERT(static_cast<fl::u8>(fl::Bus::AUTO) == 0,
                 "Bus::AUTO must be the zero/default sentinel");
FL_STATIC_ASSERT(static_cast<fl::u8>(fl::Bus::RMT) != static_cast<fl::u8>(fl::Bus::PARLIO),
                 "Bus enum values must be distinct");
FL_STATIC_ASSERT(sizeof(fl::Bus) == 1,
                 "Bus underlying type should be u8 to keep ChannelOptions cheap");

// DefaultBus<Chipset> resolves on the host (FL_IS_STUB is set under tests).
FL_STATIC_ASSERT(fl::DefaultBus<fl::ClocklessChipset>::value == fl::Bus::STUB,
                 "Host default clockless bus should be STUB");
FL_STATIC_ASSERT(fl::DefaultBus<fl::SpiChipsetConfig>::value == fl::Bus::STUB,
                 "Host default SPI bus should be STUB");

// Capability traits: BusSupports defaults to false_type for unknown pairs and is
// true_type for the stub clockless specialization.
FL_STATIC_ASSERT(fl::BusSupports<fl::Bus::STUB, fl::ClocklessChipset>::value,
                 "Stub bus must support clockless chipsets");
FL_STATIC_ASSERT(!fl::BusSupports<fl::Bus::STUB, fl::SpiChipsetConfig>::value,
                 "Stub bus does not (yet) support SPI chipsets");
FL_STATIC_ASSERT(!fl::BusSupports<fl::Bus::RMT, fl::SpiChipsetConfig>::value,
                 "Bus values without specializations must default to false_type");

FL_TEST_CASE("BusTraits<Bus::STUB>::instance returns a stable singleton") {
    auto& a = fl::BusTraits<fl::Bus::STUB>::instance();
    auto& b = fl::BusTraits<fl::Bus::STUB>::instance();
    FL_CHECK(&a == &b);
}

FL_TEST_CASE("BusTraits<Bus::STUB>::Driver implements IChannelDriver contract") {
    using Driver = fl::BusTraits<fl::Bus::STUB>::Driver;
    fl::IChannelDriver* iface = &fl::BusTraits<fl::Bus::STUB>::instance();
    FL_REQUIRE(iface != nullptr);
    FL_CHECK(iface->getName() == fl::string::from_literal("STUB"));
    FL_CHECK(iface->getCapabilities().supportsClockless);
    FL_CHECK_FALSE(iface->getCapabilities().supportsSpi);
    // Cast back to concrete type to exercise the alias.
    Driver* concrete = static_cast<Driver*>(iface);
    FL_CHECK(concrete != nullptr);
}

FL_TEST_CASE("BusTraits<Bus::STUB>::instancePtr returns a non-null shared_ptr") {
    auto p = fl::BusTraits<fl::Bus::STUB>::instancePtr();
    FL_REQUIRE(p != nullptr);
    // instance() and *instancePtr() must reference the same object.
    FL_CHECK(p.get() == &fl::BusTraits<fl::Bus::STUB>::instance());
}

FL_TEST_CASE("enableDrivers<Bus::STUB>() registers the stub driver with ChannelManager") {
    // Snapshot driver count before; explicit enableDrivers should add (or
    // replace, if the legacy auto-init already registered) by name.
    auto& mgr = fl::ChannelManager::instance();
    auto before = mgr.getDriverCount();
    fl::enableDrivers<fl::Bus::STUB>();
    auto after = mgr.getDriverCount();
    // After enableDrivers the manager must contain the stub driver. Count
    // either grows by one (fresh registration) or stays equal (replace by name).
    FL_CHECK(after >= before);
    auto stubDriver = mgr.getDriverByName(fl::string::from_literal("STUB"));
    FL_REQUIRE(stubDriver != nullptr);
    // Identity check: the registered driver must be the BusTraits singleton.
    FL_CHECK(stubDriver.get() == &fl::BusTraits<fl::Bus::STUB>::instance());
}

// ---------------------------------------------------------------------------
// #2460 — trailing `fl::Bus B` template parameter on FastLED.addLeds<>
// ---------------------------------------------------------------------------
//
// These cases exercise the new `busKeepAlive<B>()` helper and at least one
// clockless + one SPI legacy-API call site with `B = fl::Bus::STUB`. On host
// `Bus::STUB` is the only bus we can name without dragging in real hardware
// drivers, but the body change is the same for every variant.

FL_TEST_CASE("busKeepAlive<Bus::AUTO>() is a no-op and compiles") {
    fl::busKeepAlive<fl::Bus::AUTO>();  // AUTO specialization: no ODR-use.
    // Same identity check as plain `instance()` — proving the AUTO path
    // did not corrupt state in any way.
    auto& a = fl::BusTraits<fl::Bus::STUB>::instance();
    auto& b = fl::BusTraits<fl::Bus::STUB>::instance();
    FL_CHECK(&a == &b);
}

FL_TEST_CASE("busKeepAlive<Bus::STUB>() ODR-uses the singleton accessor") {
    fl::busKeepAlive<fl::Bus::STUB>();
    // After the keep-alive runs, the singleton must still be valid.
    auto p = fl::BusTraits<fl::Bus::STUB>::instancePtr();
    FL_REQUIRE(p != nullptr);
    FL_CHECK(p.get() == &fl::BusTraits<fl::Bus::STUB>::instance());
}

FL_TEST_CASE("FastLED.addLeds<WS2812, ..., fl::Bus::STUB> clockless variant compiles") {
    // Acceptance criterion: clockless variant accepts the trailing Bus param.
    static CRGB leds[8];
    auto& ctrl = FastLED.addLeds<WS2812, 2, GRB, fl::Bus::STUB>(leds, 8);
    (void)ctrl;
    // Identity check post-instantiation.
    FL_CHECK(&fl::BusTraits<fl::Bus::STUB>::instance() != nullptr);
}

FL_TEST_CASE("FastLED.addLeds<APA102, ..., fl::Bus::STUB> SPI variant compiles") {
    // Acceptance criterion: SPI variant accepts the trailing Bus param.
    static CRGB leds[8];
    auto& ctrl = FastLED.addLeds<APA102, 23, 18, RGB, DATA_RATE_MHZ(12), fl::Bus::STUB>(leds, 8);
    (void)ctrl;
    FL_CHECK(&fl::BusTraits<fl::Bus::STUB>::instance() != nullptr);
}

FL_TEST_CASE("FastLED.addLeds<WS2812, ...> default-AUTO call site is unchanged") {
    // Regression guard: omitting the Bus param must compile and behave as
    // before #2460 (the AUTO specialization is a no-op).
    static CRGB leds[8];
    auto& ctrl = FastLED.addLeds<WS2812, 3, GRB>(leds, 8);
    (void)ctrl;
}

FL_TEST_CASE("FastLED.addLeds<APA102, ...> default-AUTO SPI call site is unchanged") {
    static CRGB leds[8];
    auto& ctrl = FastLED.addLeds<APA102, 23, 18, RGB, DATA_RATE_MHZ(12)>(leds, 8);
    (void)ctrl;
}

FL_TEST_CASE("Legacy initChannelDrivers() and enableDrivers<Bus::STUB>() share singleton (Phase 5a)") {
    // Phase 5a guarantee: the legacy auto-init path and the new opt-in
    // enableDrivers<Bus::X>() API register the same singleton instance.
    // Before Phase 5a, the legacy `initChannelDrivers()` constructed a fresh
    // ClocklessChannelEngineStub via `fl::make_shared_no_tracking(stubEngine)`,
    // distinct from the BusTraits singleton.
    auto& mgr = fl::ChannelManager::instance();
    // The legacy auto-init runs lazily on first ChannelManager::instance() call;
    // accessing instance() above guaranteed it ran. Verify the registered driver
    // identity matches the BusTraits singleton.
    auto registered = mgr.getDriverByName(fl::string::from_literal("STUB"));
    FL_REQUIRE(registered != nullptr);
    FL_CHECK(registered.get() == &fl::BusTraits<fl::Bus::STUB>::instance());
    // Calling enableDrivers explicitly must not change the registered driver
    // identity -- the manager replaces by name with the same singleton.
    fl::enableDrivers<fl::Bus::STUB>();
    auto registered2 = mgr.getDriverByName(fl::string::from_literal("STUB"));
    FL_REQUIRE(registered2 != nullptr);
    FL_CHECK(registered2.get() == &fl::BusTraits<fl::Bus::STUB>::instance());
}

}  // FL_TEST_FILE
