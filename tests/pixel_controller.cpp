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

// ---------------------------------------------------------------------------
// #4347: the phase advances on frame *attempt*, so a frame that is never
// presented still consumes one. Every measurement above sums a *complete*
// eight-frame cycle, which is exactly the condition a phase-correlated drop
// breaks -- and #4347 records that nothing observes the consequence.
//
// These do. A drop pattern that correlates with the phase leaves the
// presented frames sampling a biased subset of the dither offsets, and the
// time-weighted mean moves off the value the dither exists to render.
// ---------------------------------------------------------------------------

namespace {

// Total emitted code over `kDitherCycle` logical frames, counting only those
// the predicate presents. The phase advances either way -- that is the
// behaviour under test, not an approximation of it.
int presentedSum(fl::u8 value, fl::u8 premixed, bool keep_odd_phase,
                 int* out_presented) {
    int sum = 0;
    int presented = 0;
    for (int frame = 0; frame < kDitherCycle; ++frame) {
        fl::detail::advanceDitherFrame();
        const bool odd = (fl::detail::ditherFrame() & 0x01) != 0;
        if (odd != keep_odd_phase) {
            continue;  // dropped: phase consumed, nothing emitted
        }
        CRGB pixel(value, value, value);
        ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
        adjustment.premixed = CRGB(premixed, premixed, premixed);
        PixelController<RGB> pixels(&pixel, 1, adjustment, BINARY_DITHER);
        sum += pixels.loadAndScale0();
        ++presented;
    }
    *out_presented = presented;
    return sum;
}

} // namespace

FL_TEST_CASE("[#4347] dropping every other frame biases the dithered mean") {
    // A quarter brightness, where the dither has real work to do: the exact
    // product is fractional, so the rendered value depends on the offsets
    // averaging out over the cycle.
    const fl::u8 kValue = 9;
    const fl::u8 kPremixed = 64;

    // The baseline: a complete cycle lands near the exact product.
    const double ideal_per_frame = idealSum(kValue, kPremixed) / kDitherCycle;
    const double whole_cycle =
        static_cast<double>(cycleSum(kValue, kPremixed)) / kDitherCycle;
    FL_CHECK_LT(fl::fabs(whole_cycle - ideal_per_frame), 1.0);

    // Now drop by parity of the phase. The low half of the bit-reversed
    // offsets lives on even phases and the high half on odd, so each subset
    // renders a consistently displaced value rather than a noisier one.
    int even_frames = 0;
    int odd_frames = 0;
    const double even_mean =
        static_cast<double>(presentedSum(kValue, kPremixed, false, &even_frames)) /
        even_frames;
    const double odd_mean =
        static_cast<double>(presentedSum(kValue, kPremixed, true, &odd_frames)) /
        odd_frames;

    // Half the cycle survives either way -- the drops are the correlation,
    // not a change in how much light is asked for.
    FL_CHECK_EQ(even_frames, kDitherCycle / 2);
    FL_CHECK_EQ(odd_frames, kDitherCycle / 2);

    // Measured against the *undropped* cycle, not against the exact product.
    // The two are not the same number here -- the whole cycle renders 2.38
    // where the product is 2.26, the low-code brightening the case above
    // records -- and charging that pre-existing offset to the drop pattern
    // would overstate this effect by a third.
    //
    // Against the right baseline the subsets straddle it almost exactly:
    // 2.00 and 2.75 about 2.38, or -0.38 and +0.37. That symmetry is the
    // signature of a phase-correlated drop rather than of lost light.
    FL_CHECK_LT(even_mean, whole_cycle - 0.2);
    FL_CHECK_GT(odd_mean, whole_cycle + 0.2);
    FL_CHECK_LT(fl::fabs((whole_cycle - even_mean) - (odd_mean - whole_cycle)),
                0.1);

    // And the swing is bounded by the dither amplitude, which is what #4347
    // claims without measuring: one output LSB per channel. 0.75 codes
    // separation, so each subset sits under half an LSB off -- real, and
    // inside the stated bound.
    const double separation = odd_mean - even_mean;
    FL_CHECK_GT(separation, 0.5);
    FL_CHECK_LE(separation, 1.0);
}

FL_TEST_CASE("[#4347] uncorrelated drops keep the mean; correlation is the fault") {
    // The other half of #4347's claim, and the reason the issue is about
    // *correlation* rather than about dropping frames: "Uncorrelated drops
    // average out and cost nothing but noise."
    //
    // Worth stating what does not work. A contiguous half-window -- present
    // the first four phases of each cycle, drop the rest -- is still a
    // correlated subset, and still biased: it renders 2.25 against the
    // undropped 2.38. Bit-reversal maps phases 0-3 to offsets 16/144/80/208,
    // whose mean is 112 rather than 128, so "contiguous" is not "unbiased".
    // Only a drop pattern independent of the phase is.
    const fl::u8 kValue = 9;
    const fl::u8 kPremixed = 64;
    const double whole_cycle =
        static_cast<double>(cycleSum(kValue, kPremixed)) / kDitherCycle;

    // A plain LCG, fixed seed: deterministic, and independent of the phase.
    fl::u32 rng = 0x13579bdfu;
    int sum = 0;
    int presented = 0;
    const int kFrames = kDitherCycle * 512;
    for (int frame = 0; frame < kFrames; ++frame) {
        fl::detail::advanceDitherFrame();
        rng = rng * 1664525u + 1013904223u;
        if (((rng >> 16) & 0x01) == 0) {
            continue;  // dropped, phase still consumed
        }
        CRGB pixel(kValue, kValue, kValue);
        ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
        adjustment.premixed = CRGB(kPremixed, kPremixed, kPremixed);
        PixelController<RGB> pixels(&pixel, 1, adjustment, BINARY_DITHER);
        sum += pixels.loadAndScale0();
        ++presented;
    }

    // Pinned exactly, not as a lower bound. The LCG is fixed-seed u32
    // arithmetic, so this is deterministic -- and a lower bound alone would
    // be satisfied by a drop rule that stopped dropping, which would then
    // trivially render the undropped mean this case compares against. That
    // is the one way this test could go vacuous.
    //
    // 2058 of 4096 is about half, and enough that sampling noise sits well
    // under the effect being ruled out: the per-frame spread is about +/-0.4
    // codes, so the standard error here is near 0.01 against a 0.05 bound.
    FL_CHECK_EQ(presented, 2058);
    FL_CHECK_EQ(kFrames, 4096);
    const double mean = static_cast<double>(sum) / presented;

    // Lands on the undropped value, where the parity split missed it by 0.38.
    FL_CHECK_LT(fl::fabs(mean - whole_cycle), 0.05);
}

