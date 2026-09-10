// ok cpp include
// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/gfx/hsv16.h"
#include "fl/math/intmap.h"
#include "fl/stl/stdint.h"
#include "test.h"
#include "hsv2rgb.h"
#include "fl/gfx/colorimetric_response.h"
#include "fl/gfx/crgb.h"
#include "fl/gfx/oklab_q16.h"
#include "fl/math/ease.h"
#include "fl/math/math.h"
#include "fl/stl/int.h"


FL_TEST_CASE("RGB to HSV16 to RGB") {

    FL_SUBCASE("Primary Colors - Good Conversion") {
        // Test colors that convert well with HSV16

        // Pure red - perfect conversion - expect exact match
        const int red_tolerance = 0; // Reduced from 1 to 0 - try for perfect
        CRGB red(255, 0, 0);
        fl::HSV16 hsv_red(red);
        CRGB red_result = hsv_red.ToRGB();
        FL_CHECK_CLOSE(red_result.r, red.r, red_tolerance);
        FL_CHECK_CLOSE(red_result.g, red.g, red_tolerance);
        FL_CHECK_CLOSE(red_result.b, red.b, red_tolerance);

        // Pure green - try for even better accuracy
        const int green_tolerance = 0; // Reduced from 2 to 0 - try for perfect!
        CRGB green(0, 255, 0);
        fl::HSV16 hsv_green(green);
        CRGB green_result = hsv_green.ToRGB();
        FL_CHECK_CLOSE(green_result.r, green.r, green_tolerance);
        FL_CHECK_CLOSE(green_result.g, green.g, green_tolerance);
        FL_CHECK_CLOSE(green_result.b, green.b, green_tolerance);

        // Pure blue - try for even better accuracy
        const int blue_tolerance = 0; // Reduced from 2 to 0 - try for perfect!
        CRGB blue(0, 0, 255);
        fl::HSV16 hsv_blue(blue);
        CRGB blue_result = hsv_blue.ToRGB();
        FL_CHECK_CLOSE(blue_result.r, blue.r, blue_tolerance);
        FL_CHECK_CLOSE(blue_result.g, blue.g, blue_tolerance);
        FL_CHECK_CLOSE(blue_result.b, blue.b, blue_tolerance);

        // Test black - perfect conversion expected
        CRGB black(0, 0, 0);
        fl::HSV16 hsv_black(black);
        CRGB black_result = hsv_black.ToRGB();
        FL_CHECK(black_result.r == black.r);
        FL_CHECK(black_result.g == black.g);
        FL_CHECK(black_result.b == black.b);
    }

    FL_SUBCASE("White and Grayscale - Good Conversion") {
        // Test white - try for perfect accuracy
        const int white_tolerance = 0; // Reduced from 1 to 0 - try for perfect!
        CRGB white(255, 255, 255);
        fl::HSV16 hsv_white(white);
        CRGB white_result = hsv_white.ToRGB();
        FL_CHECK_CLOSE(white_result.r, white.r, white_tolerance);
        FL_CHECK_CLOSE(white_result.g, white.g, white_tolerance);
        FL_CHECK_CLOSE(white_result.b, white.b, white_tolerance);

        // Test various shades of gray - some require tolerance 1
        const int gray_tolerance =
            1; // Gray128 and Gray200 are off by exactly 1

        CRGB gray50(50, 50, 50);
        fl::HSV16 hsv_gray50(gray50);
        CRGB gray50_result = hsv_gray50.ToRGB();
        FL_CHECK_CLOSE(gray50_result.r, gray50.r,
                    0); // Gray50 is perfect - use tolerance 0
        FL_CHECK_CLOSE(gray50_result.g, gray50.g, 0);
        FL_CHECK_CLOSE(gray50_result.b, gray50.b, 0);

        CRGB gray128(128, 128, 128);
        fl::HSV16 hsv_gray128(gray128);
        CRGB gray128_result = hsv_gray128.ToRGB();
        FL_CHECK_CLOSE(gray128_result.r, gray128.r,
                    gray_tolerance); // Gray128 needs tolerance 1
        FL_CHECK_CLOSE(gray128_result.g, gray128.g, gray_tolerance);
        FL_CHECK_CLOSE(gray128_result.b, gray128.b, gray_tolerance);

        CRGB gray200(200, 200, 200);
        fl::HSV16 hsv_gray200(gray200);
        CRGB gray200_result = hsv_gray200.ToRGB();
        FL_CHECK_CLOSE(gray200_result.r, gray200.r,
                    gray_tolerance); // Gray200 needs tolerance 1
        FL_CHECK_CLOSE(gray200_result.g, gray200.g, gray_tolerance);
        FL_CHECK_CLOSE(gray200_result.b, gray200.b, gray_tolerance);
    }

    FL_SUBCASE("HSV16 Constructor Values") {
        // Test direct HSV16 construction with known values
        const int direct_construction_tolerance =
            0; // Reduced from 2 to 0 - try for perfect!

        fl::HSV16 hsv_red_direct(0, 65535, 65535); // Red: H=0, S=max, V=max
        CRGB red_direct_result = hsv_red_direct.ToRGB();
        FL_CHECK(red_direct_result.r >=
              255 - direct_construction_tolerance); // Should be close to 255
        FL_CHECK(red_direct_result.g <=
              direct_construction_tolerance); // Should be close to 0
        FL_CHECK(red_direct_result.b <=
              direct_construction_tolerance); // Should be close to 0

        fl::HSV16 hsv_green_direct(21845, 65535,
                               65535); // Green: H=1/3*65535, S=max, V=max
        CRGB green_direct_result = hsv_green_direct.ToRGB();
        FL_CHECK(green_direct_result.r <=
              direct_construction_tolerance); // Should be close to 0
        FL_CHECK(green_direct_result.g >=
              255 - direct_construction_tolerance); // Should be close to 255
        FL_CHECK(green_direct_result.b <=
              direct_construction_tolerance); // Should be close to 0

        fl::HSV16 hsv_blue_direct(43690, 65535,
                              65535); // Blue: H=2/3*65535, S=max, V=max
        CRGB blue_direct_result = hsv_blue_direct.ToRGB();
        FL_CHECK(blue_direct_result.r <=
              direct_construction_tolerance); // Should be close to 0
        FL_CHECK(blue_direct_result.g <=
              direct_construction_tolerance); // Should be close to 0
        FL_CHECK(blue_direct_result.b >=
              255 - direct_construction_tolerance); // Should be close to 255

        // Test zero saturation (should produce grayscale)
        const int grayscale_direct_tolerance = 0; // Keep at 0 - already perfect
        fl::HSV16 hsv_gray_direct(32768, 0,
                              32768); // Any hue, no saturation, half value
        CRGB gray_direct_result = hsv_gray_direct.ToRGB();
        FL_CHECK_CLOSE(gray_direct_result.r, gray_direct_result.g,
                    grayscale_direct_tolerance);
        FL_CHECK_CLOSE(gray_direct_result.g, gray_direct_result.b,
                    grayscale_direct_tolerance);
        FL_CHECK(gray_direct_result.r >=
              128 - 1); // Reduced from 2 to 1 - even tighter
        FL_CHECK(gray_direct_result.r <= 128 + 1);
    }

    FL_SUBCASE("Secondary Colors - Good Conversion") {
        // These secondary colors should now convert accurately with the fixed
        // HSV16 implementation

        // Yellow should preserve both red and green components
        const int yellow_tolerance =
            0; // Reduced from 1 to 0 - perfect conversion!
        CRGB yellow(255, 255, 0);
        fl::HSV16 hsv_yellow(yellow);
        CRGB yellow_result = hsv_yellow.ToRGB();
        FL_CHECK_CLOSE(yellow_result.r, yellow.r,
                    yellow_tolerance); // Red should be preserved
        FL_CHECK_CLOSE(yellow_result.g, yellow.g,
                    yellow_tolerance); // Green should be preserved
        FL_CHECK_CLOSE(yellow_result.b, yellow.b,
                    yellow_tolerance); // Blue should stay 0

        // Cyan should preserve both green and blue components
        const int cyan_tolerance =
            0; // Reduced from 1 to 0 - perfect conversion!
        CRGB cyan(0, 255, 255);
        fl::HSV16 hsv_cyan(cyan);
        CRGB cyan_result = hsv_cyan.ToRGB();
        FL_CHECK_CLOSE(cyan_result.r, cyan.r, cyan_tolerance); // Red should stay 0
        FL_CHECK_CLOSE(cyan_result.g, cyan.g,
                    cyan_tolerance); // Green should be preserved
        FL_CHECK_CLOSE(cyan_result.b, cyan.b,
                    cyan_tolerance); // Blue should be preserved

        // Magenta should preserve both red and blue components
        const int magenta_tolerance =
            0; // Reduced from 1 to 0 - perfect conversion!
        CRGB magenta(255, 0, 255);
        fl::HSV16 hsv_magenta(magenta);
        CRGB magenta_result = hsv_magenta.ToRGB();
        FL_CHECK_CLOSE(magenta_result.r, magenta.r,
                    magenta_tolerance); // Red should be preserved
        FL_CHECK_CLOSE(magenta_result.g, magenta.g,
                    magenta_tolerance); // Green should stay 0
        FL_CHECK_CLOSE(magenta_result.b, magenta.b,
                    magenta_tolerance); // Blue should be preserved
    }

    FL_SUBCASE("Low-Value Problematic Colors") {
        // Test very dark colors that are known to be problematic for HSV
        // conversion These colors often reveal quantization and rounding issues

        // Very dark red - near black but not black
        const int dark_primary_tolerance = 0; // Try for perfect conversion
        CRGB dark_red(10, 0, 0);
        fl::HSV16 hsv_dark_red(dark_red);
        CRGB dark_red_result = hsv_dark_red.ToRGB();
        FL_CHECK_CLOSE(dark_red_result.r, dark_red.r, dark_primary_tolerance);
        FL_CHECK_CLOSE(dark_red_result.g, dark_red.g, dark_primary_tolerance);
        FL_CHECK_CLOSE(dark_red_result.b, dark_red.b, dark_primary_tolerance);

        // Very dark green
        CRGB dark_green(0, 10, 0);
        fl::HSV16 hsv_dark_green(dark_green);
        CRGB dark_green_result = hsv_dark_green.ToRGB();
        FL_CHECK_CLOSE(dark_green_result.r, dark_green.r, dark_primary_tolerance);
        FL_CHECK_CLOSE(dark_green_result.g, dark_green.g, dark_primary_tolerance);
        FL_CHECK_CLOSE(dark_green_result.b, dark_green.b, dark_primary_tolerance);

        // Very dark blue
        CRGB dark_blue(0, 0, 10);
        fl::HSV16 hsv_dark_blue(dark_blue);
        CRGB dark_blue_result = hsv_dark_blue.ToRGB();
        FL_CHECK_CLOSE(dark_blue_result.r, dark_blue.r, dark_primary_tolerance);
        FL_CHECK_CLOSE(dark_blue_result.g, dark_blue.g, dark_primary_tolerance);
        FL_CHECK_CLOSE(dark_blue_result.b, dark_blue.b, dark_primary_tolerance);

        // Barely visible gray - single digit values
        const int barely_visible_tolerance = 0; // Try for perfect conversion
        CRGB barely_gray1(1, 1, 1);
        fl::HSV16 hsv_barely_gray1(barely_gray1);
        CRGB barely_gray1_result = hsv_barely_gray1.ToRGB();
        FL_CHECK_CLOSE(barely_gray1_result.r, barely_gray1.r,
                    barely_visible_tolerance);
        FL_CHECK_CLOSE(barely_gray1_result.g, barely_gray1.g,
                    barely_visible_tolerance);
        FL_CHECK_CLOSE(barely_gray1_result.b, barely_gray1.b,
                    barely_visible_tolerance);

        CRGB barely_gray5(5, 5, 5);
        fl::HSV16 hsv_barely_gray5(barely_gray5);
        CRGB barely_gray5_result = hsv_barely_gray5.ToRGB();
        FL_CHECK_CLOSE(barely_gray5_result.r, barely_gray5.r,
                    barely_visible_tolerance);
        FL_CHECK_CLOSE(barely_gray5_result.g, barely_gray5.g,
                    barely_visible_tolerance);
        FL_CHECK_CLOSE(barely_gray5_result.b, barely_gray5.b,
                    barely_visible_tolerance);

        // Low saturation, low value - muddy browns/grays
        const int muddy_tolerance = 1; // These may need tolerance 1
        CRGB muddy_brown(15, 10, 8);
        fl::HSV16 hsv_muddy_brown(muddy_brown);
        CRGB muddy_brown_result = hsv_muddy_brown.ToRGB();
        FL_CHECK_CLOSE(muddy_brown_result.r, muddy_brown.r, muddy_tolerance);
        FL_CHECK_CLOSE(muddy_brown_result.g, muddy_brown.g, muddy_tolerance);
        FL_CHECK_CLOSE(muddy_brown_result.b, muddy_brown.b, muddy_tolerance);

        // Edge case: slightly unequal very dark values
        CRGB dark_unequal(3, 2, 1);
        fl::HSV16 hsv_dark_unequal(dark_unequal);
        CRGB dark_unequal_result = hsv_dark_unequal.ToRGB();
        FL_CHECK_CLOSE(dark_unequal_result.r, dark_unequal.r, muddy_tolerance);
        FL_CHECK_CLOSE(dark_unequal_result.g, dark_unequal.g, muddy_tolerance);
        FL_CHECK_CLOSE(dark_unequal_result.b, dark_unequal.b, muddy_tolerance);

        // Very dark but colorful - low value, high saturation
        CRGB dark_saturated_red(20, 1, 1);
        fl::HSV16 hsv_dark_saturated_red(dark_saturated_red);
        CRGB dark_saturated_red_result = hsv_dark_saturated_red.ToRGB();
        FL_CHECK_CLOSE(dark_saturated_red_result.r, dark_saturated_red.r,
                    dark_primary_tolerance);
        FL_CHECK_CLOSE(dark_saturated_red_result.g, dark_saturated_red.g,
                    dark_primary_tolerance);
        FL_CHECK_CLOSE(dark_saturated_red_result.b, dark_saturated_red.b,
                    dark_primary_tolerance);
    }
}

