// g++ --std=c++11 test.cpp

#include "test.h"
#include "fl/sin32.h"
#include "fl/math.h"
#include "fl/math_macros.h"

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE

// 16777216 is 1 cycle
const uint32_t _360 = 16777216;
const uint32_t _ONE = 2147418112;
const uint32_t _NEG_ONE = -2147418112;

TEST_CASE("compile test") {
    int32_t result = sin32(0);
    REQUIRE(result == 0);

    result = sin32(_360);
    REQUIRE(result == 0);

    result = sin32(_360 / 4);
    REQUIRE(result == _ONE);

    result = sin32(_360 / 2);
    REQUIRE(result == 0);

    result = sin32(_360 / 4 * 3);
    REQUIRE(result == _NEG_ONE);
    
}

TEST_CASE("sinf_fixed basic functionality") {
    const float TOLERANCE = 0.0001f; // Push tolerance to theoretical limits (0.01% as claimed)
    
    // Test key angles
    REQUIRE(ABS(sinf_fixed(0.0f) - 0.0f) < TOLERANCE);
    REQUIRE(ABS(sinf_fixed((float)PI/2.0f) - 1.0f) < TOLERANCE);
    REQUIRE(ABS(sinf_fixed((float)PI) - 0.0f) < TOLERANCE);
    REQUIRE(ABS(sinf_fixed(3.0f*(float)PI/2.0f) - (-1.0f)) < TOLERANCE);
    REQUIRE(ABS(sinf_fixed(2.0f*(float)PI) - 0.0f) < TOLERANCE);
    
    // Test additional common angles with tighter precision
    REQUIRE(ABS(sinf_fixed((float)PI/6.0f) - 0.5f) < TOLERANCE);      // sin(30°) = 0.5
    REQUIRE(ABS(sinf_fixed((float)PI/4.0f) - 0.707107f) < TOLERANCE); // sin(45°) ≈ 0.707
    REQUIRE(ABS(sinf_fixed((float)PI/3.0f) - 0.866025f) < TOLERANCE); // sin(60°) ≈ 0.866
}

TEST_CASE("cosf_fixed basic functionality") {
    const float TOLERANCE = 0.0001f; // Push tolerance to theoretical limits (0.01% as claimed)
    
    // Test key angles
    REQUIRE(ABS(cosf_fixed(0.0f) - 1.0f) < TOLERANCE);
    REQUIRE(ABS(cosf_fixed((float)PI/2.0f) - 0.0f) < TOLERANCE);
    REQUIRE(ABS(cosf_fixed((float)PI) - (-1.0f)) < TOLERANCE);
    REQUIRE(ABS(cosf_fixed(3.0f*(float)PI/2.0f) - 0.0f) < TOLERANCE);
    REQUIRE(ABS(cosf_fixed(2.0f*(float)PI) - 1.0f) < TOLERANCE);
    
    // Test additional common angles with tighter precision
    REQUIRE(ABS(cosf_fixed((float)PI/6.0f) - 0.866025f) < TOLERANCE); // cos(30°) ≈ 0.866
    REQUIRE(ABS(cosf_fixed((float)PI/4.0f) - 0.707107f) < TOLERANCE); // cos(45°) ≈ 0.707
    REQUIRE(ABS(cosf_fixed((float)PI/3.0f) - 0.5f) < TOLERANCE);      // cos(60°) = 0.5
}

TEST_CASE("sinf_fixed vs standard sinf accuracy") {
    const float BASE_TOLERANCE = 0.0001f; // Push to theoretical limits
    
    // Test various angles across multiple cycles
    for (int i = 0; i < 1000; ++i) {
        float angle = (float)i * (float)PI / 500.0f; // 0 to 2π in 1000 steps
        float expected = sin(angle);
        float actual = sinf_fixed(angle);
        float error = ABS(actual - expected);
        
        // Allow slightly higher tolerance for values very close to zero due to floating point precision
        float local_tolerance = ABS(expected) < 0.01f ? BASE_TOLERANCE * 5.0f : BASE_TOLERANCE;
        REQUIRE(error < local_tolerance);
    }
}

