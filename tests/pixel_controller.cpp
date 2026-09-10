#include "FastLED.h"  // FL_DITHER_ENABLE_MIN_REFRESH_HZ
#include "pixel_controller.h"
#include "fl/math/math.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("PixelController uses the same temporal dither phase for multiple controllers") {
    CRGB pixel(40, 40, 40);
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    adjustment.premixed = CRGB(24, 24, 24);

    PixelController<RGB> first(&pixel, 1, adjustment, BINARY_DITHER);
    PixelController<RGB> second(&pixel, 1, adjustment, BINARY_DITHER);

    FL_CHECK_EQ(first.d[0], second.d[0]);
    FL_CHECK_EQ(first.d[1], second.d[1]);
    FL_CHECK_EQ(first.d[2], second.d[2]);
}

FL_TEST_CASE("PixelController advances temporal dither phase per frame") {
    CRGB pixel(1, 1, 1);
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    adjustment.premixed = CRGB(32, 32, 32);

    fl::u8 frame_phases[8];
    for (fl::u8 frame = 0; frame < 8; ++frame) {
        fl::detail::advanceDitherFrame();
        PixelController<RGB> pixels(&pixel, 1, adjustment, BINARY_DITHER);
        frame_phases[frame] = pixels.d[0];
    }

    for (fl::u8 frame = 1; frame < 8; ++frame) {
        FL_CHECK_NE(frame_phases[frame - 1], frame_phases[frame]);
    }

    PixelController<RGB> pixels(&pixel, 1, adjustment, BINARY_DITHER);
    if (pixels.d[0] == pixels.e[0] - pixels.d[0]) {
        fl::detail::advanceDitherFrame();
        pixels.init_binary_dithering();
    }
    const fl::u8 frame_start = pixels.d[0];
    pixels.stepDithering();
    FL_CHECK_EQ(pixels.d[0], pixels.e[0] - frame_start);
    FL_CHECK_NE(pixels.d[0], frame_start);
    pixels.stepDithering();
    FL_CHECK_EQ(pixels.d[0], frame_start);
}

// ---------------------------------------------------------------------------
// The black floor and the low-code bias (#4156 R8, P8 #4042)
//
// R8 asks for the black floor, the luminance-error denominator and the
// unsupported low-light region to be defined rather than inferred from wider
// arithmetic. These measure the shipped path instead of arguing about it.
//
// Sums are taken over a full eight-frame dither cycle, which makes them
// independent of the phase the test happens to start on.
// ---------------------------------------------------------------------------

namespace {

const int kDitherCycle = 8;

// Total emitted code across one dither cycle. The mean light is this over
// `kDitherCycle * 255` of full scale.
int cycleSum(fl::u8 value, fl::u8 premixed) {
    int sum = 0;
    for (int frame = 0; frame < kDitherCycle; ++frame) {
        CRGB pixel(value, value, value);
        ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
        adjustment.premixed = CRGB(premixed, premixed, premixed);
        fl::detail::advanceDitherFrame();
        PixelController<RGB> pixels(&pixel, 1, adjustment, BINARY_DITHER);
        sum += pixels.loadAndScale0();
    }
    return sum;
}

// What the cycle should sum to if the path were exact.
//
// Floating point on purpose. An integer version truncates, and the row this
// is compared against most -- premixed 16, source code 1 -- has an exact
// ideal of 0.502 codes. Truncating that to 0 turns a +99% error into a
// division by zero and reads as though the error were unbounded.
double idealSum(fl::u8 value, fl::u8 premixed) {
    return static_cast<double>(kDitherCycle) * value * premixed / 255.0;
}

} // namespace

