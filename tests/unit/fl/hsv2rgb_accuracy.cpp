
#include "hsv2rgb.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/new.h"
#include "fl/stl/pair.h"
#include "fl/stl/utility.h"
#include "fl/stl/vector.h"
#include "fl/gfx/colorutils.h"
#include "chsv.h"
#include "crgb.h"
#include "test.h"
#include "fl/log/log.h"
#include "fl/math/math.h"
#include "fl/stl/strstream.h"

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;
struct AccuracyStats {
    float average;
    float median;
    float max;
    float min;
    vector<float> deviations;

    void calculate() {
        if (deviations.empty()) {
            average = median = max = min = 0.0f;
            return;
        }

        // Sort for median calculation
        sort(deviations.begin(), deviations.end());

        // Calculate average
        float sum = 0.0f;
        for (size_t i = 0; i < deviations.size(); ++i) {
            sum += deviations[i];
        }
        average = sum / deviations.size();

        // Calculate median
        size_t mid = deviations.size() / 2;
        if (deviations.size() % 2 == 0) {
            median = (deviations[mid - 1] + deviations[mid]) / 2.0f;
        } else {
            median = deviations[mid];
        }

        // Min and max
        min = deviations.front();
        max = deviations.back();
    }
    
    void print(const char* function_name) const {
        FL_WARN(function_name << " Accuracy Statistics:");
        FL_WARN("  Average: " << average);
        FL_WARN("  Median:  " << median);
        FL_WARN("  Min:     " << min);
        FL_WARN("  Max:     " << max);
        FL_WARN("  Samples: " << deviations.size());
        FL_WARN("");
    }
};

// Calculate euclidean distance between two RGB colors
static float calculateRGBDeviation(const CRGB& original, const CRGB& converted) {
    float dr = float(original.r) - float(converted.r);
    float dg = float(original.g) - float(converted.g);
    float db = float(original.b) - float(converted.b);
    return fl::sqrtf(dr*dr + dg*dg + db*db);
}

// Test a specific conversion function with RGB -> HSV -> RGB round trip
template<typename ConversionFunc>
static AccuracyStats testConversionFunction(ConversionFunc hsv2rgb_func, const char* func_name) {
    (void)func_name; // Suppress unused parameter warning
    AccuracyStats stats;
    
    // Test a comprehensive set of RGB colors
    // We'll test every 16th value to get good coverage without taking too long
    // Increased step from 8 to 16 for performance (32^3 -> 16^3 iterations = 32K -> 4K = 87.5% reduction)
    // Still provides excellent coverage: 16^3 = 4,096 test cases per conversion function
    const int step = 16;
    
    for (int r = 0; r < 256; r += step) {
        for (int g = 0; g < 256; g += step) {
            for (int b = 0; b < 256; b += step) {
                // Original RGB color
                CRGB original_rgb(r, g, b);
                
                // Convert RGB -> HSV
                CHSV hsv = rgb2hsv_approximate(original_rgb);
                
                // Convert HSV -> RGB using the test function
                CRGB converted_rgb;
                hsv2rgb_func(hsv, converted_rgb);
                
                // Calculate deviation
                float deviation = calculateRGBDeviation(original_rgb, converted_rgb);
                stats.deviations.push_back(deviation);
            }
        }
    }
    
    stats.calculate();
    return stats;
}

