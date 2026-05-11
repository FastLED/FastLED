/// @file channel_typed.cpp
/// @brief Phase 3b tests -- compile-time bus/chipset enforcement
///        (`Channel<Bus, Chipset>`, `FastLED.add<Bus, Chipset>(cfg)`).
///
/// These tests verify both runtime behavior (positive: platform-default path
/// resolves correctly) and compile-time behavior (negative: mismatched
/// bus/chipset combinations fail `static_assert`).
///
/// Compile-fail negatives are exercised via `FL_STATIC_ASSERT` on the
/// `BusSupports` predicate -- the same predicate that the `FastLED.add<>`
/// template's `static_assert` uses. This proves that the contract that
/// rejects nonsense combinations *would* fire at the `add<>()` call site
/// without requiring the test to actually fail compilation.
///
/// See issue #2428.

#include "FastLED.h"

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/channel.h"
#include "fl/channels/channel_typed.h"
#include "fl/channels/config.h"
#include "fl/channels/manager.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/spi.h"
#include "fl/gfx/crgb.h"
#include "fl/stl/static_assert.h"
#include "platforms/is_platform.h"
#include "platforms/stub/bus_traits.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// =========================================================================
// Compile-time invariants -- Phase 3b core contract
// =========================================================================

// Positive: stub bus supports clockless chipsets.
FL_STATIC_ASSERT(
    fl::BusSupports<fl::Bus::STUB, fl::ClocklessChipset>::value,
    "Phase 3b regression: STUB bus must support ClocklessChipset");

// Negative: stub bus does NOT support SPI chipsets -- the static_assert in
// `FastLED.add<Bus::STUB>(spi_cfg)` will fire on this predicate.
FL_STATIC_ASSERT(
    !fl::BusSupports<fl::Bus::STUB, fl::SpiChipsetConfig>::value,
    "Phase 3b regression: STUB bus must NOT support SpiChipsetConfig "
    "(the negative-path static_assert relies on this)");

// DefaultBus<ClocklessChipset> on the host build is Bus::STUB.
FL_STATIC_ASSERT(
    fl::DefaultBus<fl::ClocklessChipset>::value == fl::Bus::STUB,
    "Phase 3b regression: host platform default clockless bus must be STUB");

// Resolved bus when caller passes Bus::AUTO == DefaultBus value.
FL_STATIC_ASSERT(
    fl::TypedChannel<fl::Bus::AUTO, fl::ClocklessChipset>::kBus == fl::Bus::STUB,
    "Phase 3b regression: TypedChannel<Bus::AUTO, ClocklessChipset>::kBus "
    "must resolve to the platform default (STUB on host)");

// Explicit bus selection is preserved.
FL_STATIC_ASSERT(
    fl::TypedChannel<fl::Bus::STUB, fl::ClocklessChipset>::kBus == fl::Bus::STUB,
    "Phase 3b regression: explicit Bus argument must be preserved");

// =========================================================================
// Runtime tests
// =========================================================================

FL_TEST_CASE("Phase 3b: ChannelConfigOf<ClocklessChipset> implicit conversion to ChannelConfig") {
    CRGB workspace[4];
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ClocklessChipset clockless(4, timing);

    fl::ChannelConfigOf<fl::ClocklessChipset> typed{
        clockless, fl::span<CRGB>(workspace, 4), GRB};

    // Implicit conversion to the erased form preserves payload.
    fl::ChannelConfig erased = typed;
    FL_CHECK(erased.isClockless());
    FL_CHECK_FALSE(erased.isSpi());
    FL_CHECK(erased.getDataPin() == 4);
    FL_CHECK(erased.rgb_order == GRB);
    FL_CHECK(erased.mLeds.size() == 4u);
}

FL_TEST_CASE("Phase 3b: TypedChannel<Bus::AUTO, ClocklessChipset>::create resolves to host default") {
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

FL_TEST_CASE("#2459: TypedChannel<Bus::STUB, ClocklessChipset>::create binds the STUB driver") {
    CRGB workspace[3];
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfigOf<fl::ClocklessChipset> cfg{
        fl::ClocklessChipset(8, timing),
        fl::span<CRGB>(workspace, 3),
        RGB};

    // Compile-time path: TypedChannel is the single template entry point.
    auto channel = fl::TypedChannel<fl::Bus::STUB, fl::ClocklessChipset>::create(cfg);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->isClockless());
    FL_CHECK(channel->getPin() == 8);

    channel->removeFromDrawList();
}

FL_TEST_CASE("#2459: runtime FastLED.add(cfg) with cfg.options.mBus = Bus::STUB") {
    CRGB workspace[5];
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfigOf<fl::ClocklessChipset> typed_cfg{
        fl::ClocklessChipset(2, timing),
        fl::span<CRGB>(workspace, 5),
        RGB};

    // Runtime path: erase, set mBus, dispatch via non-template FastLED.add.
    fl::ChannelConfig erased = typed_cfg.toErased();
    erased.options.mBus = fl::Bus::STUB;
    auto channel = FastLED.add(erased);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->isClockless());
    FL_CHECK(channel->getPin() == 2);

    channel->removeFromDrawList();
}

// =========================================================================
// Compile-fail proof: the negative branch IS reachable.
//
// We don't actually instantiate `FastLED.add<Bus::STUB>(spi_cfg)` because that
// would (correctly) fail the build. Instead we verify the *predicate* that
// would fire is `false` -- so the static_assert message is the failure mode.
// =========================================================================

FL_STATIC_ASSERT(
    !fl::BusSupports<fl::Bus::STUB, fl::SpiChipsetConfig>::value,
    "Phase 3b contract: FastLED.add<Bus::STUB>(SpiChipsetConfig{...}) must "
    "fail to compile -- the predicate guarding the static_assert is false.");

// Bonus: explicit cross-check against PARLIO+SpiChipsetConfig (the example
// from the issue body). This predicate is also `false`, which is exactly
// what makes `FastLED.add<Bus::PARLIO>(spi_cfg)` a compile error.
FL_STATIC_ASSERT(
    !fl::BusSupports<fl::Bus::PARLIO, fl::SpiChipsetConfig>::value,
    "Phase 3b contract: FastLED.add<Bus::PARLIO>(SpiChipsetConfig{...}) must "
    "fail to compile -- BusSupports<PARLIO, SpiChipsetConfig> is false.");

}  // FL_TEST_FILE
