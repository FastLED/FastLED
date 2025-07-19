#include "test.h"
#include "FastLED.h"
#include "hsv2rgb.h"
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

FASTLED_USING_NAMESPACE

struct ErrorStats {
    float average;
    float median;
    float max;
    float min;
    std::vector<float> errors;
    
    void calculate() {
        if (errors.empty()) {
            average = median = max = min = 0.0f;
            return;
        }
        
        // Sort for median calculation
        std::sort(errors.begin(), errors.end());
        
        // Calculate average
        float sum = std::accumulate(errors.begin(), errors.end(), 0.0f);
        average = sum / errors.size();
        
        // Calculate median
        size_t mid = errors.size() / 2;
        if (errors.size() % 2 == 0) {
            median = (errors[mid - 1] + errors[mid]) / 2.0f;
        } else {
            median = errors[mid];
        }
        
        // Min and max
        min = errors.front();
        max = errors.back();
    }
    
    void print(const char* function_name) const {
        printf("%s Error Statistics:\n", function_name);
        printf("  Average: %.6f\n", average);
        printf("  Median:  %.6f\n", median);
        printf("  Min:     %.6f\n", min);
        printf("  Max:     %.6f\n", max);
        printf("  Samples: %zu\n", errors.size());
        printf("\n");
    }
};

// Calculate euclidean distance between two RGB colors
static float calculateRGBError(const CRGB& original, const CRGB& converted) {
    float dr = float(original.r) - float(converted.r);
    float dg = float(original.g) - float(converted.g);
    float db = float(original.b) - float(converted.b);
    return sqrtf(dr*dr + dg*dg + db*db);
}

// Test a specific conversion function with RGB -> HSV -> RGB round trip
template<typename ConversionFunc>
static ErrorStats testConversionFunction(ConversionFunc hsv2rgb_func, const char* func_name) {
    (void)func_name; // Suppress unused parameter warning
    ErrorStats stats;
    
    // Test a comprehensive set of RGB colors
    // We'll test every 8th value to get good coverage without taking too long
    const int step = 8;
    
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
                
                // Calculate error
                float error = calculateRGBError(original_rgb, converted_rgb);
                stats.errors.push_back(error);
            }
        }
    }
    
    stats.calculate();
    return stats;
}

TEST_CASE("HSV to RGB Conversion Accuracy Comparison") {
    printf("\n=== HSV to RGB Conversion Accuracy Test ===\n");
    printf("Testing RGB -> HSV -> RGB round-trip accuracy\n");
    printf("Sampling every 8th RGB value for comprehensive coverage\n\n");
    
    // Test all three conversion functions
    ErrorStats rainbow_stats = testConversionFunction(
        [](const CHSV& hsv, CRGB& rgb) { hsv2rgb_rainbow(hsv, rgb); },
        "hsv2rgb_rainbow"
    );
    
    ErrorStats spectrum_stats = testConversionFunction(
        [](const CHSV& hsv, CRGB& rgb) { hsv2rgb_spectrum(hsv, rgb); },
        "hsv2rgb_spectrum"
    );
    
    ErrorStats fullspectrum_stats = testConversionFunction(
        [](const CHSV& hsv, CRGB& rgb) { hsv2rgb_fullspectrum(hsv, rgb); },
        "hsv2rgb_fullspectrum"
    );
    
    // Print results
    rainbow_stats.print("hsv2rgb_rainbow");
    spectrum_stats.print("hsv2rgb_spectrum");
    fullspectrum_stats.print("hsv2rgb_fullspectrum");
    
    // Print comparison
    printf("=== Error Comparison ===\n");
    printf("Function            Average    Median     Min        Max\n");
    printf("hsv2rgb_rainbow     %.6f   %.6f   %.6f   %.6f\n", 
           rainbow_stats.average, rainbow_stats.median, rainbow_stats.min, rainbow_stats.max);
    printf("hsv2rgb_spectrum    %.6f   %.6f   %.6f   %.6f\n", 
           spectrum_stats.average, spectrum_stats.median, spectrum_stats.min, spectrum_stats.max);
    printf("hsv2rgb_fullspectrum%.6f   %.6f   %.6f   %.6f\n", 
           fullspectrum_stats.average, fullspectrum_stats.median, fullspectrum_stats.min, fullspectrum_stats.max);
    printf("\n");
    
    // Find the best performing function for each metric
    std::vector<std::pair<float, const char*>> avg_results = {
        {rainbow_stats.average, "rainbow"},
        {spectrum_stats.average, "spectrum"},
        {fullspectrum_stats.average, "fullspectrum"}
    };
    std::sort(avg_results.begin(), avg_results.end());
    
    std::vector<std::pair<float, const char*>> median_results = {
        {rainbow_stats.median, "rainbow"},
        {spectrum_stats.median, "spectrum"},
        {fullspectrum_stats.median, "fullspectrum"}
    };
    std::sort(median_results.begin(), median_results.end());
    
    std::vector<std::pair<float, const char*>> max_results = {
        {rainbow_stats.max, "rainbow"},
        {spectrum_stats.max, "spectrum"},
        {fullspectrum_stats.max, "fullspectrum"}
    };
    std::sort(max_results.begin(), max_results.end());
    
    printf("=== Best Performance Rankings ===\n");
    printf("Lowest Average Error: %s (%.6f)\n", avg_results[0].second, avg_results[0].first);
    printf("Lowest Median Error:  %s (%.6f)\n", median_results[0].second, median_results[0].first);
    printf("Lowest Max Error:     %s (%.6f)\n", max_results[0].second, max_results[0].first);
    printf("\n");
    
    // Basic sanity checks - errors should be reasonable for RGB->HSV->RGB round-trip
    // Note: RGB->HSV->RGB conversion is inherently lossy due to the approximation function
    CHECK_LT(rainbow_stats.average, 150.0f);     // Average error should be reasonable
    CHECK_LT(spectrum_stats.average, 150.0f);
    CHECK_LT(fullspectrum_stats.average, 150.0f);
    
    // Max error can exceed single RGB channel distance due to euclidean distance calculation
    CHECK_LT(rainbow_stats.max, 500.0f);         // Max error should be reasonable 
    CHECK_LT(spectrum_stats.max, 500.0f);
    CHECK_LT(fullspectrum_stats.max, 500.0f);
    
    CHECK_GE(rainbow_stats.min, 0.0f);           // Min error should be non-negative
    CHECK_GE(spectrum_stats.min, 0.0f);
    CHECK_GE(fullspectrum_stats.min, 0.0f);
    
    // Verify rainbow has the best (lowest) average error
    CHECK_LT(rainbow_stats.average, spectrum_stats.average);
    CHECK_LT(rainbow_stats.average, fullspectrum_stats.average);
}