FL_TEST_CASE("Exhaustive round trip") {
    // Increased step from 4 to 8 for performance (64^3 -> 32^3 iterations = 262K -> 33K = 87% reduction)
    // Still provides excellent coverage: 32^3 = 32,768 test cases across full RGB color space
    const int step = 8;
    for (int r = 0; r < 256; r+=step) {
        for (int g = 0; g < 256; g+=step) {
            for (int b = 0; b < 256; b+=step) {
                CRGB rgb(r, g, b);
                fl::HSV16 hsv(rgb);
                CRGB rgb_result = hsv.ToRGB();
                FL_REQUIRE_CLOSE(rgb_result.r, rgb.r, 1);
                FL_REQUIRE_CLOSE(rgb_result.g, rgb.g, 1);
                FL_REQUIRE_CLOSE(rgb_result.b, rgb.b, 1);
            }
        }
    }
}


#define TEST_VIDEO_RGB_HUE_PRESERVATION(color, hue_tolerance) \
    do { \
        fl::HSV16 hsv_original(color); \
        uint16_t original_hue = hsv_original.h; \
        \
        CRGB video_result = hsv_original.colorBoost(); \
        fl::HSV16 hsv_video_result(video_result); \
        uint16_t result_hue = hsv_video_result.h; \
        /* Special handling for hue around 0 (red) - check for wraparound */ \
        uint16_t hue_diff = (original_hue > result_hue) \
                                ? (original_hue - result_hue) \
                                : (result_hue - original_hue); \
        /* Also check wraparound case (difference near 65535) */ \
        uint16_t hue_diff_wraparound = 65535 - hue_diff; \
        uint16_t min_hue_diff = \
            (hue_diff < hue_diff_wraparound) ? hue_diff : hue_diff_wraparound; \
        \
        uint8_t hue_diff_8bit = fl::map16_to_8(min_hue_diff); \
        \
        FL_CHECK_LE(hue_diff_8bit, hue_tolerance); \
    } while(0)

