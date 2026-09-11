/// @file tests/fl/channels/dither_frame.cpp
/// @brief The shared temporal-dither phase: who advances it, and when.
///
/// `docs/color-pipeline-contracts.md` asks for this directly:
///
///   State advances on presentation according to the documented driver
///   contract, with explicit tests for dropped submissions and irregular
///   dwell.
///
/// There were none. `tests/pixel_controller.cpp` drives `advanceDitherFrame()`
/// to walk a cycle, but nothing observed who advances the phase or what
/// happens to a frame that never reaches a driver.

#include "FastLED.h"
#include "fl/channels/all_drivers.h"
#include "fl/channels/channel.h"
#include "fl/channels/data.h"
#include "fl/channels/dither_frame.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/system/engine_events.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

/// Counts submissions so a test can tell "presented" from "attempted".
class CountingDriver : public IChannelDriver {
public:
    int frames = 0;

    bool canHandle(const ChannelDataPtr& data) const override {
        (void)data;
        return true;
    }
    void enqueue(ChannelDataPtr channelData) override {
        (void)channelData;
        ++frames;
    }
    void show() override {}
    DriverState poll() override { return DriverState::READY; }
    fl::string getName() const override {
        return fl::string::from_literal("DITHER_COUNT");
    }
    Capabilities getCapabilities() const override { return Capabilities(true, true); }
};

ChannelConfig configOn(int pin, fl::span<CRGB> leds) {
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    return ChannelConfig(pin, timing, leds, RGB);
}

}  // namespace

FL_TEST_CASE("onBeginFrame advances the phase once, whatever the channel count") {
    // `advanceDitherFrame`'s contract is "once per logical frame, not once per
    // controller". The frame boundary is where it lives, and the count of
    // channels alive at that moment must not enter into it -- if it did, two
    // channels would see different phases in one frame and the 8-frame average
    // each depends on would be computed over interleaved phases.
    auto& mgr = ChannelManager::instance();
    mgr.clearAllDrivers();
    mgr.addDriver(9200, fl::make_shared<CountingDriver>());
    auto cleanup = fl::make_scope_exit([&mgr]() { mgr.clearAllDrivers(); });

    CRGB a[1] = {CRGB(40, 40, 40)};
    CRGB b[1] = {CRGB(40, 40, 40)};
    CRGB c[1] = {CRGB(40, 40, 40)};
    ChannelPtr ch_a = Channel::create(configOn(60, fl::span<CRGB>(a, 1)));
    ChannelPtr ch_b = Channel::create(configOn(61, fl::span<CRGB>(b, 1)));
    ChannelPtr ch_c = Channel::create(configOn(62, fl::span<CRGB>(c, 1)));
    FL_REQUIRE(ch_a != nullptr);
    FL_REQUIRE(ch_b != nullptr);
    FL_REQUIRE(ch_c != nullptr);

    const u8 before = fl::detail::ditherFrame();
    fl::EngineEvents::onBeginFrame();
    // u8 arithmetic: the difference is taken modulo 256, so a wrap reads as 1.
    FL_CHECK_EQ((int)(u8)(fl::detail::ditherFrame() - before), 1);
}

FL_TEST_CASE("showLeds() is its own logical frame, and advances the phase") {
    // Worth pinning because the two entry points differ and the difference is
    // easy to read as a bug. `CFastLED::show()` calls onBeginFrame() once and
    // then drives each controller through showLedsInternal(), which does not
    // advance. `CLEDController::showLeds()` -- the 3.8.x compatibility path a
    // Channel inherits -- calls onBeginFrame() itself, so N such calls are N
    // logical frames rather than one frame over N controllers.
    //
    // I wrote this case backwards first, expecting three showLeds() calls to
    // share one phase, and measured four advances instead of one. The
    // expectation was wrong, not the code.
    auto& mgr = ChannelManager::instance();
    mgr.clearAllDrivers();
    auto driver = fl::make_shared<CountingDriver>();
    mgr.addDriver(9200, driver);
    auto cleanup = fl::make_scope_exit([&mgr]() { mgr.clearAllDrivers(); });

    CRGB leds[1] = {CRGB(40, 40, 40)};
    ChannelPtr ch = Channel::create(configOn(63, fl::span<CRGB>(leds, 1)));
    FL_REQUIRE(ch != nullptr);

    const u8 before = fl::detail::ditherFrame();
    ch->showLeds(255);
    ch->showLeds(255);
    FL_CHECK_EQ((int)(u8)(fl::detail::ditherFrame() - before), 2);
    FL_CHECK_EQ(driver->frames, 2);
}

FL_TEST_CASE("[#4347] a dropped submission still consumes a dither phase") {
    // Characterising, not endorsing. The contracts addendum requires state to
    // advance "on presentation"; the advance happens at the frame boundary,
    // before encode and before enqueue, so a frame that never reaches a driver
    // spends a phase anyway.
    //
    // Pinned so that implementing the requirement is a visible change to this
    // file rather than a silent one. See FastLED#4347 for why it is not fixed
    // here: advancing on presentation means the counter cannot live at the
    // frame boundary at all.
    auto& mgr = ChannelManager::instance();
    mgr.clearAllDrivers();
    auto driver = fl::make_shared<CountingDriver>();
    mgr.addDriver(9200, driver);
    auto cleanup = fl::make_scope_exit([&mgr]() { mgr.clearAllDrivers(); });

    CRGB leds[1] = {CRGB(40, 40, 40)};
    ChannelPtr ch = Channel::create(configOn(64, fl::span<CRGB>(leds, 1)));
    FL_REQUIRE(ch != nullptr);

    const u8 before = fl::detail::ditherFrame();
    ch->setEnabled(false);          // this frame will not be presented
    ch->showLeds(255);
    ch->setEnabled(true);

    FL_CHECK_EQ(driver->frames, 0);                                 // nothing presented
    FL_CHECK_EQ((int)(u8)(fl::detail::ditherFrame() - before), 1);   // phase spent anyway
}

}