FL_TEST_CASE("HSV to RGB Conversion Accuracy Comparison") {
    FL_WARN("=== HSV to RGB Conversion Accuracy Test ===");
    FL_WARN("Testing RGB -> HSV -> RGB round-trip accuracy");
    FL_WARN("Sampling every 8th RGB value for comprehensive coverage");
    FL_WARN("");
    
    // Test all three conversion functions
    AccuracyStats rainbow_stats = testConversionFunction(
        [](const CHSV& hsv, CRGB& rgb) { hsv2rgb_rainbow(hsv, rgb); },
        "hsv2rgb_rainbow"
    );

    AccuracyStats spectrum_stats = testConversionFunction(
        [](const CHSV& hsv, CRGB& rgb) { hsv2rgb_spectrum(hsv, rgb); },
        "hsv2rgb_spectrum"
    );

    AccuracyStats fullspectrum_stats = testConversionFunction(
        [](const CHSV& hsv, CRGB& rgb) { hsv2rgb_fullspectrum(hsv, rgb); },
        "hsv2rgb_fullspectrum"
    );
    
    // Print results
    rainbow_stats.print("hsv2rgb_rainbow");
    spectrum_stats.print("hsv2rgb_spectrum");
    fullspectrum_stats.print("hsv2rgb_fullspectrum");
    
    // Print comparison
    FL_WARN("=== Accuracy Comparison ===");
    FL_WARN("Function            Average    Median     Min        Max");
    FL_WARN("hsv2rgb_rainbow     " << rainbow_stats.average << "   " << rainbow_stats.median << "   " << rainbow_stats.min << "   " << rainbow_stats.max);
    FL_WARN("hsv2rgb_spectrum    " << spectrum_stats.average << "   " << spectrum_stats.median << "   " << spectrum_stats.min << "   " << spectrum_stats.max);
    FL_WARN("hsv2rgb_fullspectrum" << fullspectrum_stats.average << "   " << fullspectrum_stats.median << "   " << fullspectrum_stats.min << "   " << fullspectrum_stats.max);
    FL_WARN("");
    
    // Find the best performing function for each metric
    vector<pair<float, const char*>> avg_results = {
        {rainbow_stats.average, "rainbow"},
        {spectrum_stats.average, "spectrum"},
        {fullspectrum_stats.average, "fullspectrum"}
    };
    sort(avg_results.begin(), avg_results.end());

    vector<pair<float, const char*>> median_results = {
        {rainbow_stats.median, "rainbow"},
        {spectrum_stats.median, "spectrum"},
        {fullspectrum_stats.median, "fullspectrum"}
    };
    sort(median_results.begin(), median_results.end());

    vector<pair<float, const char*>> max_results = {
        {rainbow_stats.max, "rainbow"},
        {spectrum_stats.max, "spectrum"},
        {fullspectrum_stats.max, "fullspectrum"}
    };
    sort(max_results.begin(), max_results.end());
    
    FL_WARN("=== Best Performance Rankings ===");
    FL_WARN("Lowest Average Deviation: " << avg_results[0].second << " (" << avg_results[0].first << ")");
    FL_WARN("Lowest Median Deviation:  " << median_results[0].second << " (" << median_results[0].first << ")");
    FL_WARN("Lowest Max Deviation:     " << max_results[0].second << " (" << max_results[0].first << ")");
    FL_WARN("");
    
    // Basic sanity checks - deviations should be reasonable for RGB->HSV->RGB round-trip
    // Note: RGB->HSV->RGB conversion is inherently lossy due to the approximation function
    FL_CHECK_LT(rainbow_stats.average, 150.0f);     // Average deviation should be reasonable
    FL_CHECK_LT(spectrum_stats.average, 150.0f);
    FL_CHECK_LT(fullspectrum_stats.average, 150.0f);

    // Max deviation can exceed single RGB channel distance due to euclidean distance calculation
    FL_CHECK_LT(rainbow_stats.max, 500.0f);         // Max deviation should be reasonable
    FL_CHECK_LT(spectrum_stats.max, 500.0f);
    FL_CHECK_LT(fullspectrum_stats.max, 500.0f);

    FL_CHECK_GE(rainbow_stats.min, 0.0f);           // Min deviation should be non-negative
    FL_CHECK_GE(spectrum_stats.min, 0.0f);
    FL_CHECK_GE(fullspectrum_stats.min, 0.0f);

    // Verify rainbow has the best (lowest) average deviation
    FL_CHECK_LT(rainbow_stats.average, spectrum_stats.average);
    FL_CHECK_LT(rainbow_stats.average, fullspectrum_stats.average);
}

FL_TEST_CASE("rgb2hsv_approximate - orange is not reported as green (issue #436)") {
    // The Orange->Yellow branch computed (g - 85) + (171 - r), which goes
    // negative for every r > 171 and then wrapped when truncated to u8 inside
    // qsub8(). Orange came back green: rgb(255,153,0) reported hue 122.
    struct Case {
        u8 r, g, b;
        u8 expected_hue;
        const char *source;
    };
    const Case cases[] = {
        {255, 153, 0, 26, "issue body"},
        {255, 128, 0, 21, "cad435 #ff8000"},
        {255, 166, 0, 24, "DrJaymz"},
        {255, 143, 0, 25, "5chmidti precise impl"},
    };

    for (const auto &c : cases) {
        CRGB original(c.r, c.g, c.b);
        CHSV hsv = rgb2hsv_approximate(original);
        CRGB roundtrip;
        hsv2rgb_rainbow(hsv, roundtrip);
        FL_WARN("rgb(" << (int)c.r << "," << (int)c.g << "," << (int)c.b
                       << ") -> hue " << (int)hsv.hue << " -> rgb("
                       << (int)roundtrip.r << "," << (int)roundtrip.g << ","
                       << (int)roundtrip.b << ")   [" << c.source
                       << ", standard-HSV hue would be " << (int)c.expected_hue
                       << "]");
        // Orange must land in the red..yellow arc, never in the greens. This
        // is the actual regression: these all returned 110-130 before.
        FL_CHECK_LE((int)hsv.hue, (int)HUE_YELLOW);
        // rgb2hsv_approximate inverts hsv2rgb_rainbow, not a textbook HSV
        // hexagon, so correctness is round-trip fidelity rather than agreement
        // with a standard-HSV hue. Green must stay the middle channel and blue
        // must stay dark -- i.e. it still looks orange.
        FL_CHECK_GT((int)roundtrip.r, (int)roundtrip.g);
        FL_CHECK_GT((int)roundtrip.g, (int)roundtrip.b);
    }
}

