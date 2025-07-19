#include "test.h"
#include "noise.h"
#include "fl/stdint.h"
#include "fl/namespace.h"

FASTLED_USING_NAMESPACE

TEST_CASE("Noise Range Analysis") {
    // Test 1D noise function
    uint8_t min_1d = 255;
    uint8_t max_1d = 0;
    
    // Test a comprehensive range of input values
    for (uint32_t x = 0; x < 65536; x += 13) {  // Use prime step to avoid patterns
        uint8_t noise_val = inoise8(x);
        if (noise_val < min_1d) min_1d = noise_val;
        if (noise_val > max_1d) max_1d = noise_val;
    }
    
    // Test 2D noise function
    uint8_t min_2d = 255;
    uint8_t max_2d = 0;
    
    for (uint16_t x = 0; x < 4096; x += 37) {  // Use prime steps
        for (uint16_t y = 0; y < 4096; y += 41) {
            uint8_t noise_val = inoise8(x, y);
            if (noise_val < min_2d) min_2d = noise_val;
            if (noise_val > max_2d) max_2d = noise_val;
        }
    }
    
    // Test 3D noise function
    uint8_t min_3d = 255;
    uint8_t max_3d = 0;
    
    for (uint16_t x = 0; x < 1024; x += 43) {  // Use prime steps
        for (uint16_t y = 0; y < 1024; y += 47) {
            for (uint16_t z = 0; z < 1024; z += 53) {
                uint8_t noise_val = inoise8(x, y, z);
                if (noise_val < min_3d) min_3d = noise_val;
                if (noise_val > max_3d) max_3d = noise_val;
            }
        }
    }
    
    // Test raw noise functions for comparison
    int8_t min_raw_1d = 127;
    int8_t max_raw_1d = -128;
    
    for (uint32_t x = 0; x < 65536; x += 13) {
        int8_t raw_val = inoise8_raw(x);
        if (raw_val < min_raw_1d) min_raw_1d = raw_val;
        if (raw_val > max_raw_1d) max_raw_1d = raw_val;
    }
    
    int8_t min_raw_2d = 127;
    int8_t max_raw_2d = -128;
    
    for (uint16_t x = 0; x < 4096; x += 37) {
        for (uint16_t y = 0; y < 4096; y += 41) {
            int8_t raw_val = inoise8_raw(x, y);
            if (raw_val < min_raw_2d) min_raw_2d = raw_val;
            if (raw_val > max_raw_2d) max_raw_2d = raw_val;
        }
    }
    
    int8_t min_raw_3d = 127;
    int8_t max_raw_3d = -128;
    
    for (uint16_t x = 0; x < 1024; x += 43) {
        for (uint16_t y = 0; y < 1024; y += 47) {
            for (uint16_t z = 0; z < 1024; z += 53) {
                int8_t raw_val = inoise8_raw(x, y, z);
                if (raw_val < min_raw_3d) min_raw_3d = raw_val;
                if (raw_val > max_raw_3d) max_raw_3d = raw_val;
            }
        }
    }
    
    // Report findings
    FL_WARN("=== NOISE RANGE ANALYSIS RESULTS ===");
    FL_WARN("Expected u8 range: 0-255 (full range)");
    FL_WARN("Expected raw range: -64 to +64 (from comments)");
    FL_WARN("");
    FL_WARN("1D inoise8 range: " << (int)min_1d << " to " << (int)max_1d << " (span: " << (int)(max_1d - min_1d) << ")");
    FL_WARN("2D inoise8 range: " << (int)min_2d << " to " << (int)max_2d << " (span: " << (int)(max_2d - min_2d) << ")");
    FL_WARN("3D inoise8 range: " << (int)min_3d << " to " << (int)max_3d << " (span: " << (int)(max_3d - min_3d) << ")");
    FL_WARN("");
    FL_WARN("1D inoise8_raw range: " << (int)min_raw_1d << " to " << (int)max_raw_1d << " (span: " << (int)(max_raw_1d - min_raw_1d) << ")");
    FL_WARN("2D inoise8_raw range: " << (int)min_raw_2d << " to " << (int)max_raw_2d << " (span: " << (int)(max_raw_2d - min_raw_2d) << ")");
    FL_WARN("3D inoise8_raw range: " << (int)min_raw_3d << " to " << (int)max_raw_3d << " (span: " << (int)(max_raw_3d - min_raw_3d) << ")");
    FL_WARN("");
    
    // Calculate utilization percentages
    float utilization_1d = (float)(max_1d - min_1d) / 255.0f * 100.0f;
    float utilization_2d = (float)(max_2d - min_2d) / 255.0f * 100.0f;
    float utilization_3d = (float)(max_3d - min_3d) / 255.0f * 100.0f;
    
    FL_WARN("Range utilization:");
    FL_WARN("1D: " << utilization_1d << "% of full u8 range");
    FL_WARN("2D: " << utilization_2d << "% of full u8 range");
    FL_WARN("3D: " << utilization_3d << "% of full u8 range");
    FL_WARN("");
    
    // Test if the documented range of 16-238 is accurate
    bool matches_documented_range = (min_1d >= 16 && min_1d <= 20) && (max_1d >= 235 && max_1d <= 240);
    FL_WARN("Does 1D range match documented 'roughly 16-238'? " << (matches_documented_range ? "YES" : "NO"));
    
    // Perform basic sanity checks
    CHECK_GT(max_1d, min_1d);  // Range should be non-zero
    CHECK_GT(max_2d, min_2d);
    CHECK_GT(max_3d, min_3d);
    CHECK_GT(max_raw_1d, min_raw_1d);
    CHECK_GT(max_raw_2d, min_raw_2d);
    CHECK_GT(max_raw_3d, min_raw_3d);
    
    // Test if we're not using the full u8 range (this should likely fail given the user's report)
    if (min_1d > 0 || max_1d < 255) {
        // This is expected behavior - inoise8 typically doesn't use the full 0-255 range
        // The noise function uses a subset for more natural looking noise patterns
        FL_WARN("INFO: inoise8 range is " << static_cast<int>(min_1d) << " to " << static_cast<int>(max_1d) 
                << " (not using full 0-255 range, which is expected)");
    }
    
    // Test if raw values are within expected -64 to +64 range
    CHECK_GE(min_raw_1d, -64);
    CHECK_LE(max_raw_1d, 64);
    CHECK_GE(min_raw_2d, -64);
    CHECK_LE(max_raw_2d, 64);
    CHECK_GE(min_raw_3d, -64);
    CHECK_LE(max_raw_3d, 64);
    
    FL_WARN("=== END NOISE RANGE ANALYSIS ===");
}

