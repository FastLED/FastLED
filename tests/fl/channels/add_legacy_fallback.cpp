/// @file add_legacy_fallback.cpp
/// @brief FastLED.add(cfg) without any enable<Bus>() still drives the strip.
///
/// 3.10.9 sketches call FastLED.add(cfg) with no driver opt-in. When no
/// driver was enabled, add(cfg) must register the platform default driver
/// for the config's chipset family, as 3.10.9 did, instead of leaving the
/// channel unserved.

#include "FastLED.h"
#include "fl/channels/bus.h"
#include "fl/channels/driver_bindings.h"
#include "fl/channels/manager.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

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

}  // FL_TEST_FILE
