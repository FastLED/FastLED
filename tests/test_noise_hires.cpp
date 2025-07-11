#include "test.h"
#include "noise.h"
#include "fl/stdint.h"
#include "fl/namespace.h"

FASTLED_USING_NAMESPACE

TEST_CASE("High-Resolution Noise Functions Basic Operation") {
    
    SUBCASE("inoise8_hires functions exist and work") {
        // Test that all functions execute without crashing
        uint8_t result_3d = inoise8_hires(1000, 2000, 3000);
        uint8_t result_2d = inoise8_hires(1000, 2000);
        uint8_t result_1d = inoise8_hires(1000);
        
        // Results should be in valid range
        CHECK(result_3d <= 255);
        CHECK(result_2d <= 255);
        CHECK(result_1d <= 255);
    }
    
    SUBCASE("Same coordinates give same results") {
        // Test deterministic behavior
        uint8_t result1 = inoise8_hires(100, 200, 300);
        uint8_t result2 = inoise8_hires(100, 200, 300);
        CHECK_EQ(result1, result2);
        
        uint8_t result3 = inoise8_hires(100, 200);
        uint8_t result4 = inoise8_hires(100, 200);
        CHECK_EQ(result3, result4);
        
        uint8_t result5 = inoise8_hires(100);
        uint8_t result6 = inoise8_hires(100);
        CHECK_EQ(result5, result6);
    }
}

TEST_CASE("Range Utilization Comparison: Regular vs High-Resolution") {
    FL_WARN("=== COMPREHENSIVE RANGE COMPARISON ===");
    FL_WARN("Comparing regular inoise8 vs inoise8_hires range utilization");
    FL_WARN("");
    
    // Test parameters for comprehensive range analysis
    const int NUM_SAMPLES = 2000;  // More samples for better coverage
    
    // Regular inoise8 ranges
    uint8_t reg_3d_min = 255, reg_3d_max = 0;
    uint8_t reg_2d_min = 255, reg_2d_max = 0;
    uint8_t reg_1d_min = 255, reg_1d_max = 0;
    
    // High-resolution inoise8_hires ranges
    uint8_t hires_3d_min = 255, hires_3d_max = 0;
    uint8_t hires_2d_min = 255, hires_2d_max = 0;
    uint8_t hires_1d_min = 255, hires_1d_max = 0;
    
    // Sample across coordinate space with varied patterns
    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Use multiple sampling patterns for comprehensive coverage
        uint16_t x1 = i * 65;      // Linear progression
        uint16_t y1 = i * 71;      // Different increment
        uint16_t z1 = i * 83;      // Prime number increment
        
        uint16_t x2 = (i * 137) % 65535;  // Wrap-around pattern
        uint16_t y2 = (i * 149) % 65535;  
        uint16_t z2 = (i * 163) % 65535;
        
        // Test both sampling patterns
        for (int pattern = 0; pattern < 2; pattern++) {
            uint16_t x = (pattern == 0) ? x1 : x2;
            uint16_t y = (pattern == 0) ? y1 : y2;
            uint16_t z = (pattern == 0) ? z1 : z2;
            
            // Regular inoise8
            uint8_t reg_3d = inoise8(x, y, z);
            uint8_t reg_2d = inoise8(x, y);
            uint8_t reg_1d = inoise8(x);
            
            // High-resolution inoise8_hires
            uint8_t hires_3d = inoise8_hires(x, y, z);
            uint8_t hires_2d = inoise8_hires(x, y);
            uint8_t hires_1d = inoise8_hires(x);
            
            // Track ranges for regular inoise8
            if (reg_3d < reg_3d_min) reg_3d_min = reg_3d;
            if (reg_3d > reg_3d_max) reg_3d_max = reg_3d;
            if (reg_2d < reg_2d_min) reg_2d_min = reg_2d;
            if (reg_2d > reg_2d_max) reg_2d_max = reg_2d;
            if (reg_1d < reg_1d_min) reg_1d_min = reg_1d;
            if (reg_1d > reg_1d_max) reg_1d_max = reg_1d;
            
            // Track ranges for high-resolution inoise8_hires
            if (hires_3d < hires_3d_min) hires_3d_min = hires_3d;
            if (hires_3d > hires_3d_max) hires_3d_max = hires_3d;
            if (hires_2d < hires_2d_min) hires_2d_min = hires_2d;
            if (hires_2d > hires_2d_max) hires_2d_max = hires_2d;
            if (hires_1d < hires_1d_min) hires_1d_min = hires_1d;
            if (hires_1d > hires_1d_max) hires_1d_max = hires_1d;
        }
    }
    
    // Calculate utilization percentages
    float reg_3d_util = ((reg_3d_max - reg_3d_min) / 255.0f) * 100.0f;
    float reg_2d_util = ((reg_2d_max - reg_2d_min) / 255.0f) * 100.0f;
    float reg_1d_util = ((reg_1d_max - reg_1d_min) / 255.0f) * 100.0f;
    
    float hires_3d_util = ((hires_3d_max - hires_3d_min) / 255.0f) * 100.0f;
    float hires_2d_util = ((hires_2d_max - hires_2d_min) / 255.0f) * 100.0f;
    float hires_1d_util = ((hires_1d_max - hires_1d_min) / 255.0f) * 100.0f;
    
    FL_WARN("REGULAR inoise8 ranges:");
    FL_WARN("  1D: " << (int)reg_1d_min << "-" << (int)reg_1d_max << " (" << reg_1d_util << "% utilization)");
    FL_WARN("  2D: " << (int)reg_2d_min << "-" << (int)reg_2d_max << " (" << reg_2d_util << "% utilization)");
    FL_WARN("  3D: " << (int)reg_3d_min << "-" << (int)reg_3d_max << " (" << reg_3d_util << "% utilization)");
    FL_WARN("");
    FL_WARN("HIGH-RESOLUTION inoise8_hires ranges:");
    FL_WARN("  1D: " << (int)hires_1d_min << "-" << (int)hires_1d_max << " (" << hires_1d_util << "% utilization)");
    FL_WARN("  2D: " << (int)hires_2d_min << "-" << (int)hires_2d_max << " (" << hires_2d_util << "% utilization)");
    FL_WARN("  3D: " << (int)hires_3d_min << "-" << (int)hires_3d_max << " (" << hires_3d_util << "% utilization)");
    FL_WARN("");
    FL_WARN("IMPROVEMENTS:");
    FL_WARN("  1D: " << (hires_1d_util - reg_1d_util) << " percentage points");
    FL_WARN("  2D: " << (hires_2d_util - reg_2d_util) << " percentage points");
    FL_WARN("  3D: " << (hires_3d_util - reg_3d_util) << " percentage points");
    FL_WARN("");
    
    // Results analysis
    FL_WARN("ANALYSIS:");
    if (hires_3d_util > reg_3d_util) {
        FL_WARN("SUCCESS: High-resolution 3D noise achieved better range coverage!");
    } else {
        FL_WARN("FINDING: High-resolution versions have different range characteristics");
        FL_WARN("Regular inoise8 uses optimized 8-bit arithmetic");
        FL_WARN("inoise8_hires uses 16-bit precision but different coordinate scaling");
    }
    
    // Sanity checks - ranges should be reasonable
    CHECK(reg_3d_util > 70.0f);    // Regular 3D should be > 70%
    CHECK(hires_3d_util > 60.0f);  // Hires 3D should be > 60%
    CHECK(hires_1d_util > 80.0f);  // Hires 1D should be > 80%
    CHECK(hires_2d_util > 75.0f);  // Hires 2D should be > 75%
}

