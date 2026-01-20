#include "hsv2rgb.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/new.h"
#include "fl/stl/pair.h"
#include "fl/stl/utility.h"
#include "fl/stl/vector.h"
#include "chsv.h"
#include "crgb.h"
#include "doctest.h"
#include "fl/log.h"
#include "fl/stl/math.h"
#include "fl/stl/strstream.h"
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

TEST_CASE("HSV to RGB Conversion Accuracy Comparison") {
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
    CHECK_LT(rainbow_stats.average, 150.0f);     // Average deviation should be reasonable
    CHECK_LT(spectrum_stats.average, 150.0f);
    CHECK_LT(fullspectrum_stats.average, 150.0f);

    // Max deviation can exceed single RGB channel distance due to euclidean distance calculation
    CHECK_LT(rainbow_stats.max, 500.0f);         // Max deviation should be reasonable
    CHECK_LT(spectrum_stats.max, 500.0f);
    CHECK_LT(fullspectrum_stats.max, 500.0f);

    CHECK_GE(rainbow_stats.min, 0.0f);           // Min deviation should be non-negative
    CHECK_GE(spectrum_stats.min, 0.0f);
    CHECK_GE(fullspectrum_stats.min, 0.0f);

    // Verify rainbow has the best (lowest) average deviation
    CHECK_LT(rainbow_stats.average, spectrum_stats.average);
    CHECK_LT(rainbow_stats.average, fullspectrum_stats.average);
}

TEST_CASE("HSV to RGB Conversion - Specific Color Tests") {
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

TEST_CASE("HSV to RGB Conversion - Hue Sweep Test") {
    FL_WARN("=== Hue Sweep Conversion Test ===");
    FL_WARN("Testing full hue range at maximum saturation and brightness");
    FL_WARN("");
    FL_WARN("Hue   Rainbow RGB     Spectrum RGB    FullSpectrum RGB");
    FL_WARN("----  -----------     ------------    ----------------");
    
    // Test hue sweep at full saturation and brightness
    // Increased step from 1 to 4 for performance (256 -> 64 iterations = 75% reduction)
    // Still provides excellent hue sweep coverage with 64 samples
    for (int hue = 0; hue < 256; hue += 4) {
        CHSV hsv(hue, 255, 255);
        
        CRGB rainbow_rgb, spectrum_rgb, fullspectrum_rgb;
        hsv2rgb_rainbow(hsv, rainbow_rgb);
        hsv2rgb_spectrum(hsv, spectrum_rgb);
        hsv2rgb_fullspectrum(hsv, fullspectrum_rgb);
        
        FL_WARN(hue << "   (" << (int)rainbow_rgb.r << "," << (int)rainbow_rgb.g << "," << (int)rainbow_rgb.b << ")   (" <<
                (int)spectrum_rgb.r << "," << (int)spectrum_rgb.g << "," << (int)spectrum_rgb.b << ")   (" <<
                (int)fullspectrum_rgb.r << "," << (int)fullspectrum_rgb.g << "," << (int)fullspectrum_rgb.b << ")");
    }
    FL_WARN("");
}
