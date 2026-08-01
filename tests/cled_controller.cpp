/// Regression test for FastLED#1017: FastLED.clear() / clearData() blanked
/// only the first strip when a parallel (multi-lane) controller was in use.
///
/// A parallel controller is registered with the PER-LANE led count --
/// addLeds<NUM_LANES, CHIPSET, PIN, ORDER>(data, nLeds) forwards nLeds -- while
/// the caller's array is NUM_LANES * nLeds. clearLedDataInternal() cleared
/// mLeds.size(), i.e. one lane, and left the rest lit.
///
/// The real multi-lane path is Teensy-only (__FASTLED_HAS_FIBCC is defined
/// only for mxrt1062), so it cannot be instantiated here. What IS portable is
/// the mechanism: a controller reporting lanes() > 1 must have its whole
/// buffer cleared. That is what these exercise.

#include "FastLED.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

namespace {

/// Stands in for a parallel controller: registered with the per-lane count,
/// backed by an array LANES times larger, exactly as the Teensy block
/// controllers are.
class FakeParallelController : public CLEDController {
  public:
    explicit FakeParallelController(int lanes) : mLanes(lanes) {}
    int lanes() FL_NO_EXCEPT override { return mLanes; }
    void showColor(const CRGB &, int, fl::u8) FL_NO_EXCEPT override {}
    void show(const CRGB *, int, fl::u8) FL_NO_EXCEPT override {}
    void init() FL_NO_EXCEPT override {}

  private:
    int mLanes;
};

}  // namespace

FL_TEST_CASE("clearLedData clears every lane of a parallel controller (#1017)") {
    constexpr int kLanes = 4;
    constexpr int kPerLane = 8;
    static CRGB buffer[kLanes * kPerLane];
    for (auto &px : buffer) { px = CRGB::White; }

    FakeParallelController ctrl(kLanes);
    ctrl.setLeds(buffer, kPerLane);  // per-lane count, as addLeds does
    ctrl.clearLedDataInternal();  // what CFastLED::clearData() calls

    int still_lit = 0;
    for (const auto &px : buffer) {
        if (px != CRGB(0, 0, 0)) { ++still_lit; }
    }
    // Before the fix this was 24: lanes 1..3 were untouched.
    FL_CHECK_EQ(still_lit, 0);
}

FL_TEST_CASE("clearLedData on a single-lane controller is unchanged (#1017)") {
    constexpr int kLeds = 8;
    static CRGB buffer[kLeds];
    for (auto &px : buffer) { px = CRGB::White; }

    FakeParallelController ctrl(1);
    ctrl.setLeds(buffer, kLeds);
    ctrl.clearLedDataInternal();  // what CFastLED::clearData() calls

    for (const auto &px : buffer) { FL_CHECK_EQ(px, CRGB(0, 0, 0)); }
}

} // FL_TEST_FILE