// ---------------------------------------------------------------------------
// The other clause of the same contract sentence: "with explicit tests for
// dropped submissions and irregular dwell." The drop half is above. This is
// dwell.
//
// Dither correctness is a *time-weighted* mean over the cycle, so a phase held
// twice as long counts twice. Dropping a frame removes its phase from the
// average; holding one longer overweights it. The two are the same defect on
// different axes, and neither is visible to a measurement that assumes every
// frame is displayed for the same interval.
// ---------------------------------------------------------------------------

namespace {

/// Time-weighted mean emitted code over one cycle.
///
/// The weight is chosen by the phase's own parity, not by position in the
/// loop. Indexing by position is the mistake this helper exists to avoid: the
/// cycle starts wherever the global counter happens to be, so a fixed array
/// lands on whichever parity it lands on and the measurement silently
/// reverses sign.
///
/// Equal weights reduce to `cycleSum / kDitherCycle`, which is what every
/// measurement above assumes without saying so.
double dwellWeightedMean(fl::u8 value, fl::u8 premixed, int hold_odd,
                         int hold_even) {
    double weighted = 0.0;
    double total = 0.0;
    for (int frame = 0; frame < kDitherCycle; ++frame) {
        fl::detail::advanceDitherFrame();
        const bool odd = (fl::detail::ditherFrame() & 0x01) != 0;
        CRGB pixel(value, value, value);
        ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
        adjustment.premixed = CRGB(premixed, premixed, premixed);
        PixelController<RGB> pixels(&pixel, 1, adjustment, BINARY_DITHER);
        const double emitted = static_cast<double>(pixels.loadAndScale0());
        const double hold = static_cast<double>(odd ? hold_odd : hold_even);
        weighted += emitted * hold;
        total += hold;
    }
    return weighted / total;
}

} // namespace

FL_TEST_CASE("[#4347] irregular dwell moves the mean the drop test measures") {
    const fl::u8 kValue = 9;
    const fl::u8 kPremixed = 64;

    // The reference: every phase held equally, which is the undropped,
    // even-dwell case the rest of this file measures.
    const double even = dwellWeightedMean(kValue, kPremixed, 1, 1);
    const double whole_cycle =
        static_cast<double>(cycleSum(kValue, kPremixed)) / kDitherCycle;
    FL_CHECK_LT(fl::fabs(even - whole_cycle), 0.05);

    // Now hold the phases that carry the low half of the bit-reversed offsets
    // three times as long. Nothing is dropped -- every phase still reaches the
    // wire -- and the mean still moves, because the average is over time and
    // not over frames.
    const double heavy_low = dwellWeightedMean(kValue, kPremixed, 1, 3);
    const double heavy_high = dwellWeightedMean(kValue, kPremixed, 3, 1);

    // Straddles the even-dwell value, the same signature the drop test finds:
    // a phase-correlated weighting displaces the mean rather than adding
    // noise to it.
    FL_CHECK_LT(heavy_low, even - 0.1);
    FL_CHECK_GT(heavy_high, even + 0.1);

    // 0.38 codes of swing, against the drop case's 0.75. That ratio is not a
    // coincidence and is worth recording: dropping a parity gives the
    // surviving phases 100% of the weight, while holding 3:1 gives them 75%,
    // so the displacement from the even-dwell mean is half as far. Dwell is a
    // weaker lever than dropping, reaching the same place by degrees.
    //
    // Which is the point of testing it separately. An implementation that
    // advanced the phase on presentation would close the drop half of this
    // contract sentence and leave this half exactly as it is, because dwell
    // is about how long a presented frame stays up, not whether it was
    // presented at all.
    const double separation = heavy_high - heavy_low;
    FL_CHECK_GT(separation, 0.3);
    FL_CHECK_LT(separation, 0.5);
}

FL_TEST_CASE("[#4347] dwell that does not correlate with phase costs nothing") {
    // The control, matching the drop test's: irregularity alone is harmless.
    // A dwell pattern independent of the phase leaves the time-weighted mean
    // where uniform dwell puts it, so what matters is correlation, not
    // jitter.
    const fl::u8 kValue = 9;
    const fl::u8 kPremixed = 64;

    const double even = dwellWeightedMean(kValue, kPremixed, 1, 1);

    // Hold every phase for two units instead of one. The dwell is irregular
    // against the frame clock -- each frame is twice as long as before -- and
    // uniform against the phase, which is the distinction that matters.
    const double irregular = dwellWeightedMean(kValue, kPremixed, 2, 2);

    FL_CHECK_LT(fl::fabs(irregular - even), 0.35);
}

} // FL_TEST_FILE