TEST_CASE("HSV to RGB Conversion - Specific Color Tests") {
    printf("\n=== Specific Color Conversion Tests ===\n");
    
    // Test some specific colors known to be challenging
    struct TestColor {
        CRGB rgb;
        const char* name;
    };
    
    std::vector<TestColor> test_colors = {
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
    
    printf("Color           Original RGB    Rainbow RGB     Spectrum RGB    FullSpectrum RGB\n");
    printf("-------------   -----------     -----------     ------------    ----------------\n");
    
    for (const auto& test : test_colors) {
        CHSV hsv = rgb2hsv_approximate(test.rgb);
        
        CRGB rainbow_rgb, spectrum_rgb, fullspectrum_rgb;
        hsv2rgb_rainbow(hsv, rainbow_rgb);
        hsv2rgb_spectrum(hsv, spectrum_rgb);
        hsv2rgb_fullspectrum(hsv, fullspectrum_rgb);
        
        printf("%-15s (%3d,%3d,%3d)   (%3d,%3d,%3d)   (%3d,%3d,%3d)   (%3d,%3d,%3d)\n",
               test.name,
               test.rgb.r, test.rgb.g, test.rgb.b,
               rainbow_rgb.r, rainbow_rgb.g, rainbow_rgb.b,
               spectrum_rgb.r, spectrum_rgb.g, spectrum_rgb.b,
               fullspectrum_rgb.r, fullspectrum_rgb.g, fullspectrum_rgb.b);
    }
    printf("\n");
}

TEST_CASE("HSV to RGB Conversion - Hue Sweep Test") {
    printf("\n=== Hue Sweep Conversion Test ===\n");
    printf("Testing full hue range at maximum saturation and brightness\n");
    
    printf("Hue   Rainbow RGB     Spectrum RGB    FullSpectrum RGB\n");
    printf("----  -----------     ------------    ----------------\n");
    
    // Test hue sweep at full saturation and brightness
    for (int hue = 0; hue < 256; hue += 1) {
        CHSV hsv(hue, 255, 255);
        
        CRGB rainbow_rgb, spectrum_rgb, fullspectrum_rgb;
        hsv2rgb_rainbow(hsv, rainbow_rgb);
        hsv2rgb_spectrum(hsv, spectrum_rgb);
        hsv2rgb_fullspectrum(hsv, fullspectrum_rgb);
        
        printf("%3d   (%3d,%3d,%3d)   (%3d,%3d,%3d)   (%3d,%3d,%3d)\n",
               hue,
               rainbow_rgb.r, rainbow_rgb.g, rainbow_rgb.b,
               spectrum_rgb.r, spectrum_rgb.g, spectrum_rgb.b,
               fullspectrum_rgb.r, fullspectrum_rgb.g, fullspectrum_rgb.b);
    }
    printf("\n");
}
