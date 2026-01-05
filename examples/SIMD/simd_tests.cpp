// examples/SIMD/simd_tests.cpp
//
// SIMD test implementations

#include "simd_tests.h"
#include "test_helpers.h"
#include <FastLED.h>
#include "fl/simd.h"

using namespace fl;
using namespace fl::simd;

namespace simd_test {

// ============================================================================
// Load/Store Tests
// ============================================================================

void test_load_store_u8_16(TestResult& result) {
    uint8_t input[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    uint8_t output[16] = {0};

    simd_u8x16 v = load_u8_16(input);
    store_u8_16(output, v);

    if (!compare_u8_arrays(input, output, 16)) {
        result.fail("Load/store u8x16 data mismatch");
    }
}

void test_load_store_u32_4(TestResult& result) {
    uint32_t input[4] = {0x12345678, 0x9ABCDEF0, 0xFEDCBA98, 0x76543210};
    uint32_t output[4] = {0};

    simd_u32x4 v = load_u32_4(input);
    store_u32_4(output, v);

    for (int i = 0; i < 4; i++) {
        if (input[i] != output[i]) {
            result.fail("Load/store u32x4 data mismatch");
            return;
        }
    }
}

void test_load_store_f32_4(TestResult& result) {
    float input[4] = {1.5f, 2.5f, 3.5f, 4.5f};
    float output[4] = {0.0f};

    simd_f32x4 v = load_f32_4(input);
    store_f32_4(output, v);

    if (!compare_f32_arrays(input, output, 4)) {
        result.fail("Load/store f32x4 data mismatch");
    }
}

// ============================================================================
// Arithmetic Tests
// ============================================================================

void test_add_sat_u8_16(TestResult& result) {
    uint8_t a_data[16] = {100, 150, 200, 250, 100, 150, 200, 250, 100, 150, 200, 250, 100, 150, 200, 250};
    uint8_t b_data[16] = {50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100};
    uint8_t expected[16] = {150, 250, 250, 255, 150, 250, 250, 255, 150, 250, 250, 255, 150, 250, 250, 255};
    uint8_t output[16] = {0};

    simd_u8x16 a = load_u8_16(a_data);
    simd_u8x16 b = load_u8_16(b_data);
    simd_u8x16 c = add_sat_u8_16(a, b);
    store_u8_16(output, c);

    if (!compare_u8_arrays(expected, output, 16)) {
        result.fail("Saturating add produced incorrect results");
    }
}

void test_sub_sat_u8_16(TestResult& result) {
    uint8_t a_data[16] = {100, 50, 200, 10, 100, 50, 200, 10, 100, 50, 200, 10, 100, 50, 200, 10};
    uint8_t b_data[16] = {50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100};
    uint8_t expected[16] = {50, 0, 150, 0, 50, 0, 150, 0, 50, 0, 150, 0, 50, 0, 150, 0};
    uint8_t output[16] = {0};

    simd_u8x16 a = load_u8_16(a_data);
    simd_u8x16 b = load_u8_16(b_data);
    simd_u8x16 c = sub_sat_u8_16(a, b);
    store_u8_16(output, c);

    if (!compare_u8_arrays(expected, output, 16)) {
        result.fail("Saturating subtract produced incorrect results");
    }
}

void test_scale_u8_16(TestResult& result) {
    uint8_t input[16] = {255, 128, 64, 32, 255, 128, 64, 32, 255, 128, 64, 32, 255, 128, 64, 32};
    uint8_t expected[16] = {127, 64, 32, 16, 127, 64, 32, 16, 127, 64, 32, 16, 127, 64, 32, 16};
    uint8_t output[16] = {0};

    simd_u8x16 v = load_u8_16(input);
    simd_u8x16 scaled = scale_u8_16(v, 128); // Scale by 0.5
    store_u8_16(output, scaled);

    if (!compare_u8_arrays(expected, output, 16)) {
        result.fail("Scale operation produced incorrect results");
    }
}

void test_blend_u8_16(TestResult& result) {
    uint8_t a_data[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t b_data[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    uint8_t expected[16] = {127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127};
    uint8_t output[16] = {0};

    simd_u8x16 a = load_u8_16(a_data);
    simd_u8x16 b = load_u8_16(b_data);
    simd_u8x16 c = blend_u8_16(a, b, 128); // 50% blend
    store_u8_16(output, c);

    if (!compare_u8_arrays(expected, output, 16)) {
        result.fail("Blend operation produced incorrect results");
    }
}

// ============================================================================
// Comparison Tests
// ============================================================================

void test_min_u8_16(TestResult& result) {
    uint8_t a_data[16] = {100, 50, 200, 10, 100, 50, 200, 10, 100, 50, 200, 10, 100, 50, 200, 10};
    uint8_t b_data[16] = {50, 100, 150, 20, 50, 100, 150, 20, 50, 100, 150, 20, 50, 100, 150, 20};
    uint8_t expected[16] = {50, 50, 150, 10, 50, 50, 150, 10, 50, 50, 150, 10, 50, 50, 150, 10};
    uint8_t output[16] = {0};

    simd_u8x16 a = load_u8_16(a_data);
    simd_u8x16 b = load_u8_16(b_data);
    simd_u8x16 c = min_u8_16(a, b);
    store_u8_16(output, c);

    if (!compare_u8_arrays(expected, output, 16)) {
        result.fail("Min operation produced incorrect results");
    }
}

void test_max_u8_16(TestResult& result) {
    uint8_t a_data[16] = {100, 50, 200, 10, 100, 50, 200, 10, 100, 50, 200, 10, 100, 50, 200, 10};
    uint8_t b_data[16] = {50, 100, 150, 20, 50, 100, 150, 20, 50, 100, 150, 20, 50, 100, 150, 20};
    uint8_t expected[16] = {100, 100, 200, 20, 100, 100, 200, 20, 100, 100, 200, 20, 100, 100, 200, 20};
    uint8_t output[16] = {0};

    simd_u8x16 a = load_u8_16(a_data);
    simd_u8x16 b = load_u8_16(b_data);
    simd_u8x16 c = max_u8_16(a, b);
    store_u8_16(output, c);

    if (!compare_u8_arrays(expected, output, 16)) {
        result.fail("Max operation produced incorrect results");
    }
}

void test_avg_u8_16(TestResult& result) {
    uint8_t a_data[16] = {100, 50, 200, 10, 100, 50, 200, 10, 100, 50, 200, 10, 100, 50, 200, 10};
    uint8_t b_data[16] = {50, 100, 150, 20, 50, 100, 150, 20, 50, 100, 150, 20, 50, 100, 150, 20};
    uint8_t expected[16] = {75, 75, 175, 15, 75, 75, 175, 15, 75, 75, 175, 15, 75, 75, 175, 15};
    uint8_t output[16] = {0};

    simd_u8x16 a = load_u8_16(a_data);
    simd_u8x16 b = load_u8_16(b_data);
    simd_u8x16 c = avg_u8_16(a, b);
    store_u8_16(output, c);

    if (!compare_u8_arrays(expected, output, 16)) {
        result.fail("Average operation produced incorrect results");
    }
}

void test_avg_round_u8_16(TestResult& result) {
    uint8_t a_data[16] = {101, 51, 201, 11, 101, 51, 201, 11, 101, 51, 201, 11, 101, 51, 201, 11};
    uint8_t b_data[16] = {50, 100, 150, 20, 50, 100, 150, 20, 50, 100, 150, 20, 50, 100, 150, 20};
    uint8_t expected[16] = {76, 76, 176, 16, 76, 76, 176, 16, 76, 76, 176, 16, 76, 76, 176, 16};
    uint8_t output[16] = {0};

    simd_u8x16 a = load_u8_16(a_data);
    simd_u8x16 b = load_u8_16(b_data);
    simd_u8x16 c = avg_round_u8_16(a, b);
    store_u8_16(output, c);

    if (!compare_u8_arrays(expected, output, 16)) {
        result.fail("Rounding average operation produced incorrect results");
    }
}

// ============================================================================
// Bitwise Tests
// ============================================================================

void test_and_u8_16(TestResult& result) {
    uint8_t a_data[16] = {0xFF, 0xF0, 0x0F, 0xAA, 0xFF, 0xF0, 0x0F, 0xAA, 0xFF, 0xF0, 0x0F, 0xAA, 0xFF, 0xF0, 0x0F, 0xAA};
    uint8_t b_data[16] = {0xF0, 0xFF, 0x0F, 0x55, 0xF0, 0xFF, 0x0F, 0x55, 0xF0, 0xFF, 0x0F, 0x55, 0xF0, 0xFF, 0x0F, 0x55};
    uint8_t expected[16] = {0xF0, 0xF0, 0x0F, 0x00, 0xF0, 0xF0, 0x0F, 0x00, 0xF0, 0xF0, 0x0F, 0x00, 0xF0, 0xF0, 0x0F, 0x00};
    uint8_t output[16] = {0};

    simd_u8x16 a = load_u8_16(a_data);
    simd_u8x16 b = load_u8_16(b_data);
    simd_u8x16 c = and_u8_16(a, b);
    store_u8_16(output, c);

    if (!compare_u8_arrays(expected, output, 16)) {
        result.fail("AND operation produced incorrect results");
    }
}

void test_or_u8_16(TestResult& result) {
    uint8_t a_data[16] = {0xF0, 0x0F, 0xAA, 0x55, 0xF0, 0x0F, 0xAA, 0x55, 0xF0, 0x0F, 0xAA, 0x55, 0xF0, 0x0F, 0xAA, 0x55};
    uint8_t b_data[16] = {0x0F, 0xF0, 0x55, 0xAA, 0x0F, 0xF0, 0x55, 0xAA, 0x0F, 0xF0, 0x55, 0xAA, 0x0F, 0xF0, 0x55, 0xAA};
    uint8_t expected[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t output[16] = {0};

    simd_u8x16 a = load_u8_16(a_data);
    simd_u8x16 b = load_u8_16(b_data);
    simd_u8x16 c = or_u8_16(a, b);
    store_u8_16(output, c);

    if (!compare_u8_arrays(expected, output, 16)) {
        result.fail("OR operation produced incorrect results");
    }
}

void test_xor_u8_16(TestResult& result) {
    uint8_t a_data[16] = {0xFF, 0xF0, 0xAA, 0x55, 0xFF, 0xF0, 0xAA, 0x55, 0xFF, 0xF0, 0xAA, 0x55, 0xFF, 0xF0, 0xAA, 0x55};
    uint8_t b_data[16] = {0xF0, 0xFF, 0x55, 0xAA, 0xF0, 0xFF, 0x55, 0xAA, 0xF0, 0xFF, 0x55, 0xAA, 0xF0, 0xFF, 0x55, 0xAA};
    uint8_t expected[16] = {0x0F, 0x0F, 0xFF, 0xFF, 0x0F, 0x0F, 0xFF, 0xFF, 0x0F, 0x0F, 0xFF, 0xFF, 0x0F, 0x0F, 0xFF, 0xFF};
    uint8_t output[16] = {0};

    simd_u8x16 a = load_u8_16(a_data);
    simd_u8x16 b = load_u8_16(b_data);
    simd_u8x16 c = xor_u8_16(a, b);
    store_u8_16(output, c);

    if (!compare_u8_arrays(expected, output, 16)) {
        result.fail("XOR operation produced incorrect results");
    }
}

void test_andnot_u8_16(TestResult& result) {
    uint8_t a_data[16] = {0xF0, 0x0F, 0xAA, 0x55, 0xF0, 0x0F, 0xAA, 0x55, 0xF0, 0x0F, 0xAA, 0x55, 0xF0, 0x0F, 0xAA, 0x55};
    uint8_t b_data[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t expected[16] = {0x0F, 0xF0, 0x55, 0xAA, 0x0F, 0xF0, 0x55, 0xAA, 0x0F, 0xF0, 0x55, 0xAA, 0x0F, 0xF0, 0x55, 0xAA};
    uint8_t output[16] = {0};

    simd_u8x16 a = load_u8_16(a_data);
    simd_u8x16 b = load_u8_16(b_data);
    simd_u8x16 c = andnot_u8_16(a, b);
    store_u8_16(output, c);

    if (!compare_u8_arrays(expected, output, 16)) {
        result.fail("AND-NOT operation produced incorrect results");
    }
}

// ============================================================================
// Broadcast Tests
// ============================================================================

void test_set1_u32_4(TestResult& result) {
    uint32_t value = 0xDEADBEEF;
    uint32_t expected[4] = {0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF};
    uint32_t output[4] = {0};

    simd_u32x4 v = set1_u32_4(value);
    store_u32_4(output, v);

    for (int i = 0; i < 4; i++) {
        if (output[i] != expected[i]) {
            result.fail("Set1 u32x4 operation produced incorrect results");
            return;
        }
    }
}

void test_set1_f32_4(TestResult& result) {
    float value = 3.14159f;
    float expected[4] = {3.14159f, 3.14159f, 3.14159f, 3.14159f};
    float output[4] = {0.0f};

    simd_f32x4 v = set1_f32_4(value);
    store_f32_4(output, v);

    if (!compare_f32_arrays(expected, output, 4)) {
        result.fail("Set1 f32x4 operation produced incorrect results");
    }
}

// ============================================================================
// Floating Point Tests
// ============================================================================

void test_add_f32_4(TestResult& result) {
    float a_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b_data[4] = {0.5f, 1.5f, 2.5f, 3.5f};
    float expected[4] = {1.5f, 3.5f, 5.5f, 7.5f};
    float output[4] = {0.0f};

    simd_f32x4 a = load_f32_4(a_data);
    simd_f32x4 b = load_f32_4(b_data);
    simd_f32x4 c = add_f32_4(a, b);
    store_f32_4(output, c);

    if (!compare_f32_arrays(expected, output, 4)) {
        result.fail("Float add operation produced incorrect results");
    }
}

void test_sub_f32_4(TestResult& result) {
    float a_data[4] = {5.0f, 4.0f, 3.0f, 2.0f};
    float b_data[4] = {1.5f, 1.5f, 1.5f, 1.5f};
    float expected[4] = {3.5f, 2.5f, 1.5f, 0.5f};
    float output[4] = {0.0f};

    simd_f32x4 a = load_f32_4(a_data);
    simd_f32x4 b = load_f32_4(b_data);
    simd_f32x4 c = sub_f32_4(a, b);
    store_f32_4(output, c);

    if (!compare_f32_arrays(expected, output, 4)) {
        result.fail("Float subtract operation produced incorrect results");
    }
}

void test_mul_f32_4(TestResult& result) {
    float a_data[4] = {2.0f, 3.0f, 4.0f, 5.0f};
    float b_data[4] = {0.5f, 2.0f, 0.25f, 1.0f};
    float expected[4] = {1.0f, 6.0f, 1.0f, 5.0f};
    float output[4] = {0.0f};

    simd_f32x4 a = load_f32_4(a_data);
    simd_f32x4 b = load_f32_4(b_data);
    simd_f32x4 c = mul_f32_4(a, b);
    store_f32_4(output, c);

    if (!compare_f32_arrays(expected, output, 4)) {
        result.fail("Float multiply operation produced incorrect results");
    }
}

void test_div_f32_4(TestResult& result) {
    float a_data[4] = {10.0f, 20.0f, 30.0f, 40.0f};
    float b_data[4] = {2.0f, 4.0f, 5.0f, 8.0f};
    float expected[4] = {5.0f, 5.0f, 6.0f, 5.0f};
    float output[4] = {0.0f};

    simd_f32x4 a = load_f32_4(a_data);
    simd_f32x4 b = load_f32_4(b_data);
    simd_f32x4 c = div_f32_4(a, b);
    store_f32_4(output, c);

    if (!compare_f32_arrays(expected, output, 4)) {
        result.fail("Float divide operation produced incorrect results");
    }
}

void test_sqrt_f32_4(TestResult& result) {
    float input[4] = {4.0f, 9.0f, 16.0f, 25.0f};
    float expected[4] = {2.0f, 3.0f, 4.0f, 5.0f};
    float output[4] = {0.0f};

    simd_f32x4 v = load_f32_4(input);
    simd_f32x4 result_v = sqrt_f32_4(v);
    store_f32_4(output, result_v);

    if (!compare_f32_arrays(expected, output, 4, 0.01f)) {
        result.fail("Float sqrt operation produced incorrect results");
    }
}

void test_min_f32_4(TestResult& result) {
    float a_data[4] = {1.0f, 5.0f, 3.0f, 7.0f};
    float b_data[4] = {2.0f, 4.0f, 6.0f, 1.0f};
    float expected[4] = {1.0f, 4.0f, 3.0f, 1.0f};
    float output[4] = {0.0f};

    simd_f32x4 a = load_f32_4(a_data);
    simd_f32x4 b = load_f32_4(b_data);
    simd_f32x4 c = min_f32_4(a, b);
    store_f32_4(output, c);

    if (!compare_f32_arrays(expected, output, 4)) {
        result.fail("Float min operation produced incorrect results");
    }
}

void test_max_f32_4(TestResult& result) {
    float a_data[4] = {1.0f, 5.0f, 3.0f, 7.0f};
    float b_data[4] = {2.0f, 4.0f, 6.0f, 1.0f};
    float expected[4] = {2.0f, 5.0f, 6.0f, 7.0f};
    float output[4] = {0.0f};

    simd_f32x4 a = load_f32_4(a_data);
    simd_f32x4 b = load_f32_4(b_data);
    simd_f32x4 c = max_f32_4(a, b);
    store_f32_4(output, c);

    if (!compare_f32_arrays(expected, output, 4)) {
        result.fail("Float max operation produced incorrect results");
    }
}

} // namespace simd_test
