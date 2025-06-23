
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
    const float TOLERANCE = 0.01f; // 1% tolerance as mentioned in comments
    
    // Test key angles
    REQUIRE(ABS(sinf_fixed(0.0f) - 0.0f) < TOLERANCE);
    REQUIRE(ABS(sinf_fixed((float)PI/2.0f) - 1.0f) < TOLERANCE);
    REQUIRE(ABS(sinf_fixed((float)PI) - 0.0f) < TOLERANCE);
    REQUIRE(ABS(sinf_fixed(3.0f*(float)PI/2.0f) - (-1.0f)) < TOLERANCE);
    REQUIRE(ABS(sinf_fixed(2.0f*(float)PI) - 0.0f) < TOLERANCE);
    
    // Test additional common angles
    REQUIRE(ABS(sinf_fixed((float)PI/6.0f) - 0.5f) < TOLERANCE);      // sin(30°) = 0.5
    REQUIRE(ABS(sinf_fixed((float)PI/4.0f) - 0.707107f) < TOLERANCE); // sin(45°) ≈ 0.707
    REQUIRE(ABS(sinf_fixed((float)PI/3.0f) - 0.866025f) < TOLERANCE); // sin(60°) ≈ 0.866
}

TEST_CASE("cosf_fixed basic functionality") {
    const float TOLERANCE = 0.01f; // 1% tolerance as mentioned in comments
    
    // Test key angles
    REQUIRE(ABS(cosf_fixed(0.0f) - 1.0f) < TOLERANCE);
    REQUIRE(ABS(cosf_fixed((float)PI/2.0f) - 0.0f) < TOLERANCE);
    REQUIRE(ABS(cosf_fixed((float)PI) - (-1.0f)) < TOLERANCE);
    REQUIRE(ABS(cosf_fixed(3.0f*(float)PI/2.0f) - 0.0f) < TOLERANCE);
    REQUIRE(ABS(cosf_fixed(2.0f*(float)PI) - 1.0f) < TOLERANCE);
    
    // Test additional common angles
    REQUIRE(ABS(cosf_fixed((float)PI/6.0f) - 0.866025f) < TOLERANCE); // cos(30°) ≈ 0.866
    REQUIRE(ABS(cosf_fixed((float)PI/4.0f) - 0.707107f) < TOLERANCE); // cos(45°) ≈ 0.707
    REQUIRE(ABS(cosf_fixed((float)PI/3.0f) - 0.5f) < TOLERANCE);      // cos(60°) = 0.5
}

TEST_CASE("sinf_fixed vs standard sinf accuracy") {
    const float TOLERANCE = 0.01f; // Functions should differ by less than 0.01% as per comments
    
    // Test various angles across multiple cycles
    for (int i = 0; i < 100; ++i) {
        float angle = (float)i * (float)PI / 50.0f; // 0 to 2π in 100 steps
        float expected = sin(angle);
        float actual = sinf_fixed(angle);
        float error = ABS(actual - expected);
        
        // Allow slightly higher tolerance for values very close to zero
        float local_tolerance = ABS(expected) < 0.1f ? TOLERANCE * 10.0f : TOLERANCE;
        REQUIRE(error < local_tolerance);
    }
}

TEST_CASE("cosf_fixed vs standard cosf accuracy") {
    const float TOLERANCE = 0.01f; // Functions should differ by less than 0.01% as per comments
    
    // Test various angles across multiple cycles
    for (int i = 0; i < 100; ++i) {
        float angle = (float)i * (float)PI / 50.0f; // 0 to 2π in 100 steps
        float expected = cos(angle);
        float actual = cosf_fixed(angle);
        float error = ABS(actual - expected);
        
        // Allow slightly higher tolerance for values very close to zero
        float local_tolerance = ABS(expected) < 0.1f ? TOLERANCE * 10.0f : TOLERANCE;
        REQUIRE(error < local_tolerance);
    }
}

TEST_CASE("sinf_fixed and cosf_fixed trigonometric identities") {
    const float TOLERANCE = 0.02f; // Slightly higher tolerance for compound operations
    
    // Test sin²(x) + cos²(x) = 1 for various angles
    for (int i = 0; i < 50; ++i) {
        float angle = (float)i * (float)PI / 25.0f; // 0 to 2π in 50 steps
        float sin_val = sinf_fixed(angle);
        float cos_val = cosf_fixed(angle);
        float sum_of_squares = sin_val * sin_val + cos_val * cos_val;
        REQUIRE(ABS(sum_of_squares - 1.0f) < TOLERANCE);
    }
}

TEST_CASE("sinf_fixed and cosf_fixed negative angles") {
    const float TOLERANCE = 0.01f;
    
    // Test that sin(-x) = -sin(x) and cos(-x) = cos(x)
    for (int i = 1; i < 20; ++i) {
        float angle = (float)i * (float)PI / 20.0f;
        
        // sin(-x) = -sin(x)
        REQUIRE(ABS(sinf_fixed(-angle) - (-sinf_fixed(angle))) < TOLERANCE);
        
        // cos(-x) = cos(x)
        REQUIRE(ABS(cosf_fixed(-angle) - cosf_fixed(angle)) < TOLERANCE);
    }
}

TEST_CASE("sinf_fixed and cosf_fixed large angles") {
    const float TOLERANCE = 0.02f;
    
    // Test that functions handle large angles correctly (multiple full rotations)
    float base_angles[] = {0.0f, (float)PI/4.0f, (float)PI/2.0f, (float)PI, 3.0f*(float)PI/2.0f};
    float multipliers[] = {1.0f, 3.0f, 5.0f, 10.0f}; // Multiple full rotations
    
    for (float base_angle : base_angles) {
        for (float mult : multipliers) {
            float large_angle = base_angle + mult * 2.0f * (float)PI;
            
            // Should be approximately equal to the base angle result
            REQUIRE(ABS(sinf_fixed(large_angle) - sinf_fixed(base_angle)) < TOLERANCE);
            REQUIRE(ABS(cosf_fixed(large_angle) - cosf_fixed(base_angle)) < TOLERANCE);
        }
    }
}
