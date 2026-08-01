/// Regression test for FastLED#691 -- comparing two CHSV values.
///
/// Reported in 2018: `CHSV(0,0,0) == CHSV(128,0,0)` returned true, because
/// CHSV had no operator== and the operands were implicitly converted to CRGB
/// first. Both convert to black, so any two CHSV values with value==0 (or any
/// pair that happens to render to the same RGB) compared equal.
///
/// A comparison that silently changes colour space is the same failure shape
/// as a check that reports success it has not earned: it answers a question
/// the caller did not ask.

#include "test.h"

#include "fl/gfx/hsv.h"

FL_TEST_CASE("CHSV equality compares HSV fields, not the RGB rendering") {
    // The exact case from the issue. Both render to black in RGB.
    const CHSV a(0, 0, 0);
    const CHSV b(128, 0, 0);

    FL_CHECK_FALSE(a == b);
    FL_CHECK(a != b);
}

FL_TEST_CASE("CHSV equality is true only for identical fields") {
    const CHSV a(10, 20, 30);
    const CHSV same(10, 20, 30);
    const CHSV diff_hue(11, 20, 30);
    const CHSV diff_sat(10, 21, 30);
    const CHSV diff_val(10, 20, 31);

    FL_CHECK(a == same);
    FL_CHECK_FALSE(a != same);

    FL_CHECK_FALSE(a == diff_hue);
    FL_CHECK_FALSE(a == diff_sat);
    FL_CHECK_FALSE(a == diff_val);

    FL_CHECK(a != diff_hue);
    FL_CHECK(a != diff_sat);
    FL_CHECK(a != diff_val);
}

FL_TEST_CASE("CHSV hues that render alike still differ") {
    // Saturation 0 makes hue irrelevant to the RGB rendering, so these are
    // the pairs the old CRGB-conversion path collapsed together.
    const CHSV red_ish(0, 0, 200);
    const CHSV blue_ish(160, 0, 200);

    FL_CHECK_FALSE(red_ish == blue_ish);
    FL_CHECK(red_ish != blue_ish);
}