FL_TEST_CASE("colorBoost() preserves hue - easy cases") {

    // Helper function to test colorBoost() hue preservation

    // Test that colorBoost() preserves the hue while applying gamma
    // correction to saturation. Each color uses a fine-grained tolerance based
    // on empirically observed maximum hue differences.

    FL_SUBCASE("Orange - Low hue error") {
        // Test with a vibrant orange color - wraparound helped reduce tolerance
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 128, 0), 0);
    }

    FL_SUBCASE("Blue-Green - Moderate hue error") {
        // Test with a blue-green color - exactly 14 units max error observed
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(0, 200, 150), 0);
    }

    FL_SUBCASE("Purple - Very low hue error") {
        // Test with a purple color - exactly 4 units max error observed
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(180, 50, 200), 0);
    }

    FL_SUBCASE("Warm Yellow - Highest hue error case") {
        // Test with a warm yellow color - this is the worst case with exactly
        // 47 units max error (empirically determined as the absolute worst case
        // across all test colors)
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 220, 80), 0);
    }

    FL_SUBCASE("Bright Red - Wraparound case") {
        // Test edge case: Very saturated red (hue around 0) - handle wraparound
        // Special case due to hue wraparound at 0/65535 boundary
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 30, 30), 0);
    }
}

FL_TEST_CASE("colorBoost() preserves hue - hard cases") {

    FL_SUBCASE("Low Saturation Colors - Hue Instability") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(130, 128, 125), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(100, 98, 102), 3);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(85, 87, 83), 0);
    }

    FL_SUBCASE("Very Dark Colors - Low Value Instability") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(15, 10, 8), 1);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(12, 8, 20), 1);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(8, 15, 12), 1);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(20, 12, 8), 1);
    }

    FL_SUBCASE("Hue Boundary Colors - Transition Regions") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 64, 0), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(192, 255, 0), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(0, 255, 128), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(0, 128, 255), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(128, 0, 255), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 0, 128), 0);
    }

    FL_SUBCASE("Medium Saturation, Medium Value - Gamma Sensitive") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(180, 120, 60), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(120, 180, 90), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(90, 120, 180), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(180, 90, 150), 0);
    }

    FL_SUBCASE("Single Component Dominant - Extreme Ratios") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(250, 10, 5), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(8, 240, 12), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(15, 8, 245), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(200, 200, 8), 0);
    }

    FL_SUBCASE("Pastel Colors - High Value, Low Saturation") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 200, 200), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(200, 255, 200), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(200, 200, 255), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 255, 200), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(255, 200, 255), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(200, 255, 255), 0);
    }

    FL_SUBCASE("Problematic RGB Combinations - Known Difficult Cases") {
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(77, 150, 200), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(200, 150, 77), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(150, 77, 200), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(33, 66, 99), 0);
        TEST_VIDEO_RGB_HUE_PRESERVATION(CRGB(99, 33, 66), 0);
    }
}