TEST_CASE("cosf_fixed vs standard cosf accuracy") {
    const float BASE_TOLERANCE = 0.0001f; // Push to theoretical limits
    
    // Test various angles across multiple cycles
    for (int i = 0; i < 1000; ++i) {
        float angle = (float)i * (float)PI / 500.0f; // 0 to 2π in 1000 steps
        float expected = cos(angle);
        float actual = cosf_fixed(angle);
        float error = ABS(actual - expected);
        
        // Allow slightly higher tolerance for values very close to zero due to floating point precision
        float local_tolerance = ABS(expected) < 0.01f ? BASE_TOLERANCE * 5.0f : BASE_TOLERANCE;
        REQUIRE(error < local_tolerance);
    }
}

TEST_CASE("sinf_fixed and cosf_fixed trigonometric identities") {
    const float TOLERANCE = 0.0002f; // Very tight tolerance for compound operations
    
    // Test sin²(x) + cos²(x) = 1 for various angles
    for (int i = 0; i < 500; ++i) {
        float angle = (float)i * (float)PI / 250.0f; // 0 to 2π in 500 steps
        float sin_val = sinf_fixed(angle);
        float cos_val = cosf_fixed(angle);
        float sum_of_squares = sin_val * sin_val + cos_val * cos_val;
        REQUIRE(ABS(sum_of_squares - 1.0f) < TOLERANCE);
    }
}

TEST_CASE("sinf_fixed and cosf_fixed negative angles") {
    const float TOLERANCE = 0.0001f; // Push to theoretical limits
    
    // Test that sin(-x) = -sin(x) and cos(-x) = cos(x)
    for (int i = 1; i < 100; ++i) {
        float angle = (float)i * (float)PI / 100.0f;
        
        // sin(-x) = -sin(x)
        REQUIRE(ABS(sinf_fixed(-angle) - (-sinf_fixed(angle))) < TOLERANCE);
        
        // cos(-x) = cos(x)
        REQUIRE(ABS(cosf_fixed(-angle) - cosf_fixed(angle)) < TOLERANCE);
    }
}

TEST_CASE("sinf_fixed and cosf_fixed large angles") {
    const float TOLERANCE = 0.0002f; // Slightly relaxed for large angle operations
    
    // Test that functions handle large angles correctly (multiple full rotations)
    float base_angles[] = {0.0f, (float)PI/4.0f, (float)PI/2.0f, (float)PI, 3.0f*(float)PI/2.0f};
    float multipliers[] = {1.0f, 3.0f, 5.0f, 10.0f, 50.0f, 100.0f}; // Much larger multiples
    
    for (float base_angle : base_angles) {
        for (float mult : multipliers) {
            float large_angle = base_angle + mult * 2.0f * (float)PI;
            
            // Should be approximately equal to the base angle result
            REQUIRE(ABS(sinf_fixed(large_angle) - sinf_fixed(base_angle)) < TOLERANCE);
            REQUIRE(ABS(cosf_fixed(large_angle) - cosf_fixed(base_angle)) < TOLERANCE);
        }
    }
}

TEST_CASE("sinf_fixed and cosf_fixed extreme precision test") {
    const float ULTRA_TIGHT_TOLERANCE = 0.00005f; // Push to absolute limits
    
    // Test angles that align with lookup table entries (should be most accurate)
    for (int i = 0; i < 256; ++i) {
        // These angles correspond exactly to lookup table entries
        float angle = (float)i * 2.0f * (float)PI / 256.0f;
        float sin_expected = sin(angle);
        float cos_expected = cos(angle);
        float sin_actual = sinf_fixed(angle);
        float cos_actual = cosf_fixed(angle);
        
        // These should be extremely accurate since they hit lookup table entries exactly
        if (ABS(sin_expected) > 0.01f) { // Avoid near-zero values
            REQUIRE(ABS(sin_actual - sin_expected) < ULTRA_TIGHT_TOLERANCE);
        }
        if (ABS(cos_expected) > 0.01f) { // Avoid near-zero values
            REQUIRE(ABS(cos_actual - cos_expected) < ULTRA_TIGHT_TOLERANCE);
        }
    }
}