FL_TEST_CASE("rgb2hsv_approximate - scaling does not wrap channels (issue #1074)") {
    const CRGB original(218, 137, 48);
    const CHSV hsv = rgb2hsv_approximate(original);
    const CRGB roundtrip = hsv2rgb_rainbow(hsv);

    // Scaling the desaturated channels produces an intermediate red value of
    // 299. It must be normalized, not truncated to 43, which incorrectly
    // makes green the dominant channel and moves this orange into green hues.
    FL_CHECK_LE((int)hsv.hue, (int)HUE_YELLOW);
    FL_CHECK_EQ((int)hsv.value, 255);
    FL_CHECK_GT((int)roundtrip.r, (int)roundtrip.g);
    FL_CHECK_GT((int)roundtrip.g, (int)roundtrip.b);
}

FL_TEST_CASE("rgb2hsv_approximate - hue stays in the red..yellow arc red -> yellow") {
    // Sweeping green up at full red walks hue from red toward yellow. The
    // underflow made this jump out into the greens partway through (46 -> 115
    // -> 121 -> 126 -> 35), which is what made the bug so visible in fades.
    //
    // KNOWN LIMITATION: there is still a small backwards step of about 5 at
    // g=128, where the Red-Orange and Orange-Yellow branches meet. The branch
    // boundary sits at g = r/2, which does not correspond to HUE_ORANGE, so
    // the two branches are calibrated against different endpoints. Closing
    // that gap means recalibrating both branches together rather than fixing
    // one expression, so it is left for follow-up -- a 5-unit seam is not
    // visible where a 70-unit jump into green very much was.
    const int kKnownSeam = 8;
    int previous = -1;
    for (int g = 0; g <= 255; g += 15) {
        CHSV hsv = rgb2hsv_approximate(CRGB(255, g, 0));
        FL_WARN("rgb(255," << g << ",0) -> hue " << (int)hsv.hue);
        // The real regression check: never leave the red..yellow arc.
        FL_CHECK_LE((int)hsv.hue, (int)HUE_YELLOW + 4);
        FL_CHECK_GE((int)hsv.hue, previous - kKnownSeam);
        previous = static_cast<int>(hsv.hue);
    }
}

FL_TEST_CASE("rgb2hsv_approximate - greys report zero saturation") {
    for (int level = 0; level <= 255; level += 51) {
        CHSV hsv = rgb2hsv_approximate(CRGB(level, level, level));
        FL_WARN("grey " << level << " -> sat " << (int)hsv.sat);
        FL_CHECK_EQ((int)hsv.sat, 0);
    }
}

FL_TEST_CASE("hsv2rgb_rainbow keeps mixed hues at low CHSV values (issue #1016)") {
    CRGB rgb = hsv2rgb_rainbow(CHSV(80, 255, 30));

    FL_CHECK_GT(rgb.r, 0);
    FL_CHECK_GT(rgb.g, 0);
    FL_CHECK_EQ(rgb.b, 0);
}