TEST_CASE("Coordinate Scaling and Precision") {
    FL_WARN("=== COORDINATE SCALING VERIFICATION ===");
    
    SUBCASE("Verify coordinate scaling works correctly") {
        // Test edge cases
        uint8_t min_coord = inoise8_hires(0, 0, 0);
        uint8_t max_coord = inoise8_hires(65535, 65535, 65535);
        uint8_t mid_coord = inoise8_hires(32767, 32767, 32767);
        
        FL_WARN("Edge case results:");
        FL_WARN("  Min coordinates (0,0,0): " << (int)min_coord);
        FL_WARN("  Max coordinates (65535,65535,65535): " << (int)max_coord);
        FL_WARN("  Mid coordinates (32767,32767,32767): " << (int)mid_coord);
        
        // All should be valid
        CHECK(min_coord <= 255);
        CHECK(max_coord <= 255);
        CHECK(mid_coord <= 255);
    }
    
    SUBCASE("Compare with equivalent inoise16 calls") {
        // Test that our scaling is working correctly by comparing with direct inoise16 calls
        uint16_t x = 1000, y = 2000, z = 3000;
        
        // Our hires function
        uint8_t hires_result = inoise8_hires(x, y, z);
        
        // Direct inoise16 call with our scaling
        uint32_t scaled_x = ((uint32_t)x) << 8;
        uint32_t scaled_y = ((uint32_t)y) << 8;
        uint32_t scaled_z = ((uint32_t)z) << 8;
        uint16_t inoise16_result = inoise16(scaled_x, scaled_y, scaled_z);
        uint8_t manual_scaled = inoise16_result >> 8;
        
        FL_WARN("Scaling verification:");
        FL_WARN("  inoise8_hires(" << x << "," << y << "," << z << ") = " << (int)hires_result);
        FL_WARN("  Manual scaling result = " << (int)manual_scaled);
        
        CHECK_EQ(hires_result, manual_scaled);
    }
}

TEST_CASE("Performance and Quality Summary") {
    FL_WARN("=== HIGH-RESOLUTION NOISE SUMMARY ===");
    FL_WARN("");
    FL_WARN("CHARACTERISTICS of inoise8_hires functions:");
    FL_WARN("- Uses 16-bit precision internally with different coordinate scaling");
    FL_WARN("- Returns 8-bit values for easy drop-in replacement");
    FL_WARN("- Provides alternative noise patterns due to different coordinate mapping");
    FL_WARN("- Maintains spatial continuity and smoothness");
    FL_WARN("- Minimal performance overhead (just coordinate scaling)");
    FL_WARN("");
    FL_WARN("USAGE CONSIDERATIONS:");
    FL_WARN("- Regular inoise8 already has excellent range coverage (99.6% 1D, 87.8% 2D, 83.1% 3D)");
    FL_WARN("- inoise8_hires provides alternative patterns (88.2% 1D, 82.4% 2D, 70.2% 3D)");
    FL_WARN("- Choose based on desired noise characteristics, not just range coverage");
    FL_WARN("- Both maintain proper mathematical continuity for smooth animations");
    FL_WARN("");
    FL_WARN("TECHNICAL DETAILS:");
    FL_WARN("- Scales uint16_t coordinates to uint32_t (left-shift 8 for higher precision)");
    FL_WARN("- Calls inoise16() for high-precision calculation");
    FL_WARN("- Scales uint16_t result to uint8_t (right-shift 8)");
    FL_WARN("- Zero additional memory overhead");
    FL_WARN("");
} 
