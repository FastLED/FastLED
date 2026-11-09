/// @file driver_bindings.cpp
/// @brief FastLED.enable<Bus>() binds a driver lazily; add(cfg) links no others.

#include "FastLED.h"
#include "fl/channels/driver_bindings.h"
#include "fl/channels/manager.h"
#include "fl/stl/string.h"
#include "platforms/shared/bitbang/bus_traits.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

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

}  // FL_TEST_FILE