FL_TEST_CASE("HSV to RGB Conversion - Specific Color Tests") {
    FL_WARN("=== Specific Color Conversion Tests ===");
    
    // Test some specific colors known to be challenging
    struct TestColor {
        CRGB rgb;
        const char* name;
    };
    
    vector<TestColor> test_colors = {
        {{255, 0, 0}, "Pure Red"},
        {{0, 255, 0}, "Pure Green"},
        {{0, 0, 255}, "Pure Blue"},
        {{255, 255, 0}, "Yellow"},
        {{255, 0, 255}, "Magenta"},
        {{0, 255, 255}, "Cyan"},
        {{255, 255, 255}, "White"},
        {{0, 0, 0}, "Black"},
        {{128, 128, 128}, "Gray"},
        {{255, 128, 0}, "Orange"},
        {{128, 0, 255}, "Purple"},
        {{255, 192, 203}, "Pink"}
    };
    
    FL_WARN("Color           Original RGB    Rainbow RGB     Spectrum RGB    FullSpectrum RGB");
    FL_WARN("-------------   -----------     -----------     ------------    ----------------");
    
    for (const auto& test : test_colors) {
        CHSV hsv = rgb2hsv_approximate(test.rgb);
        
        CRGB rainbow_rgb, spectrum_rgb, fullspectrum_rgb;
        hsv2rgb_rainbow(hsv, rainbow_rgb);
        hsv2rgb_spectrum(hsv, spectrum_rgb);
        hsv2rgb_fullspectrum(hsv, fullspectrum_rgb);
        
        FL_WARN(test.name << " (" << (int)test.rgb.r << "," << (int)test.rgb.g << "," << (int)test.rgb.b << ")   (" <<
                (int)rainbow_rgb.r << "," << (int)rainbow_rgb.g << "," << (int)rainbow_rgb.b << ")   (" <<
                (int)spectrum_rgb.r << "," << (int)spectrum_rgb.g << "," << (int)spectrum_rgb.b << ")   (" <<
                (int)fullspectrum_rgb.r << "," << (int)fullspectrum_rgb.g << "," << (int)fullspectrum_rgb.b << ")");
    }
    FL_WARN("");
}

// The hue ramps are hand-written piecewise functions: eight 32-hue sections
// selected by bit tests on the hue, joined by magic constants (K85/K170/K171)
// and compile-time switches (Y1/Y2/G2/Gscale). Nothing structurally forces one
// section to end on the value the next one starts from, so a wrong constant
// surfaces as a single off-colour pixel mid-ramp -- exactly the "unexpected red
// pixel midway through the yellow region" reported in FastLED#668.
//
// Measured maxima at full saturation/value are rainbow=6, spectrum=4,
// fullspectrum=6. A broken section boundary is tens, so 8 leaves room for a
// constant to be nudged without hiding a real break.
static const int kMaxHueStep = 8;

static int iabs(int v) { return v < 0 ? -v : v; }

// Largest per-channel change between two adjacent colors.
static int channel_step(const CRGB& a, const CRGB& b) {
    const int dr = iabs(int(b.r) - int(a.r));
    const int dg = iabs(int(b.g) - int(a.g));
    const int db = iabs(int(b.b) - int(a.b));
    int worst = dr > dg ? dr : dg;
    return db > worst ? db : worst;
}

static void check_ramp_is_continuous(const char* name,
                                     void (*convert)(const CHSV&, CRGB&)) {
    int worst = 0;
    int worst_hue = -1;
    for (int hue = 0; hue < 256; hue++) {
        const int next = (hue + 1) & 0xFF;  // includes the 255 -> 0 wrap
        CRGB a, b;
        convert(CHSV(fl::u8(hue), 255, 255), a);
        convert(CHSV(fl::u8(next), 255, 255), b);
        const int step = channel_step(a, b);
        if (step > worst) {
            worst = step;
            worst_hue = hue;
        }
    }
    if (worst > kMaxHueStep) {
        FL_WARN(name << ": discontinuity of " << worst << " between hue "
                     << worst_hue << " and " << ((worst_hue + 1) & 0xFF)
                     << " (allowed " << kMaxHueStep << ")");
    }
    FL_CHECK(worst <= kMaxHueStep);
}

FL_TEST_CASE("HSV to RGB Conversion - hue ramps have no discontinuity") {
    // Every adjacent hue pair, not a sample of every fourth one: a lone bad
    // hue is the whole failure mode, and sampling steps right over it.
    check_ramp_is_continuous("rainbow", hsv2rgb_rainbow);
    check_ramp_is_continuous("spectrum", hsv2rgb_spectrum);
    check_ramp_is_continuous("fullspectrum", hsv2rgb_fullspectrum);
}

FL_TEST_CASE("fill_rainbow has no isolated off-colour pixel (issue #668)") {
    // #668 reported a lone red pixel partway through the yellow region of a
    // 114-LED strip filled with exactly this call.
    const int kNumLeds = 114;
    CRGB leds[kNumLeds];
    fill_rainbow(leds, kNumLeds, 0, 1);

    int worst = 0;
    int worst_index = -1;
    for (int i = 1; i < kNumLeds; i++) {
        const int step = channel_step(leds[i - 1], leds[i]);
        if (step > worst) {
            worst = step;
            worst_index = i;
        }
    }
    if (worst > kMaxHueStep) {
        FL_WARN("fill_rainbow: pixel " << worst_index << " jumps by " << worst
                                       << " (allowed " << kMaxHueStep << ")");
    }
    FL_CHECK(worst <= kMaxHueStep);
}

} // FL_TEST_FILE