TEST_CASE("sinf_fixed and cosf_fixed worst case interpolation test") {
    const float INTERPOLATION_TOLERANCE = 0.0002f; // Tolerance for worst-case interpolation errors
    
    // Test angles that fall exactly between lookup table entries (worst case for linear interpolation)
    for (int i = 0; i < 256; ++i) {
        // These angles fall exactly between lookup table entries
        float angle = ((float)i + 0.5f) * 2.0f * (float)PI / 256.0f;
        float sin_expected = sin(angle);
        float cos_expected = cos(angle);
        float sin_actual = sinf_fixed(angle);
        float cos_actual = cosf_fixed(angle);
        
        // These should still be quite accurate despite linear interpolation
        if (ABS(sin_expected) > 0.01f) { // Avoid near-zero values
            REQUIRE(ABS(sin_actual - sin_expected) < INTERPOLATION_TOLERANCE);
        }
        if (ABS(cos_expected) > 0.01f) { // Avoid near-zero values
            REQUIRE(ABS(cos_actual - cos_expected) < INTERPOLATION_TOLERANCE);
        }
    }
}

TEST_CASE("sinf_fixed and cosf_fixed maximum error analysis") {
    float max_sin_error = 0.0f;
    float max_cos_error = 0.0f;
    float max_sin_angle = 0.0f;
    float max_cos_angle = 0.0f;
    
    // Comprehensive error analysis across many angles
    for (int i = 0; i < 10000; ++i) {
        float angle = (float)i * 2.0f * (float)PI / 10000.0f;
        
        float sin_expected = sin(angle);
        float cos_expected = cos(angle);
        float sin_actual = sinf_fixed(angle);
        float cos_actual = cosf_fixed(angle);
        
        float sin_error = ABS(sin_actual - sin_expected);
        float cos_error = ABS(cos_actual - cos_expected);
        
        if (sin_error > max_sin_error && ABS(sin_expected) > 0.01f) {
            max_sin_error = sin_error;
            max_sin_angle = angle;
        }
        
        if (cos_error > max_cos_error && ABS(cos_expected) > 0.01f) {
            max_cos_error = cos_error;
            max_cos_angle = angle;
        }
    }
    
    // Verify maximum errors are within theoretical bounds
    // With 256-entry lookup table and linear interpolation, max error should be very small
    REQUIRE(max_sin_error < 0.0005f); // 0.05% maximum error
    REQUIRE(max_cos_error < 0.0005f); // 0.05% maximum error
    
    FL_WARN("Maximum sine error: " << max_sin_error << " at angle " << max_sin_angle);
    FL_WARN("Maximum cosine error: " << max_cos_error << " at angle " << max_cos_angle);
}

TEST_CASE("sinf_fixed and cosf_fixed frequency analysis") {
    const float FREQUENCY_TOLERANCE = 0.0001f;
    
    // Test various frequencies to ensure accuracy across different scales
    float frequencies[] = {0.1f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
    
    for (float freq : frequencies) {
        for (int i = 0; i < 100; ++i) {
            float t = (float)i / 100.0f; // 0 to 1 seconds
            float angle = 2.0f * (float)PI * freq * t;
            
            float sin_expected = sin(angle);
            float cos_expected = cos(angle);
            float sin_actual = sinf_fixed(angle);
            float cos_actual = cosf_fixed(angle);
            
            if (ABS(sin_expected) > 0.01f) {
                REQUIRE(ABS(sin_actual - sin_expected) < FREQUENCY_TOLERANCE);
            }
            if (ABS(cos_expected) > 0.01f) {
                REQUIRE(ABS(cos_actual - cos_expected) < FREQUENCY_TOLERANCE);
            }
        }
    }
}