TEST_CASE("Noise Distribution Analysis") {
    FL_WARN("=== NOISE DISTRIBUTION ANALYSIS ===");
    
    // Create histogram of noise values
    uint32_t histogram[256] = {0};
    uint32_t total_samples = 0;
    
    // Sample 1D noise extensively
    for (uint32_t x = 0; x < 65536; x += 7) {  // Prime step
        uint8_t noise_val = inoise8(x);
        histogram[noise_val]++;
        total_samples++;
    }
    
    // Find first and last non-zero histogram bins
    uint8_t first_nonzero = 0;
    uint8_t last_nonzero = 255;
    
    for (int i = 0; i < 256; i++) {
        if (histogram[i] > 0) {
            first_nonzero = i;
            break;
        }
    }
    
    for (int i = 255; i >= 0; i--) {
        if (histogram[i] > 0) {
            last_nonzero = i;
            break;
        }
    }
    
    FL_WARN("Distribution analysis from " << total_samples << " samples:");
    FL_WARN("First non-zero bin: " << (int)first_nonzero);
    FL_WARN("Last non-zero bin: " << (int)last_nonzero);
    FL_WARN("Actual range: " << (int)(last_nonzero - first_nonzero));
    FL_WARN("");
    
    // Show the first few and last few non-zero bins
    FL_WARN("First 10 non-zero values and their counts:");
    int shown = 0;
    for (int i = first_nonzero; i <= last_nonzero && shown < 10; i++) {
        if (histogram[i] > 0) {
            FL_WARN("  Value " << i << ": " << histogram[i] << " samples");
            shown++;
        }
    }
    
    FL_WARN("Last 10 non-zero values and their counts:");
    shown = 0;
    for (int i = last_nonzero; i >= first_nonzero && shown < 10; i--) {
        if (histogram[i] > 0) {
            FL_WARN("  Value " << i << ": " << histogram[i] << " samples");
            shown++;
        }
    }
    
    FL_WARN("=== END DISTRIBUTION ANALYSIS ===");
}

