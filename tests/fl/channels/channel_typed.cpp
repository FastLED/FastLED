/// @file channel_typed.cpp
/// @brief Compile-time bus/chipset enforcement tests.

#include "FastLED.h"

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/channel.h"
#include "fl/channels/channel_typed.h"
#include "fl/channels/config.h"
#include "fl/channels/manager.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/gfx/crgb.h"
#include "fl/stl/static_assert.h"
#include "platforms/shared/bitbang/bus_traits.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_STATIC_ASSERT(
    fl::BusSupports<fl::Bus::BIT_BANG, fl::ClocklessChipset>::value,
    "BIT_BANG bus must support ClocklessChipset");

FL_STATIC_ASSERT(
    fl::BusSupports<fl::Bus::BIT_BANG, fl::SpiChipsetConfig>::value,
    "BIT_BANG bus must support SpiChipsetConfig");

FL_STATIC_ASSERT(
    fl::DefaultBus<fl::ClocklessChipset>::value == fl::Bus::BIT_BANG,
    "Host platform default clockless bus must be BIT_BANG");

FL_STATIC_ASSERT(
    fl::TypedChannel<fl::Bus::AUTO, fl::ClocklessChipset>::kBus == fl::Bus::BIT_BANG,
    "TypedChannel<Bus::AUTO, ClocklessChipset>::kBus must resolve to the host default");

FL_STATIC_ASSERT(
    fl::TypedChannel<fl::Bus::BIT_BANG, fl::ClocklessChipset>::kBus == fl::Bus::BIT_BANG,
    "Explicit Bus argument must be preserved");

FL_TEST_CASE("ChannelConfigOf<ClocklessChipset> implicit conversion to ChannelConfig") {
    CRGB workspace[4];
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ClocklessChipset clockless(4, timing);

    fl::ChannelConfigOf<fl::ClocklessChipset> typed{
        clockless, fl::span<CRGB>(workspace, 4), GRB};

    fl::ChannelConfig erased = typed;
    FL_CHECK(erased.isClockless());
    FL_CHECK_FALSE(erased.isSpi());
    FL_CHECK(erased.getDataPin() == 4);
    FL_CHECK(erased.rgb_order == GRB);
    FL_CHECK(erased.mLeds.size() == 4u);
}

FL_TEST_CASE("TypedChannel<Bus::AUTO, ClocklessChipset>::create resolves to host default") {
    CRGB workspace[2];
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfigOf<fl::ClocklessChipset> cfg{
        fl::ClocklessChipset(7, timing),
        fl::span<CRGB>(workspace, 2),
        RGB};

    auto channel = fl::TypedChannel<fl::Bus::AUTO, fl::ClocklessChipset>::create(cfg);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->getPin() == 7);
    FL_CHECK(channel->isClockless());
}

FL_TEST_CASE("TypedChannel<Bus::BIT_BANG, ClocklessChipset>::create binds BIT_BANG") {
    CRGB workspace[3];
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfigOf<fl::ClocklessChipset> cfg{
        fl::ClocklessChipset(8, timing),
        fl::span<CRGB>(workspace, 3),
        RGB};

    auto channel = fl::TypedChannel<fl::Bus::BIT_BANG, fl::ClocklessChipset>::create(cfg);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->isClockless());
    FL_CHECK(channel->getPin() == 8);

    channel->removeFromDrawList();
}

FL_TEST_CASE("runtime FastLED.add(cfg) with cfg.options.mBus = Bus::BIT_BANG") {
    CRGB workspace[5];
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfigOf<fl::ClocklessChipset> typed_cfg{
        fl::ClocklessChipset(2, timing),
        fl::span<CRGB>(workspace, 5),
        RGB};

    fl::ChannelConfig erased = typed_cfg.toErased();
    erased.options.mBus = fl::Bus::BIT_BANG;
    auto channel = FastLED.add(erased);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->isClockless());
    FL_CHECK(channel->getPin() == 2);

    channel->removeFromDrawList();
}

FL_STATIC_ASSERT(
    !fl::BusSupports<fl::Bus::RMT, fl::SpiChipsetConfig>::value,
    "RMT + SpiChipsetConfig must fail the BusSupports predicate on host");

}  // FL_TEST_FILE
