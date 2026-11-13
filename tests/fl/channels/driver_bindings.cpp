/// @file driver_bindings.cpp
/// @brief FastLED.enable<Bus>() binds a driver lazily; add(cfg) links no others.

#include "FastLED.h"
#include "fl/channels/driver_bindings.h"
#include "fl/channels/manager.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/string.h"
#include "platforms/shared/bitbang/bus_traits.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

// Runs first: needs DriverBindings empty (3.10.9 compat for add(cfg)).
FL_TEST_CASE("FastLED.add(cfg) with no enable<Bus>() registers the default driver") {
    // Precondition for the legacy path: nothing opted in explicitly.
    FL_REQUIRE(fl::DriverBindings::instance().empty());
    fl::ChannelManager& mgr = fl::ChannelManager::registry();
    mgr.clearAllDrivers();

    CRGB leds[4];
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig cfg(fl::ClocklessChipset(3, timing),
                          fl::span<CRGB>(leds, 4), RGB);
    auto channel = FastLED.add(cfg);
    FL_REQUIRE(channel != nullptr);

    const fl::Bus def = fl::DefaultBus<fl::ClocklessChipset>::value;
    FL_CHECK(mgr.findDriverByName(fl::string::from_literal(
                 fl::busDriverName(def, 0, false))) != nullptr);
    FL_CHECK(mgr.getDriverCount() > 0);

    channel->removeFromDrawList();
}

FL_TEST_CASE("DriverBindings - enable<Bus>() defers registration until bind") {
    fl::ChannelManager& mgr = fl::ChannelManager::registry();
    mgr.clearAllDrivers();
    const fl::string name = fl::string::from_literal("BIT_BANG");

    FastLED.enable<fl::Bus::BIT_BANG>().enable<fl::Bus::BIT_BANG>();  // chains, idempotent
    // Enabling only links a node: nothing is constructed or registered yet.
    FL_CHECK(mgr.findDriverByName(name) == nullptr);

    FL_CHECK(fl::DriverBindings::instance().bind(fl::Bus::BIT_BANG, 0));
    FL_CHECK(mgr.findDriverByName(name) != nullptr);

    // A cleared manager gets the driver back on the next bind.
    mgr.clearAllDrivers();
    FL_CHECK(fl::DriverBindings::instance().bind(fl::Bus::AUTO, 0));
    FL_CHECK(mgr.findDriverByName(name) != nullptr);
}

FL_TEST_CASE("DriverBindings - a bus that was never enabled does not match") {
    FL_CHECK_FALSE(fl::DriverBindings::instance().bind(fl::Bus::RMT, 0));
}

FL_TEST_CASE("DriverBindings - enable<Bus::AUTO>() names the platform default") {
    using AutoBinding = fl::detail::BusBinding<fl::Bus::AUTO, fl::ClocklessChipset, 0>;
    const fl::Bus resolved = AutoBinding::kBus;
    FL_CHECK(resolved == fl::DefaultBus<fl::ClocklessChipset>::value);
}

namespace {
struct NoDefaultBusChipset {};  // no DefaultBus specialization (like Zephyr)
}

FL_TEST_CASE("DriverBindings - platforms without a DefaultBus bind nothing") {
    FL_STATIC_ASSERT(!fl::detail::HasDefaultBus<NoDefaultBusChipset>::value,
                  "incomplete DefaultBus must be detected");
    FL_STATIC_ASSERT(fl::detail::HasDefaultBus<fl::ClocklessChipset>::value,
                  "host specializes DefaultBus<ClocklessChipset>");
    FL_CHECK_FALSE(fl::detail::bindDefaultDriver<NoDefaultBusChipset>());
}

}  // FL_TEST_FILE