FL_TEST_CASE("Dither - a source code of zero is never lifted off the floor") {
    // `dither()` is `b ? qadd8(b, d) : 0`, so black is excluded by
    // construction. The consequence is the black floor R8 asks to have
    // defined: nothing below one source code is reachable at any brightness,
    // any refresh rate, or any dither cycle length. R8's example -- an
    // identity linear16 input of 1/65535, which is 0.0039 of an 8-bit code --
    // emits zero forever, and its 100% luminance error is not a cadence
    // problem that a longer cycle could fix.
    const fl::u8 premixed[] = {255, 128, 64, 32, 16, 4, 1};
    for (fl::u8 scale : premixed) {
        FL_CHECK_EQ(cycleSum(0, scale), 0);
    }
    // And one source code above the floor is reachable, so the floor is the
    // quantization of the source and not a dead zone above it.
    FL_CHECK_GT(cycleSum(1, 255), 0);
    FL_CHECK_GT(cycleSum(1, 16), 0);
}

FL_TEST_CASE("Dither - sub-code precision is recovered above the floor") {
    // The thing dithering is for: at 1/16 brightness a source code of 1 wants
    // an output of 0.06 codes, which no single frame can emit. The cycle emits
    // code 1 on one frame of eight and zero on the rest, so the mean lands
    // between two output codes.
    const int sum = cycleSum(1, 16);
    FL_CHECK_GT(sum, 0);
    FL_CHECK_LT(sum, kDitherCycle);
}

FL_TEST_CASE("Dither - the lowest codes render brighter than they should") {
    // The correction that makes scale8's truncation round-to-nearest is a
    // constant addition of about half a dither quantum, and at the bottom of
    // the range that is a large fraction of the value itself. Measured over a
    // full cycle, against the exact product:
    //
    //   premixed  value  emitted/ideal
    //   255       1      11 / 8      (+37.5%)
    //   255       2      19 / 16     (+18.8%)
    //   16        1       1 / 0.502  (+99%)
    //
    // This is not a rounding artefact of the measurement -- the sum is over a
    // whole cycle, so it is the time-averaged light. It is the reason R8 says
    // a quantized reference can conceal optical error: compared against an
    // 8-bit reference these all match, and compared against the light they
    // are asking for they do not.
    FL_CHECK_EQ(cycleSum(1, 255), 11);
    FL_CHECK_EQ(idealSum(1, 255), 8.0);
    FL_CHECK_EQ(cycleSum(2, 255), 19);
    FL_CHECK_EQ(idealSum(2, 255), 16.0);

    // And the low-brightness row the table quotes, where the ideal is a
    // fraction of a code rather than a whole one.
    FL_CHECK_EQ(cycleSum(1, 16), 1);
    FL_CHECK_GT(idealSum(1, 16), 0.50);
    FL_CHECK_LT(idealSum(1, 16), 0.51);

    // The bias shrinks as a fraction as the value grows, which is why it is
    // invisible anywhere but the bottom.
    FL_CHECK_GT(cycleSum(1, 255), idealSum(1, 255) * 1.30);
    FL_CHECK_LT(cycleSum(64, 255), idealSum(64, 255) * 1.02);
}

FL_TEST_CASE("Dither - at full scale the correction has nothing to correct") {
    // `scale8(i, 255)` is exact under FASTLED_SCALE8_FIXED: `(i * 256) >> 8`
    // is `i`. So at premixed 255 no fractional precision is lost, there is
    // nothing for a rounding correction to recover, and every code the dither
    // adds is pure gain. The cycle sum is above the exact product rather than
    // equal to it, for every low code.
    for (fl::u8 value = 1; value <= 8; ++value) {
        FL_CHECK_EQ(fl::scale8(value, 255), value);
        FL_CHECK_GT(cycleSum(value, 255), idealSum(value, 255));
    }
    // Disabling the dither is what makes the path exact there.
    for (fl::u8 value = 1; value <= 8; ++value) {
        CRGB pixel(value, value, value);
        ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
        adjustment.premixed = CRGB(255, 255, 255);
        PixelController<RGB> pixels(&pixel, 1, adjustment, DISABLE_DITHER);
        FL_CHECK_EQ(pixels.loadAndScale0(), value);
    }
}

