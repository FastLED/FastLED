/// Property tests for the lerp family in src/lib8tion.h.
///
/// Issue #875 reported that lerp8by8 "hangs" an ESP32. The function is a
/// branch and two arithmetic ops with no loop, so it cannot hang -- but that
/// was never demonstrated either way, because the whole lerp family had no
/// tests at all. Its sibling blend8 has ten (tests/platforms/shared/math8.cpp).
///
/// These assert the properties any interpolator must hold, so a future
/// regression in scale8/scale16 shows up here rather than as drifting colour.

#include "lib8tion.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("lerp8by8 returns the endpoints exactly") {
    for (int a = 0; a < 256; a++) {
        for (int b = 0; b < 256; b++) {
            FL_CHECK_EQ(lerp8by8(fl::u8(a), fl::u8(b), 0), fl::u8(a));
            FL_CHECK_EQ(lerp8by8(fl::u8(a), fl::u8(b), 255), fl::u8(b));
        }
    }
}

FL_TEST_CASE("lerp8by8 never leaves the interval") {
    // The failure this guards is a scale8 change overshooting, which would
    // show up as a colour briefly jumping past its target during a blend.
    int violations = 0;
    for (int a = 0; a < 256; a++) {
        for (int b = 0; b < 256; b++) {
            const int lo = a < b ? a : b;
            const int hi = a < b ? b : a;
            for (int frac = 0; frac < 256; frac += 17) {
                const int r = lerp8by8(fl::u8(a), fl::u8(b), fl::u8(frac));
                if (r < lo || r > hi) { violations++; }
            }
        }
    }
    FL_CHECK_EQ(violations, 0);
}

FL_TEST_CASE("lerp8by8 is monotonic in the fraction") {
    int violations = 0;
    for (int a = 0; a < 256; a += 5) {
        for (int b = 0; b < 256; b += 5) {
            int prev = lerp8by8(fl::u8(a), fl::u8(b), 0);
            for (int frac = 1; frac < 256; frac++) {
                const int cur = lerp8by8(fl::u8(a), fl::u8(b), fl::u8(frac));
                if (b >= a ? (cur < prev) : (cur > prev)) { violations++; }
                prev = cur;
            }
        }
    }
    FL_CHECK_EQ(violations, 0);
}

FL_TEST_CASE("lerp8by8 between equal endpoints is a constant") {
    for (int a = 0; a < 256; a++) {
        for (int frac = 0; frac < 256; frac += 7) {
            FL_CHECK_EQ(lerp8by8(fl::u8(a), fl::u8(a), fl::u8(frac)), fl::u8(a));
        }
    }
}

FL_TEST_CASE("lerp8by8 midpoint lands mid-interval") {
    FL_CHECK_EQ(lerp8by8(0, 255, 128), 128);
    FL_CHECK_EQ(lerp8by8(255, 0, 128), 127);
    FL_CHECK_EQ(lerp8by8(0, 100, 128), 50);
}

FL_TEST_CASE("the 16-bit lerps return the endpoints exactly") {
    for (long a = 0; a < 65536; a += 257) {
        for (long b = 0; b < 65536; b += 257) {
            FL_CHECK_EQ(lerp16by16(fl::u16(a), fl::u16(b), 0), fl::u16(a));
            FL_CHECK_EQ(lerp16by16(fl::u16(a), fl::u16(b), 65535), fl::u16(b));
            FL_CHECK_EQ(lerp16by8(fl::u16(a), fl::u16(b), 0), fl::u16(a));
            FL_CHECK_EQ(lerp16by8(fl::u16(a), fl::u16(b), 255), fl::u16(b));
        }
    }
}

FL_TEST_CASE("the signed 15-bit lerps return the endpoints exactly") {
    // Domain is +/-16383 per the doc comment ("signed 15-bit values"); the
    // subtraction b - a would overflow across the full i16 range.
    for (long a = -16383; a <= 16383; a += 331) {
        for (long b = -16383; b <= 16383; b += 331) {
            FL_CHECK_EQ(lerp15by8(fl::i16(a), fl::i16(b), 0), fl::i16(a));
            FL_CHECK_EQ(lerp15by8(fl::i16(a), fl::i16(b), 255), fl::i16(b));
            FL_CHECK_EQ(lerp15by16(fl::i16(a), fl::i16(b), 0), fl::i16(a));
            FL_CHECK_EQ(lerp15by16(fl::i16(a), fl::i16(b), 65535), fl::i16(b));
        }
    }
}

} // FL_TEST_FILE