// Grouped tests
#include "tests/fl/gfx/eorder.hpp"
FL_TEST_FILE(FL_FILEPATH) {

} // FL_TEST_FILE

// Intrinsic hue and neutral distortion of FastLED's 8-bit shaping paths,
// for the colour pipeline's P8 section-5 study (#4042).
//
// Section 5 asks which strategy best spends a WS2812's 8 bits. Before that
// can be answered, each candidate path has to be characterised on its own:
// a path that moves hue or tints neutrals by itself cannot be part of the
// answer, whatever it does for smoothness.
//
// The paths run here are the shipping functions, not models of them --
// re-implementing `colorBoost()` or the HSV16 round trip in a harness would
// score a paraphrase. Scoring likewise uses the shipping colorimetry
// (`rgb_source_to_XYZ` + `xyzToOklabQ16`) rather than a second copy.
//
// Measured numbers and what they rule in or out:
// docs/color-ws2812-shaping-paths.md
//
// Grouped into `tests/fl/gfx/hsv16.cpp` rather than standing alone: the
// paths under test are `HSV16::ToRGB` and `CRGB::colorBoost`, both of which
// live in `fl/gfx/hsv16.h`, and each extra test file costs compile time.


using namespace fl;
using namespace fl::colorimetric_response;

namespace ws2812_shaping {
namespace {

constexpr float kQ16 = 65536.0f;
constexpr float kPi = 3.14159265358979323846f;

struct Oklab {
    float lightness;
    float a;
    float b;
};

/// Built by value on each call rather than cached in a function-local
/// static. A static inside a header gets one copy per module that links it,
/// which is a real portability trap (see the BusTraits singleton note in
/// tests/fl/channels/channel.cpp) and buys nothing here: this is a 3x3
/// inverse.
RgbColorimetricCache ws2812Cache() {
    RgbColorimetricCache cache;
    const bool built = build_rgb_colorimetric_cache(
        fl::colorimetric_response::profiles::WS2812B, &cache);
    FL_ASSERT(built, "WS2812B primaries must not be singular");
    return cache;
}

/// The light an 8-bit code triple actually produces, in OKLab.
///
/// A WS2812 drives its diodes with linear PWM, so the code *is* the drive.
/// Nothing here re-applies a transfer function: that is the point of the
/// measurement, since every path under test is claiming to shape the codes.
Oklab lightOf(const CRGB& code) {
    float xyz[3];
    const RgbColorimetricCache cache = ws2812Cache();
    rgb_source_to_XYZ(cache, static_cast<float>(code.r) / 255.0f,
                      static_cast<float>(code.g) / 255.0f,
                      static_cast<float>(code.b) / 255.0f, xyz);
    const i32 q16[3] = {
        static_cast<i32>(xyz[0] * kQ16 + 0.5f),
        static_cast<i32>(xyz[1] * kQ16 + 0.5f),
        static_cast<i32>(xyz[2] * kQ16 + 0.5f),
    };
    i32 lab[3];
    xyzToOklabQ16(q16, lab);
    return Oklab{static_cast<float>(lab[0]) / kQ16,
                 static_cast<float>(lab[1]) / kQ16,
                 static_cast<float>(lab[2]) / kQ16};
}

float chromaOf(const Oklab& lab) {
    return fl::sqrtf(lab.a * lab.a + lab.b * lab.b);
}

/// Absolute hue difference in degrees, wrapped into [0, 180].
///
/// Returns 180 -- the worst possible answer -- when either colour has
/// collapsed to the neutral axis while the other has not. Hue is undefined
/// there, and returning 0 would let a path that destroys all chroma score as
/// perfectly hue-preserving, which is the failure this metric exists to
/// catch.
float hueDriftDegrees(const Oklab& before, const Oklab& after) {
    constexpr float kChromaFloor = 1e-6f;
    const float chroma_before = chromaOf(before);
    const float chroma_after = chromaOf(after);
    const bool before_neutral = chroma_before < kChromaFloor;
    const bool after_neutral = chroma_after < kChromaFloor;
    if (before_neutral && after_neutral) {
        return 0.0f;
    }
    if (before_neutral != after_neutral) {
        return 180.0f;
    }
    const float first = fl::atan2f(before.b, before.a);
    const float second = fl::atan2f(after.b, after.a);
    float difference = (second - first) * 180.0f / kPi;
    while (difference > 180.0f) {
        difference -= 360.0f;
    }
    while (difference < -180.0f) {
        difference += 360.0f;
    }
    return fl::fabsf(difference);
}

CRGB pathIdentity(const CRGB& in) { return in; }

CRGB pathHsv16RoundTrip(const CRGB& in) { return HSV16(in).ToRGB(); }

CRGB pathColorBoostSaturation(const CRGB& in) {
    return in.colorBoost(EaseType::EASE_IN_QUAD, EaseType::EASE_NONE);
}

CRGB pathColorBoostLuminance(const CRGB& in) {
    return in.colorBoost(EaseType::EASE_NONE, EaseType::EASE_IN_QUAD);
}

/// Saturated inputs across the hue circle, at the drive levels that matter.
///
/// The interesting range is the bottom few percent: linear quantisation is
/// already inside a quarter of a dE2000 above ten percent, so a path only
/// has to earn its keep down low.
struct Sample {
    CRGB code;
    int level;
    int saturation;
};

fl::vector<Sample> darkHueSweep() {
    fl::vector<Sample> samples;
    const int levels[] = {1, 2, 3, 5, 8, 13, 26, 64, 128, 255};
    // Saturation has to vary. A fully saturated sweep makes
    // `colorBoost(EASE_IN_QUAD, ...)` a no-op -- there is no headroom left to
    // boost -- and it would score as perfectly colour-preserving for the one
    // reason that says nothing about it.
    const int saturations[] = {96, 176, 255};
    for (int level : levels) {
        for (int saturation : saturations) {
            for (int step = 0; step < 12; ++step) {
                const CHSV hsv(static_cast<u8>(step * 21),
                               static_cast<u8>(saturation),
                               static_cast<u8>(level));
                CRGB rgb;
                hsv2rgb_rainbow(hsv, rgb);
                samples.push_back(Sample{rgb, level, saturation});
            }
        }
    }
    return samples;
}

/// True when a path has driven a coloured input all the way to black.
///
/// Tracked separately from hue drift: folding it in reports 180 degrees,
/// which is arithmetically right and analytically useless -- the interesting
/// fact is that the colour is gone, not that its hue moved.
bool collapsedToBlack(const CRGB& before, const CRGB& after) {
    const bool had_light = before.r != 0 || before.g != 0 || before.b != 0;
    const bool has_light = after.r != 0 || after.g != 0 || after.b != 0;
    return had_light && !has_light;
}

/// Codes that render a D65 neutral at the given luminance on this profile.
///
/// Equal codes are NOT a neutral here: the profile normalises each emitter to
/// unit luminance, so an equal-drive triple is neither D65 nor unit
/// luminance. Measuring "does grey stay grey" on equal drives scores the
/// device normalisation rather than the path -- the mistake this helper
/// exists to prevent.
CRGB neutralCodes(float luminance) {
    float target[3];
    xyY_to_XYZ(0.3127f, 0.3290f, luminance, target);
    float drives[3];
    const RgbColorimetricCache cache = ws2812Cache();
    matvec3(cache.P_RGB_inv, target, drives);
    CRGB out;
    out.r = quantize_u8(drives[0]);
    out.g = quantize_u8(drives[1]);
    out.b = quantize_u8(drives[2]);
    return out;
}

}  // namespace

FL_TEST_CASE("The sweep reaches the dark, partly-saturated inputs the study is about") {
    // Every bound below is quantified over this sweep. An all-bright or
    // fully-saturated one would make them pass without measuring anything --
    // and a fully saturated sweep in particular makes `colorBoost`'s
    // saturation easing a no-op, since there is no headroom left to boost.
    const fl::vector<Sample> samples = darkHueSweep();
    FL_CHECK_EQ(samples.size(), fl::size(360));
    int dark = 0;
    int unsaturated = 0;
    for (const Sample& sample : samples) {
        if (sample.level <= 13) {
            ++dark;
        }
        if (sample.saturation < 255) {
            ++unsaturated;
        }
    }
    FL_CHECK_EQ(dark, 216);
    FL_CHECK_EQ(unsaturated, 240);
}

FL_TEST_CASE("Identity is hue-exact, so the metric is not reporting round-trip noise") {
    // A control. Without it every number below could be measuring the
    // colorimetry round trip rather than the path.
    float worst = 0.0f;
    for (const Sample& sample : darkHueSweep()) {
        worst = fl::max(worst, hueDriftDegrees(lightOf(sample.code),
                                               lightOf(pathIdentity(sample.code))));
    }
    FL_CHECK_EQ(worst, 0.0f);
}

FL_TEST_CASE("The HSV16 round trip is colour-preserving to within a code") {
    float worst_hue = 0.0f;
    float worst_gain = 0.0f;
    float worst_loss = 2.0f;
    int collapses = 0;
    for (const Sample& sample : darkHueSweep()) {
        const CRGB out = pathHsv16RoundTrip(sample.code);
        if (collapsedToBlack(sample.code, out)) {
            ++collapses;
            continue;
        }
        const Oklab before = lightOf(sample.code);
        const Oklab after = lightOf(out);
        if (chromaOf(before) > 0.01f && chromaOf(after) > 0.01f) {
            worst_hue = fl::max(worst_hue, hueDriftDegrees(before, after));
            const float ratio = chromaOf(after) / chromaOf(before);
            worst_gain = fl::max(worst_gain, ratio);
            worst_loss = fl::min(worst_loss, ratio);
        }
    }
    // Measured 0.649 deg, 0.973x .. 1.003x. Bracketed on both sides: a
    // one-sided bound would still pass if the metric collapsed to zero.
    FL_CHECK(worst_hue < 1.0f);
    FL_CHECK(worst_hue > 0.1f);
    FL_CHECK(worst_gain < 1.05f);
    FL_CHECK(worst_loss > 0.95f);
    FL_CHECK_EQ(collapses, 0);
}

FL_TEST_CASE("colorBoost zeroes minor channels at low codes, moving hue a long way") {
    // The finding that rules it out as a hue-preserving re-encoding. It is an
    // appearance control and changes the colour on purpose; section 5 needs
    // to know it cannot be used as a neutral 8-bit shaping stage.
    const CRGB dim(1, 3, 1);
    const CRGB boosted = pathColorBoostSaturation(dim);
    FL_CHECK_EQ(boosted.r, u8(0));
    FL_CHECK_EQ(boosted.g, u8(3));
    FL_CHECK_EQ(boosted.b, u8(0));

    float worst_hue = 0.0f;
    int scored = 0;
    for (const Sample& sample : darkHueSweep()) {
        const CRGB out = pathColorBoostSaturation(sample.code);
        if (collapsedToBlack(sample.code, out)) {
            continue;
        }
        const Oklab before = lightOf(sample.code);
        const Oklab after = lightOf(out);
        if (chromaOf(before) > 0.01f && chromaOf(after) > 0.01f) {
            worst_hue = fl::max(worst_hue, hueDriftDegrees(before, after));
            ++scored;
        }
    }
    // 142.969 deg measured, on a sample that stays visibly chromatic on both
    // sides -- so this is a real hue shift, not the ill-conditioning that
    // afflicts hue near the neutral axis.
    FL_CHECK(worst_hue > 100.0f);
    // Every sample, not most of them. The claim above is that the 143 deg is
    // not near-axis ill-conditioning, and that rests on the whole population
    // surviving the chroma screen -- a loose bound here would let a
    // desaturation regression shrink the population instead of failing.
    FL_CHECK_EQ(scored, static_cast<int>(darkHueSweep().size()));
}

FL_TEST_CASE("colorBoost luminance easing is a dimming curve, not an encoding") {
    int collapses = 0;
    const fl::vector<Sample> samples = darkHueSweep();
    for (const Sample& sample : samples) {
        if (collapsedToBlack(sample.code, pathColorBoostLuminance(sample.code))) {
            ++collapses;
        }
    }
    // 254 of 360 measured. Quadratic easing on an 8-bit value drives dark
    // content to zero, which is exactly the range section 5 is about.
    FL_CHECK(collapses > 200);
    FL_CHECK(collapses < static_cast<int>(samples.size()));
}

/// OKLab distance between two lights, as a plain Euclidean norm.
///
/// Not dE2000: the question below is which of several code triples lands
/// closest to a target, and OKLab was designed so that its own metric answers
/// that. Bringing in a second colour difference would add a second thing to
/// be wrong about without changing the ordering it produces here.
float oklabDistance(const Oklab& a, const Oklab& b) {
    const float dl = a.lightness - b.lightness;
    const float da = a.a - b.a;
    const float db = a.b - b.b;
    return fl::sqrtf(dl * dl + da * da + db * db);
}

/// The light a neutral target *should* produce, before quantization.
Oklab idealNeutralLight(float luminance) {
    const RgbColorimetricCache cache = ws2812Cache();
    float target[3];
    xyY_to_XYZ(0.3127f, 0.3290f, luminance, target);
    float drives[3];
    matvec3(cache.P_RGB_inv, target, drives);
    float xyz[3];
    rgb_source_to_XYZ(cache, drives[0], drives[1], drives[2], xyz);
    const i32 q16[3] = {
        static_cast<i32>(xyz[0] * kQ16 + 0.5f),
        static_cast<i32>(xyz[1] * kQ16 + 0.5f),
        static_cast<i32>(xyz[2] * kQ16 + 0.5f),
    };
    i32 lab[3];
    xyzToOklabQ16(q16, lab);
    return Oklab{static_cast<float>(lab[0]) / kQ16,
                 static_cast<float>(lab[1]) / kQ16,
                 static_cast<float>(lab[2]) / kQ16};
}

FL_TEST_CASE("Per-channel rounding is not the best code a single frame can pick") {
    // Section 5 asks which strategy best spends eight bits. This bounds what
    // any *static* strategy could achieve, which turns out not to be what the
    // pipeline currently gets.
    //
    // `quantize_u8` is round-to-nearest, and it rounds each drive
    // independently -- minimising error in *drive* space. The three drives
    // carry very different perceptual weight, so the nearest drive triple is
    // not the nearest colour. Searching a +/-2 code neighbourhood in OKLab
    // finds a better triple on most neutral targets.
    //
    // The measured answer, on 40 neutral luminances from 1% to 40%:
    // rounding is optimal on 13, and the worst shortfall is 0.0115 in OKLab
    // distance -- against the 0.0605 of neutral chroma the same sweep
    // reports, so roughly a fifth of that error is the rounding rule rather
    // than the lattice.
    //
    // What this does NOT say is that a 125-candidate search belongs on the
    // per-pixel path; it plainly does not. It says the ceiling for a static
    // strategy sits above what the pipeline reaches today, which is what
    // section 5 needed to know before comparing shaping functions that all
    // sit below it.
    int examined = 0;
    int rounding_was_optimal = 0;
    float worst_shortfall = 0.0f;
    for (int step = 1; step <= 40; ++step) {
        const float luminance = static_cast<float>(step) / 100.0f;
        const Oklab ideal = idealNeutralLight(luminance);
        const CRGB rounded = neutralCodes(luminance);
        const float rounded_distance = oklabDistance(lightOf(rounded), ideal);

        float best = rounded_distance;
        for (int dr = -2; dr <= 2; ++dr) {
            for (int dg = -2; dg <= 2; ++dg) {
                for (int db = -2; db <= 2; ++db) {
                    const int r = static_cast<int>(rounded.r) + dr;
                    const int g = static_cast<int>(rounded.g) + dg;
                    const int b = static_cast<int>(rounded.b) + db;
                    if (r < 0 || g < 0 || b < 0) continue;
                    if (r > 255 || g > 255 || b > 255) continue;
                    const CRGB candidate(static_cast<u8>(r), static_cast<u8>(g),
                                         static_cast<u8>(b));
                    best = fl::min(best, oklabDistance(lightOf(candidate), ideal));
                }
            }
        }
        ++examined;
        if (rounded_distance <= best + 1e-6f) {
            ++rounding_was_optimal;
        }
        worst_shortfall = fl::max(worst_shortfall, rounded_distance - best);
        // The search includes the rounded triple itself, so it can never
        // report worse. If it does, the search is broken rather than the
        // rounding being good.
        FL_CHECK(best <= rounded_distance + 1e-6f);
    }

    // Not vacuous: the sweep has to have looked at something.
    FL_CHECK_EQ(examined, 40);
    // The finding, bracketed on both sides. Rounding wins sometimes -- so
    // this is a rule that is wrong often, not one that is always wrong -- and
    // it loses often enough, and by enough, to matter.
    FL_CHECK(rounding_was_optimal > 0);
    FL_CHECK(rounding_was_optimal < examined);
    FL_CHECK(worst_shortfall > 0.005f);
}

FL_TEST_CASE("The search would find a better code if one existed") {
    // A positive control for the case above. Deliberately start from a code
    // that is *not* the rounded one and show the neighbourhood search beats
    // it -- otherwise "rounding was optimal" could mean the search never
    // looked anywhere.
    const float luminance = 0.20f;
    const Oklab ideal = idealNeutralLight(luminance);
    const CRGB rounded = neutralCodes(luminance);
    const CRGB nudged(static_cast<u8>(rounded.r + 2), rounded.g, rounded.b);

    const float nudged_distance = oklabDistance(lightOf(nudged), ideal);
    const float rounded_distance = oklabDistance(lightOf(rounded), ideal);
    FL_CHECK(rounded_distance < nudged_distance);
}

FL_TEST_CASE("The neutral axis leaves the axis from quantization alone") {
    // Evaluated on a real D65 neutral target, not on equal drives: this
    // profile normalises each emitter to unit luminance, so equal codes are
    // neither D65 nor unit luminance, and scoring them measures the device
    // normalisation instead of the path.
    float worst_identity = 0.0f;
    float worst_hsv16 = 0.0f;
    float worst_boost = 0.0f;
    for (int step = 1; step <= 100; ++step) {
        const CRGB grey = neutralCodes(static_cast<float>(step) / 100.0f);
        worst_identity = fl::max(worst_identity, chromaOf(lightOf(grey)));
        worst_hsv16 =
            fl::max(worst_hsv16, chromaOf(lightOf(pathHsv16RoundTrip(grey))));
        worst_boost =
            fl::max(worst_boost, chromaOf(lightOf(pathColorBoostLuminance(grey))));
    }
    // 0.0605 at 2% luminance, from rounding three drives to 8 bits. This is
    // the number any section-5 dithering strategy has to beat.
    FL_CHECK(worst_identity > 0.05f);
    FL_CHECK(worst_identity < 0.07f);
    // Neither existing path improves it, and the boost makes it worse.
    FL_CHECK_EQ(worst_hsv16, worst_identity);
    FL_CHECK(worst_boost > worst_identity);
}

}  // namespace ws2812_shaping