// ---------------------------------------------------------------------------
// The cadence half of #4156 R8, which is also what P8's temporal dither waits
// on (#4042).
//
// R8 asks for "cadence, observation window, and unsupported low-light region"
// to be defined. #4269 answered the black floor. This is the cadence, and it
// turns out the two numbers that decide it disagree with each other by 4x.
// ---------------------------------------------------------------------------

FL_TEST_CASE("Dither - the cycle rate at the enable threshold is below the file's own floor") {
    // The cycle length is *derived* from a cadence assumption:
    //
    //   MAX_LIKELY_UPDATE_RATE_HZ     400
    //   MIN_ACCEPTABLE_DITHER_RATE_HZ  50
    //   UPDATES_PER_FULL_DITHER_CYCLE  400 / 50 = 8
    //
    // So eight frames is the number that makes a 400 Hz refresh complete a
    // cycle at 50 Hz. That is coherent.
    //
    // What enables dithering is not 400. `CFastLED::show()` and `showColor()`
    // each carry `if (mNFPS < 100) { pCur->setDither(0); }` -- a bare literal,
    // twice, with no reference to the constants above. At 100 FPS an
    // eight-frame cycle completes at 12.5 Hz, which is a quarter of the
    // file's own MIN_ACCEPTABLE_DITHER_RATE_HZ and sits near the peak of human
    // flicker sensitivity.
    //
    // The guide above states two different 50 Hz conditions and the code
    // implements the weaker one: "at refresh rates above ~50Hz, human vision
    // integrates these variations" is about the *refresh*, while "8-frame
    // cycle at 400Hz = 50Hz complete cycle" is about the *cycle*. What the eye
    // integrates is the modulation, which is at the cycle rate.
    //
    // This case records the arithmetic rather than asserting the intent.
    // Raising the threshold would disable dithering for most sketches, which
    // is a decision and not a cleanup -- so if either number moves, this fails
    // and whoever moved it writes down why.
    FL_CHECK_EQ(UPDATES_PER_FULL_DITHER_CYCLE, 8);
    FL_CHECK_EQ(MIN_ACCEPTABLE_DITHER_RATE_HZ, 50);
    FL_CHECK_EQ(MAX_LIKELY_UPDATE_RATE_HZ, 400);

    // The threshold `show()` actually applies -- the shipped constant, not a
    // copy of it. It used to be a bare `100` at both call sites and a second
    // bare `100` here, so changing the threshold moved the behaviour and
    // failed nothing. That is the defect FastLED#4279 fixed elsewhere: a
    // test that reads a copy is not reading the thing it names.
    const int kDitherEnableFps = FL_DITHER_ENABLE_MIN_REFRESH_HZ;
    // Pinned separately so a change to the threshold fails *here*, where the
    // cadence consequence is written down, rather than only wherever it is
    // next used.
    FL_CHECK_EQ(kDitherEnableFps, 100);

    // In floating point, because the number is 12.5 and the point of this
    // case is to record the cadence rather than a rounded stand-in for it.
    // An integer division here reports 12, which is a different claim.
    const float cycle_hz_at_threshold =
        static_cast<float>(kDitherEnableFps) /
        static_cast<float>(UPDATES_PER_FULL_DITHER_CYCLE);
    FL_CHECK_LT(fl::fabsf(cycle_hz_at_threshold - 12.5f), 1e-6f);

    // Four times short, by the file's own floor.
    FL_CHECK_LT(cycle_hz_at_threshold,
                static_cast<float>(MIN_ACCEPTABLE_DITHER_RATE_HZ));
    FL_CHECK_EQ(MAX_LIKELY_UPDATE_RATE_HZ / kDitherEnableFps, 4);

    // And the refresh that would actually reach the floor is the one the
    // cycle length was derived from.
    FL_CHECK_EQ(MIN_ACCEPTABLE_DITHER_RATE_HZ * UPDATES_PER_FULL_DITHER_CYCLE,
                MAX_LIKELY_UPDATE_RATE_HZ);
}

} // FL_TEST_FILE