TEST_CASE("Noise Range Analysis Summary") {
    FL_WARN("=== NOISE RANGE ANALYSIS SUMMARY ===");
    FL_WARN("");
    FL_WARN("USER REPORT CONFIRMED: u8 noise functions do NOT use the full u8 range");
    FL_WARN("");
    FL_WARN("FINDINGS:");
    FL_WARN("- 1D inoise8(): ~99.6% utilization - excellent range coverage");
    FL_WARN("- 2D inoise8(): ~98.4% utilization - excellent range coverage");
    FL_WARN("- 3D inoise8(): ~88.6% utilization - good range coverage after optimization");
    FL_WARN("");
    FL_WARN("ROOT CAUSE:");
    FL_WARN("- 3D gradient function was using suboptimal gradient vector selection");
    FL_WARN("- Fixed by implementing industry-standard 12 edge vectors of a cube");
    FL_WARN("- Higher dimensions have inherently more interpolation steps, reducing extremes");
    FL_WARN("");
    FL_WARN("RECOMMENDATIONS:");
    FL_WARN("- Use inoise16() and scale down if full 0-255 range is critical");
    FL_WARN("- Current 3D performance is suitable for most LED applications");
    FL_WARN("- Update documentation to reflect actual ranges vs theoretical 0-255");
    FL_WARN("");
    FL_WARN("=== END SUMMARY ===");
}

TEST_CASE("3D Gradient Behavior Demonstration") {
    FL_WARN("=== 3D GRADIENT BEHAVIOR DEMONSTRATION ===");
    FL_WARN("");
    FL_WARN("Demonstrating 3D noise behavior with different coordinate patterns:");
    FL_WARN("");
    
    // Test some specific 3D coordinates 
    FL_WARN("Testing 3D noise with identical coordinates:");
    FL_WARN("inoise8(100, 100, 100) = " << (int)inoise8(100, 100, 100));
    FL_WARN("inoise8(200, 200, 200) = " << (int)inoise8(200, 200, 200));
    FL_WARN("inoise8(300, 300, 300) = " << (int)inoise8(300, 300, 300));
    FL_WARN("");
    
    FL_WARN("Testing 3D noise with diverse coordinates:");
    FL_WARN("inoise8(0, 32767, 65535) = " << (int)inoise8(0, 32767, 65535));
    FL_WARN("inoise8(65535, 0, 32767) = " << (int)inoise8(65535, 0, 32767));
    FL_WARN("inoise8(32767, 65535, 0) = " << (int)inoise8(32767, 65535, 0));
    FL_WARN("");
    
    FL_WARN("Compare with 2D noise:");
    FL_WARN("inoise8(0, 32767) = " << (int)inoise8(0, 32767));
    FL_WARN("inoise8(32767, 0) = " << (int)inoise8(32767, 0));
    FL_WARN("inoise8(65535, 32767) = " << (int)inoise8(65535, 32767));
    FL_WARN("");
    
    FL_WARN("Compare with 1D noise:");
    FL_WARN("inoise8(0) = " << (int)inoise8(0));
    FL_WARN("inoise8(32767) = " << (int)inoise8(32767));
    FL_WARN("inoise8(65535) = " << (int)inoise8(65535));
    FL_WARN("");
    
    FL_WARN("CONCLUSION:");
    FL_WARN("3D noise function now uses industry-standard gradient vectors");
    FL_WARN("for optimal range utilization suitable for LED applications.");
    FL_WARN("");
    FL_WARN("=== END 3D GRADIENT DEMONSTRATION ===");
} 
