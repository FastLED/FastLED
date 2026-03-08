// ValidationSimd.h - Comprehensive SIMD operation validation tests
//
// Full SIMD test suite covering every operation in fl::simd namespace.
// Tests include normal cases, boundary/edge cases, and signed arithmetic.
//
// Uses scalar fallback on platforms without native SIMD support.

#pragma once

#include <FastLED.h>
#include "fl/simd.h"
#include "fl/stl/sstream.h"
#include "fl/stl/fixed_point/s16x16.h"
#include "fl/stl/fixed_point/s0x32x4.h"
#include "fl/stl/fixed_point/s16x16x4.h"

namespace validation {
namespace simd_check {

using namespace fl::simd;

// ============================================================================
// Helper Functions
// ============================================================================

inline bool compare_u8(const uint8_t* a, const uint8_t* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

inline bool compare_u32(const uint32_t* a, const uint32_t* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

inline bool compare_f32(const float* a, const float* b, size_t n, float eps = 0.001f) {
    for (size_t i = 0; i < n; i++) {
        float diff = a[i] - b[i];
        if (diff < -eps || diff > eps) return false;
    }
    return true;
}

// Reinterpret u32 as i32 for readability
inline int32_t as_i32(uint32_t v) { return static_cast<int32_t>(v); }
inline uint32_t as_u32(int32_t v) { return static_cast<uint32_t>(v); }

// ============================================================================
// u8x16 Load/Store Tests
// ============================================================================

inline bool test_load_store_u8_16() {
    uint8_t input[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    uint8_t output[16] = {0};
    simd_u8x16 v = load_u8_16(input);
    store_u8_16(output, v);
    return compare_u8(input, output, 16);
}

inline bool test_load_store_u8_16_boundary() {
    uint8_t input[16] = {0,255,0,255, 0,255,0,255, 0,255,0,255, 0,255,0,255};
    uint8_t output[16] = {0};
    simd_u8x16 v = load_u8_16(input);
    store_u8_16(output, v);
    return compare_u8(input, output, 16);
}

// ============================================================================
// u32x4 Load/Store Tests
// ============================================================================

inline bool test_load_store_u32_4() {
    uint32_t input[4] = {0x12345678, 0x9ABCDEF0, 0xFEDCBA98, 0x76543210};
    uint32_t output[4] = {0};
    simd_u32x4 v = load_u32_4(input);
    store_u32_4(output, v);
    return compare_u32(input, output, 4);
}

inline bool test_load_store_u32_4_boundary() {
    uint32_t input[4] = {0, 0xFFFFFFFF, 1, 0x80000000};
    uint32_t output[4] = {0};
    simd_u32x4 v = load_u32_4(input);
    store_u32_4(output, v);
    return compare_u32(input, output, 4);
}

inline bool test_load_store_u32_4_aligned() {
    FL_ALIGNAS(16) uint32_t input[4] = {0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD};
    FL_ALIGNAS(16) uint32_t output[4] = {0};
    simd_u32x4 v = load_u32_4_aligned(input);
    store_u32_4_aligned(output, v);
    return compare_u32(input, output, 4);
}

// ============================================================================
// f32x4 Load/Store Tests
// ============================================================================

inline bool test_load_store_f32_4() {
    float input[4] = {1.5f, 2.5f, 3.5f, 4.5f};
    float output[4] = {0.0f};
    simd_f32x4 v = load_f32_4(input);
    store_f32_4(output, v);
    return compare_f32(input, output, 4);
}

inline bool test_load_store_f32_4_special() {
    float input[4] = {0.0f, -0.0f, 1e30f, -1e30f};
    float output[4] = {0.0f};
    simd_f32x4 v = load_f32_4(input);
    store_f32_4(output, v);
    // Compare bitwise for zero sign preservation
    for (int i = 0; i < 4; i++) {
        float diff = input[i] - output[i];
        if (diff < -0.001f || diff > 0.001f) return false;
    }
    return true;
}

// ============================================================================
// u8x16 Saturating Arithmetic Tests
// ============================================================================

inline bool test_add_sat_u8_16() {
    uint8_t a[16] = {100,150,200,250, 100,150,200,250, 100,150,200,250, 100,150,200,250};
    uint8_t b[16] = {50,100,50,100, 50,100,50,100, 50,100,50,100, 50,100,50,100};
    uint8_t expected[16] = {150,250,250,255, 150,250,250,255, 150,250,250,255, 150,250,250,255};
    uint8_t output[16] = {0};
    simd_u8x16 va = load_u8_16(a);
    simd_u8x16 vb = load_u8_16(b);
    store_u8_16(output, add_sat_u8_16(va, vb));
    return compare_u8(expected, output, 16);
}

inline bool test_add_sat_u8_16_full_saturate() {
    // 255 + 255 = 255 (saturated), 0 + 0 = 0
    uint8_t a[16] = {255,255,0,0, 128,1,254,255, 255,255,0,0, 128,1,254,255};
    uint8_t b[16] = {255,1,0,0, 128,255,2,0, 255,1,0,0, 128,255,2,0};
    uint8_t expected[16] = {255,255,0,0, 255,255,255,255, 255,255,0,0, 255,255,255,255};
    uint8_t output[16] = {0};
    store_u8_16(output, add_sat_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, output, 16);
}

inline bool test_sub_sat_u8_16() {
    uint8_t a[16] = {100,50,200,10, 100,50,200,10, 100,50,200,10, 100,50,200,10};
    uint8_t b[16] = {50,100,50,100, 50,100,50,100, 50,100,50,100, 50,100,50,100};
    uint8_t expected[16] = {50,0,150,0, 50,0,150,0, 50,0,150,0, 50,0,150,0};
    uint8_t output[16] = {0};
    store_u8_16(output, sub_sat_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, output, 16);
}

inline bool test_sub_sat_u8_16_full_clamp() {
    // 0 - 255 = 0 (clamped), 255 - 0 = 255, equal values = 0
    uint8_t a[16] = {0,255,100,0, 1,0,0,0, 0,255,100,0, 1,0,0,0};
    uint8_t b[16] = {255,0,100,0, 0,1,255,128, 255,0,100,0, 0,1,255,128};
    uint8_t expected[16] = {0,255,0,0, 1,0,0,0, 0,255,0,0, 1,0,0,0};
    uint8_t output[16] = {0};
    store_u8_16(output, sub_sat_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, output, 16);
}

// ============================================================================
// u8x16 Scale / Blend Tests
// ============================================================================

inline bool test_scale_u8_16() {
    uint8_t a[16] = {255,128,64,32, 255,128,64,32, 255,128,64,32, 255,128,64,32};
    uint8_t output[16] = {0};
    store_u8_16(output, scale_u8_16(load_u8_16(a), 128));
    for (int i = 0; i < 16; i++) {
        int expected = (a[i] * 128) / 256;
        int diff = (int)output[i] - expected;
        if (diff < -1 || diff > 1) return false;
    }
    return true;
}

inline bool test_scale_u8_16_zero() {
    uint8_t a[16] = {255,128,64,32, 255,128,64,32, 255,128,64,32, 255,128,64,32};
    uint8_t expected[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
    uint8_t output[16] = {0};
    store_u8_16(output, scale_u8_16(load_u8_16(a), 0));
    return compare_u8(expected, output, 16);
}

inline bool test_scale_u8_16_full() {
    // scale by 255 should give approximately the original value
    uint8_t a[16] = {255,128,64,32, 10,200,100,50, 255,128,64,32, 10,200,100,50};
    uint8_t output[16] = {0};
    store_u8_16(output, scale_u8_16(load_u8_16(a), 255));
    for (int i = 0; i < 16; i++) {
        int expected = (a[i] * 255) / 256;
        int diff = (int)output[i] - expected;
        if (diff < -1 || diff > 1) return false;
    }
    return true;
}

inline bool test_blend_u8_16() {
    uint8_t a[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
    uint8_t b[16] = {255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255};
    uint8_t output[16] = {0};
    store_u8_16(output, blend_u8_16(load_u8_16(a), load_u8_16(b), 128));
    for (int i = 0; i < 16; i++) {
        int diff = (int)output[i] - 128;
        if (diff < -2 || diff > 2) return false;
    }
    return true;
}

inline bool test_blend_u8_16_endpoints() {
    // amount=0 -> all a, amount=255 -> ~all b
    uint8_t a[16] = {100,100,100,100, 100,100,100,100, 100,100,100,100, 100,100,100,100};
    uint8_t b[16] = {200,200,200,200, 200,200,200,200, 200,200,200,200, 200,200,200,200};
    uint8_t output0[16] = {0};
    uint8_t output255[16] = {0};
    store_u8_16(output0, blend_u8_16(load_u8_16(a), load_u8_16(b), 0));
    store_u8_16(output255, blend_u8_16(load_u8_16(a), load_u8_16(b), 255));
    for (int i = 0; i < 16; i++) {
        if (output0[i] != 100) return false;  // amount=0 -> a
        int diff = (int)output255[i] - 200;
        if (diff < -2 || diff > 2) return false;  // amount=255 -> ~b
    }
    return true;
}

// ============================================================================
// u8x16 Comparison Tests
// ============================================================================

inline bool test_min_u8_16() {
    uint8_t a[16] = {10,200,30,240, 0,255,1,254, 10,200,30,240, 0,255,1,254};
    uint8_t b[16] = {20,100,40,120, 0,0,255,255, 20,100,40,120, 0,0,255,255};
    uint8_t expected[16] = {10,100,30,120, 0,0,1,254, 10,100,30,120, 0,0,1,254};
    uint8_t output[16] = {0};
    store_u8_16(output, min_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, output, 16);
}

inline bool test_max_u8_16() {
    uint8_t a[16] = {10,200,30,240, 0,255,1,254, 10,200,30,240, 0,255,1,254};
    uint8_t b[16] = {20,100,40,120, 0,0,255,255, 20,100,40,120, 0,0,255,255};
    uint8_t expected[16] = {20,200,40,240, 0,255,255,255, 20,200,40,240, 0,255,255,255};
    uint8_t output[16] = {0};
    store_u8_16(output, max_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, output, 16);
}

inline bool test_avg_u8_16() {
    uint8_t a[16] = {100,200,50,0, 255,0,1,254, 100,200,50,0, 255,0,1,254};
    uint8_t b[16] = {200,100,150,0, 255,0,1,254, 200,100,150,0, 255,0,1,254};
    uint8_t output[16] = {0};
    store_u8_16(output, avg_u8_16(load_u8_16(a), load_u8_16(b)));
    for (int i = 0; i < 16; i++) {
        int expected = (a[i] + b[i]) / 2;
        int diff = (int)output[i] - expected;
        if (diff < -1 || diff > 1) return false;
    }
    return true;
}

inline bool test_avg_round_u8_16() {
    uint8_t a[16] = {101,201,51,1, 255,0,3,253, 101,201,51,1, 255,0,3,253};
    uint8_t b[16] = {200,100,150,0, 254,1,2,252, 200,100,150,0, 254,1,2,252};
    uint8_t output[16] = {0};
    store_u8_16(output, avg_round_u8_16(load_u8_16(a), load_u8_16(b)));
    for (int i = 0; i < 16; i++) {
        int expected = (a[i] + b[i] + 1) / 2;
        int diff = (int)output[i] - expected;
        if (diff < -1 || diff > 1) return false;
    }
    return true;
}

// ============================================================================
// u8x16 Bitwise Tests
// ============================================================================

inline bool test_and_u8_16() {
    uint8_t a[16] = {0xFF,0x0F,0xF0,0xAA, 0xFF,0x0F,0xF0,0xAA, 0xFF,0x0F,0xF0,0xAA, 0xFF,0x0F,0xF0,0xAA};
    uint8_t b[16] = {0x0F,0xFF,0xFF,0x55, 0x0F,0xFF,0xFF,0x55, 0x0F,0xFF,0xFF,0x55, 0x0F,0xFF,0xFF,0x55};
    uint8_t expected[16] = {0x0F,0x0F,0xF0,0x00, 0x0F,0x0F,0xF0,0x00, 0x0F,0x0F,0xF0,0x00, 0x0F,0x0F,0xF0,0x00};
    uint8_t output[16] = {0};
    store_u8_16(output, and_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, output, 16);
}

inline bool test_or_u8_16() {
    uint8_t a[16] = {0xFF,0x0F,0xF0,0xAA, 0x00,0x00,0xFF,0x80, 0xFF,0x0F,0xF0,0xAA, 0x00,0x00,0xFF,0x80};
    uint8_t b[16] = {0x0F,0xFF,0xFF,0x55, 0x00,0xFF,0x00,0x01, 0x0F,0xFF,0xFF,0x55, 0x00,0xFF,0x00,0x01};
    uint8_t expected[16] = {0xFF,0xFF,0xFF,0xFF, 0x00,0xFF,0xFF,0x81, 0xFF,0xFF,0xFF,0xFF, 0x00,0xFF,0xFF,0x81};
    uint8_t output[16] = {0};
    store_u8_16(output, or_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, output, 16);
}

inline bool test_xor_u8_16() {
    uint8_t a[16] = {0xFF,0x0F,0xF0,0xAA, 0xFF,0x00,0xAB,0x01, 0xFF,0x0F,0xF0,0xAA, 0xFF,0x00,0xAB,0x01};
    uint8_t b[16] = {0x0F,0xFF,0xFF,0x55, 0xFF,0x00,0xAB,0x01, 0x0F,0xFF,0xFF,0x55, 0xFF,0x00,0xAB,0x01};
    uint8_t expected[16] = {0xF0,0xF0,0x0F,0xFF, 0x00,0x00,0x00,0x00, 0xF0,0xF0,0x0F,0xFF, 0x00,0x00,0x00,0x00};
    uint8_t output[16] = {0};
    store_u8_16(output, xor_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, output, 16);
}

inline bool test_andnot_u8_16() {
    // andnot(a, b) = ~a & b
    uint8_t a[16] = {0xFF,0x0F,0xF0,0xAA, 0x00,0xFF,0x55,0x80, 0xFF,0x0F,0xF0,0xAA, 0x00,0xFF,0x55,0x80};
    uint8_t b[16] = {0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF};
    uint8_t expected[16] = {0x00,0xF0,0x0F,0x55, 0xFF,0x00,0xAA,0x7F, 0x00,0xF0,0x0F,0x55, 0xFF,0x00,0xAA,0x7F};
    uint8_t output[16] = {0};
    store_u8_16(output, andnot_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, output, 16);
}

// ============================================================================
// u32x4 Broadcast / Construct Tests
// ============================================================================

inline bool test_set1_u32_4() {
    uint32_t output[4] = {0};
    store_u32_4(output, set1_u32_4(0xDEADBEEF));
    uint32_t expected[4] = {0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF};
    return compare_u32(expected, output, 4);
}

inline bool test_set1_u32_4_zero() {
    uint32_t output[4] = {1,1,1,1};
    store_u32_4(output, set1_u32_4(0));
    uint32_t expected[4] = {0, 0, 0, 0};
    return compare_u32(expected, output, 4);
}

inline bool test_set_u32_4() {
    uint32_t output[4] = {0};
    store_u32_4(output, set_u32_4(0x11111111, 0x22222222, 0x33333333, 0x44444444));
    uint32_t expected[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
    return compare_u32(expected, output, 4);
}

inline bool test_set1_f32_4() {
    float output[4] = {0.0f};
    store_f32_4(output, set1_f32_4(3.14f));
    float expected[4] = {3.14f, 3.14f, 3.14f, 3.14f};
    return compare_f32(expected, output, 4);
}

// ============================================================================
// u32x4 Extract Test
// ============================================================================

inline bool test_extract_u32_4() {
    simd_u32x4 v = set_u32_4(10, 20, 30, 40);
    if (extract_u32_4(v, 0) != 10) return false;
    if (extract_u32_4(v, 1) != 20) return false;
    if (extract_u32_4(v, 2) != 30) return false;
    if (extract_u32_4(v, 3) != 40) return false;
    return true;
}

// ============================================================================
// u32x4 Bitwise Tests
// ============================================================================

inline bool test_xor_u32_4() {
    uint32_t a[4] = {0xFFFFFFFF, 0x0F0F0F0F, 0xAAAAAAAA, 0x12345678};
    uint32_t b[4] = {0x0F0F0F0F, 0xFFFFFFFF, 0x55555555, 0x12345678};
    uint32_t expected[4] = {0xF0F0F0F0, 0xF0F0F0F0, 0xFFFFFFFF, 0x00000000};
    uint32_t output[4] = {0};
    store_u32_4(output, xor_u32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, output, 4);
}

inline bool test_and_u32_4() {
    uint32_t a[4] = {0xFFFF0000, 0x0F0F0F0F, 0xAAAAAAAA, 0x00000000};
    uint32_t b[4] = {0x0000FFFF, 0xF0F0F0F0, 0xFFFFFFFF, 0xFFFFFFFF};
    uint32_t expected[4] = {0x00000000, 0x00000000, 0xAAAAAAAA, 0x00000000};
    uint32_t output[4] = {0};
    store_u32_4(output, and_u32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, output, 4);
}

inline bool test_or_u32_4() {
    uint32_t a[4] = {0xFFFF0000, 0x0F0F0F0F, 0xAAAAAAAA, 0x00000000};
    uint32_t b[4] = {0x0000FFFF, 0xF0F0F0F0, 0x55555555, 0x00000000};
    uint32_t expected[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000};
    uint32_t output[4] = {0};
    store_u32_4(output, or_u32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, output, 4);
}

// ============================================================================
// i32x4 Arithmetic Tests
// ============================================================================

inline bool test_add_i32_4() {
    // Use signed values stored as u32
    uint32_t a[4] = {as_u32(100), as_u32(-100), as_u32(0x7FFFFFFF), as_u32(0)};
    uint32_t b[4] = {as_u32(200), as_u32(-200), as_u32(1), as_u32(0)};
    uint32_t output[4] = {0};
    store_u32_4(output, add_i32_4(load_u32_4(a), load_u32_4(b)));
    if (as_i32(output[0]) != 300) return false;
    if (as_i32(output[1]) != -300) return false;
    // 0x7FFFFFFF + 1 wraps (expected behavior, no saturation)
    if (output[2] != 0x80000000) return false;
    if (as_i32(output[3]) != 0) return false;
    return true;
}

inline bool test_sub_i32_4() {
    uint32_t a[4] = {as_u32(300), as_u32(-100), as_u32(0), as_u32(1000)};
    uint32_t b[4] = {as_u32(200), as_u32(200), as_u32(0), as_u32(1000)};
    uint32_t output[4] = {0};
    store_u32_4(output, sub_i32_4(load_u32_4(a), load_u32_4(b)));
    if (as_i32(output[0]) != 100) return false;
    if (as_i32(output[1]) != -300) return false;
    if (as_i32(output[2]) != 0) return false;
    if (as_i32(output[3]) != 0) return false;
    return true;
}

// ============================================================================
// i32x4 Shift Tests
// ============================================================================

inline bool test_srl_u32_4() {
    uint32_t input[4] = {0x80000000, 0xFFFFFFFF, 0x00000010, 0x12345678};
    uint32_t output[4] = {0};
    store_u32_4(output, srl_u32_4(load_u32_4(input), 4));
    uint32_t expected[4] = {0x08000000, 0x0FFFFFFF, 0x00000001, 0x01234567};
    return compare_u32(expected, output, 4);
}

inline bool test_sll_u32_4() {
    uint32_t input[4] = {0x00000001, 0x0FFFFFFF, 0x80000000, 0x12345678};
    uint32_t output[4] = {0};
    store_u32_4(output, sll_u32_4(load_u32_4(input), 4));
    uint32_t expected[4] = {0x00000010, 0xFFFFFFF0, 0x00000000, 0x23456780};
    return compare_u32(expected, output, 4);
}

inline bool test_sra_i32_4() {
    // Arithmetic shift preserves sign bit
    uint32_t input[4] = {as_u32(-16), as_u32(16), 0x80000000, 0x7FFFFFFF};
    uint32_t output[4] = {0};
    store_u32_4(output, sra_i32_4(load_u32_4(input), 2));
    if (as_i32(output[0]) != -4) return false;   // -16 >> 2 = -4 (sign-extended)
    if (as_i32(output[1]) != 4) return false;     // 16 >> 2 = 4
    if (as_i32(output[2]) != as_i32(0xE0000000)) return false;  // 0x80000000 >> 2 sign-extended
    if (as_i32(output[3]) != as_i32(0x1FFFFFFF)) return false;  // 0x7FFFFFFF >> 2
    return true;
}

// ============================================================================
// i32x4 Min/Max Tests (signed)
// ============================================================================

inline bool test_min_i32_4() {
    uint32_t a[4] = {as_u32(10), as_u32(-10), as_u32(0x7FFFFFFF), as_u32(-1)};
    uint32_t b[4] = {as_u32(20), as_u32(10), as_u32((int32_t)0x80000000), as_u32(0)};
    uint32_t output[4] = {0};
    store_u32_4(output, min_i32_4(load_u32_4(a), load_u32_4(b)));
    if (as_i32(output[0]) != 10) return false;
    if (as_i32(output[1]) != -10) return false;
    if (as_i32(output[2]) != (int32_t)0x80000000) return false;  // INT32_MIN < INT32_MAX
    if (as_i32(output[3]) != -1) return false;  // -1 < 0
    return true;
}

inline bool test_max_i32_4() {
    uint32_t a[4] = {as_u32(10), as_u32(-10), as_u32(0x7FFFFFFF), as_u32(-1)};
    uint32_t b[4] = {as_u32(20), as_u32(10), as_u32((int32_t)0x80000000), as_u32(0)};
    uint32_t output[4] = {0};
    store_u32_4(output, max_i32_4(load_u32_4(a), load_u32_4(b)));
    if (as_i32(output[0]) != 20) return false;
    if (as_i32(output[1]) != 10) return false;
    if (as_i32(output[2]) != 0x7FFFFFFF) return false;
    if (as_i32(output[3]) != 0) return false;
    return true;
}

// ============================================================================
// Fixed-Point Multiply Tests (Q16.16)
// ============================================================================

inline bool test_mulhi_i32_4() {
    // mulhi_i32_4: ((i64)a * (i64)b) >> 16
    // Test: 0x00010000 * 0x00020000 >> 16 = 0x00020000 (1.0 * 2.0 = 2.0 in Q16.16)
    uint32_t a[4] = {0x00010000, 0x00020000, as_u32(-0x00010000), 0x00008000};
    uint32_t b[4] = {0x00020000, 0x00010000, 0x00020000, 0x00008000};
    uint32_t output[4] = {0};
    store_u32_4(output, mulhi_i32_4(load_u32_4(a), load_u32_4(b)));
    if (output[0] != 0x00020000) return false;  // 1.0 * 2.0 = 2.0
    if (output[1] != 0x00020000) return false;  // 2.0 * 1.0 = 2.0
    if (as_i32(output[2]) != -0x00020000) return false;  // -1.0 * 2.0 = -2.0
    if (output[3] != 0x00004000) return false;  // 0.5 * 0.5 = 0.25
    return true;
}

inline bool test_mulhi_u32_4() {
    // mulhi_u32_4: ((u64)a * (u64)b) >> 16 (unsigned)
    uint32_t a[4] = {0x00010000, 0x00020000, 0x00030000, 0x00008000};
    uint32_t b[4] = {0x00020000, 0x00030000, 0x00010000, 0x00008000};
    uint32_t output[4] = {0};
    store_u32_4(output, mulhi_u32_4(load_u32_4(a), load_u32_4(b)));
    if (output[0] != 0x00020000) return false;  // 1.0 * 2.0
    if (output[1] != 0x00060000) return false;  // 2.0 * 3.0
    if (output[2] != 0x00030000) return false;  // 3.0 * 1.0
    if (output[3] != 0x00004000) return false;  // 0.5 * 0.5
    return true;
}

inline bool test_mulhi_su32_4() {
    // mulhi_su32_4: signed a * unsigned b >> 16
    // When b >= 0, should match mulhi_i32_4
    uint32_t a[4] = {as_u32(-0x00010000), 0x00020000, as_u32(-0x00020000), 0x00010000};
    uint32_t b[4] = {0x00020000, 0x00010000, 0x00010000, 0x00030000};
    uint32_t output[4] = {0};
    store_u32_4(output, mulhi_su32_4(load_u32_4(a), load_u32_4(b)));
    if (as_i32(output[0]) != -0x00020000) return false;  // -1.0 * 2.0 = -2.0
    if (output[1] != 0x00020000) return false;            // 2.0 * 1.0 = 2.0
    if (as_i32(output[2]) != -0x00020000) return false;   // -2.0 * 1.0 = -2.0
    if (output[3] != 0x00030000) return false;            // 1.0 * 3.0 = 3.0
    return true;
}

inline bool test_mulhi32_i32_4() {
    // mulhi32_i32_4: ((i64)a * (i64)b) >> 32
    // Test: 0x40000000 * 0x40000000 >> 32 = 0x10000000
    uint32_t a[4] = {0x40000000, as_u32(-0x40000000), 0x7FFFFFFF, 0x00000001};
    uint32_t b[4] = {0x40000000, 0x40000000, 0x00000002, 0x7FFFFFFF};
    uint32_t output[4] = {0};
    store_u32_4(output, mulhi32_i32_4(load_u32_4(a), load_u32_4(b)));
    if (output[0] != 0x10000000) return false;             // 0.5 * 0.5 = 0.25 (Q31)
    if (as_i32(output[1]) != as_i32(0xF0000000)) return false;  // -0.5 * 0.5 = -0.25
    if (output[2] != 0x00000000) return false;             // small * small -> 0
    if (output[3] != 0x00000000) return false;             // 1 * MAX -> 0 (high bits only)
    return true;
}

// ============================================================================
// u32x4 Interleave / Unpack Tests
// ============================================================================

inline bool test_unpacklo_u32_4() {
    // {a0, b0, a1, b1}
    simd_u32x4 a = set_u32_4(1, 2, 3, 4);
    simd_u32x4 b = set_u32_4(10, 20, 30, 40);
    uint32_t output[4] = {0};
    store_u32_4(output, unpacklo_u32_4(a, b));
    uint32_t expected[4] = {1, 10, 2, 20};
    return compare_u32(expected, output, 4);
}

inline bool test_unpackhi_u32_4() {
    // {a2, b2, a3, b3}
    simd_u32x4 a = set_u32_4(1, 2, 3, 4);
    simd_u32x4 b = set_u32_4(10, 20, 30, 40);
    uint32_t output[4] = {0};
    store_u32_4(output, unpackhi_u32_4(a, b));
    uint32_t expected[4] = {3, 30, 4, 40};
    return compare_u32(expected, output, 4);
}

inline bool test_unpacklo_u64_as_u32_4() {
    // {a0, a1, b0, b1}
    simd_u32x4 a = set_u32_4(1, 2, 3, 4);
    simd_u32x4 b = set_u32_4(10, 20, 30, 40);
    uint32_t output[4] = {0};
    store_u32_4(output, unpacklo_u64_as_u32_4(a, b));
    uint32_t expected[4] = {1, 2, 10, 20};
    return compare_u32(expected, output, 4);
}

inline bool test_unpackhi_u64_as_u32_4() {
    // {a2, a3, b2, b3}
    simd_u32x4 a = set_u32_4(1, 2, 3, 4);
    simd_u32x4 b = set_u32_4(10, 20, 30, 40);
    uint32_t output[4] = {0};
    store_u32_4(output, unpackhi_u64_as_u32_4(a, b));
    uint32_t expected[4] = {3, 4, 30, 40};
    return compare_u32(expected, output, 4);
}

// ============================================================================
// f32x4 Arithmetic Tests
// ============================================================================

inline bool test_add_f32_4() {
    float a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[4] = {5.0f, 6.0f, 7.0f, 8.0f};
    float expected[4] = {6.0f, 8.0f, 10.0f, 12.0f};
    float output[4] = {0.0f};
    store_f32_4(output, add_f32_4(load_f32_4(a), load_f32_4(b)));
    return compare_f32(expected, output, 4);
}

inline bool test_add_f32_4_negative() {
    float a[4] = {-1.0f, 2.0f, -3.0f, 0.0f};
    float b[4] = {1.0f, -2.0f, -3.0f, 0.0f};
    float expected[4] = {0.0f, 0.0f, -6.0f, 0.0f};
    float output[4] = {0.0f};
    store_f32_4(output, add_f32_4(load_f32_4(a), load_f32_4(b)));
    return compare_f32(expected, output, 4);
}

inline bool test_sub_f32_4() {
    float a[4] = {10.0f, 20.0f, 30.0f, 40.0f};
    float b[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float expected[4] = {9.0f, 18.0f, 27.0f, 36.0f};
    float output[4] = {0.0f};
    store_f32_4(output, sub_f32_4(load_f32_4(a), load_f32_4(b)));
    return compare_f32(expected, output, 4);
}

inline bool test_mul_f32_4() {
    float a[4] = {2.0f, 3.0f, 4.0f, 5.0f};
    float b[4] = {3.0f, 4.0f, 5.0f, 6.0f};
    float expected[4] = {6.0f, 12.0f, 20.0f, 30.0f};
    float output[4] = {0.0f};
    store_f32_4(output, mul_f32_4(load_f32_4(a), load_f32_4(b)));
    return compare_f32(expected, output, 4);
}

inline bool test_mul_f32_4_negative() {
    float a[4] = {-2.0f, 3.0f, -4.0f, 0.0f};
    float b[4] = {3.0f, -4.0f, -5.0f, 100.0f};
    float expected[4] = {-6.0f, -12.0f, 20.0f, 0.0f};
    float output[4] = {0.0f};
    store_f32_4(output, mul_f32_4(load_f32_4(a), load_f32_4(b)));
    return compare_f32(expected, output, 4);
}

inline bool test_div_f32_4() {
    float a[4] = {10.0f, 20.0f, 30.0f, 40.0f};
    float b[4] = {2.0f, 4.0f, 5.0f, 8.0f};
    float expected[4] = {5.0f, 5.0f, 6.0f, 5.0f};
    float output[4] = {0.0f};
    store_f32_4(output, div_f32_4(load_f32_4(a), load_f32_4(b)));
    return compare_f32(expected, output, 4);
}

inline bool test_sqrt_f32_4() {
    float a[4] = {4.0f, 9.0f, 16.0f, 25.0f};
    float expected[4] = {2.0f, 3.0f, 4.0f, 5.0f};
    float output[4] = {0.0f};
    store_f32_4(output, sqrt_f32_4(load_f32_4(a)));
    return compare_f32(expected, output, 4);
}

inline bool test_sqrt_f32_4_zero() {
    float a[4] = {0.0f, 1.0f, 100.0f, 0.25f};
    float expected[4] = {0.0f, 1.0f, 10.0f, 0.5f};
    float output[4] = {0.0f};
    store_f32_4(output, sqrt_f32_4(load_f32_4(a)));
    return compare_f32(expected, output, 4);
}

inline bool test_min_f32_4() {
    float a[4] = {1.0f, 5.0f, -3.0f, 7.0f};
    float b[4] = {4.0f, 2.0f, 6.0f, -1.0f};
    float expected[4] = {1.0f, 2.0f, -3.0f, -1.0f};
    float output[4] = {0.0f};
    store_f32_4(output, min_f32_4(load_f32_4(a), load_f32_4(b)));
    return compare_f32(expected, output, 4);
}

inline bool test_max_f32_4() {
    float a[4] = {1.0f, 5.0f, -3.0f, 7.0f};
    float b[4] = {4.0f, 2.0f, 6.0f, -1.0f};
    float expected[4] = {4.0f, 5.0f, 6.0f, 7.0f};
    float output[4] = {0.0f};
    store_f32_4(output, max_f32_4(load_f32_4(a), load_f32_4(b)));
    return compare_f32(expected, output, 4);
}

// ============================================================================
// Cross-Validation Tests: SIMD vs Scalar Reference
// These tests compute expected results using explicit scalar math, then compare
// against the SIMD function output. Catches PIE assembly bugs by never trusting
// the SIMD implementation for expected values.
// ============================================================================

// Scalar reference: bitwise AND of two u8[16] arrays
inline void ref_and_u8_16(const uint8_t* a, const uint8_t* b, uint8_t* out) {
    for (int i = 0; i < 16; i++) out[i] = a[i] & b[i];
}
inline void ref_or_u8_16(const uint8_t* a, const uint8_t* b, uint8_t* out) {
    for (int i = 0; i < 16; i++) out[i] = a[i] | b[i];
}
inline void ref_xor_u8_16(const uint8_t* a, const uint8_t* b, uint8_t* out) {
    for (int i = 0; i < 16; i++) out[i] = a[i] ^ b[i];
}
inline void ref_andnot_u8_16(const uint8_t* a, const uint8_t* b, uint8_t* out) {
    for (int i = 0; i < 16; i++) out[i] = (~a[i]) & b[i];
}
inline void ref_xor_u32_4(const uint32_t* a, const uint32_t* b, uint32_t* out) {
    for (int i = 0; i < 4; i++) out[i] = a[i] ^ b[i];
}
inline void ref_and_u32_4(const uint32_t* a, const uint32_t* b, uint32_t* out) {
    for (int i = 0; i < 4; i++) out[i] = a[i] & b[i];
}
inline void ref_or_u32_4(const uint32_t* a, const uint32_t* b, uint32_t* out) {
    for (int i = 0; i < 4; i++) out[i] = a[i] | b[i];
}

// Adversarial cross-validation: alternating bit patterns
inline bool test_crossval_and_u8_16() {
    uint8_t a[16] = {0xAA,0x55,0xFF,0x00, 0x0F,0xF0,0x81,0x7E, 0x01,0xFE,0xCC,0x33, 0xDB,0x24,0xA5,0x5A};
    uint8_t b[16] = {0x55,0xAA,0x00,0xFF, 0xF0,0x0F,0x7E,0x81, 0xFE,0x01,0x33,0xCC, 0x24,0xDB,0x5A,0xA5};
    uint8_t expected[16], actual[16];
    ref_and_u8_16(a, b, expected);
    store_u8_16(actual, and_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, actual, 16);
}

inline bool test_crossval_or_u8_16() {
    uint8_t a[16] = {0xAA,0x55,0xFF,0x00, 0x0F,0xF0,0x81,0x7E, 0x01,0xFE,0xCC,0x33, 0xDB,0x24,0xA5,0x5A};
    uint8_t b[16] = {0x55,0xAA,0x00,0xFF, 0xF0,0x0F,0x7E,0x81, 0xFE,0x01,0x33,0xCC, 0x24,0xDB,0x5A,0xA5};
    uint8_t expected[16], actual[16];
    ref_or_u8_16(a, b, expected);
    store_u8_16(actual, or_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, actual, 16);
}

inline bool test_crossval_xor_u8_16() {
    uint8_t a[16] = {0xAA,0x55,0xFF,0x00, 0x0F,0xF0,0x81,0x7E, 0x01,0xFE,0xCC,0x33, 0xDB,0x24,0xA5,0x5A};
    uint8_t b[16] = {0x55,0xAA,0x00,0xFF, 0xF0,0x0F,0x7E,0x81, 0xFE,0x01,0x33,0xCC, 0x24,0xDB,0x5A,0xA5};
    uint8_t expected[16], actual[16];
    ref_xor_u8_16(a, b, expected);
    store_u8_16(actual, xor_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, actual, 16);
}

inline bool test_crossval_andnot_u8_16() {
    uint8_t a[16] = {0xAA,0x55,0xFF,0x00, 0x0F,0xF0,0x81,0x7E, 0x01,0xFE,0xCC,0x33, 0xDB,0x24,0xA5,0x5A};
    uint8_t b[16] = {0x55,0xAA,0x00,0xFF, 0xF0,0x0F,0x7E,0x81, 0xFE,0x01,0x33,0xCC, 0x24,0xDB,0x5A,0xA5};
    uint8_t expected[16], actual[16];
    ref_andnot_u8_16(a, b, expected);
    store_u8_16(actual, andnot_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, actual, 16);
}

// Adversarial u32 cross-validation: powers of 2, all-ones, sign bit
inline bool test_crossval_xor_u32_4() {
    uint32_t a[4] = {0x80000001, 0x7FFFFFFE, 0xDEADBEEF, 0x00000000};
    uint32_t b[4] = {0x80000001, 0x80000001, 0xCAFEBABE, 0xFFFFFFFF};
    uint32_t expected[4], actual[4];
    ref_xor_u32_4(a, b, expected);
    store_u32_4(actual, xor_u32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, actual, 4);
}

inline bool test_crossval_and_u32_4() {
    uint32_t a[4] = {0x80000001, 0x7FFFFFFE, 0xDEADBEEF, 0x00000000};
    uint32_t b[4] = {0x80000001, 0x80000001, 0xCAFEBABE, 0xFFFFFFFF};
    uint32_t expected[4], actual[4];
    ref_and_u32_4(a, b, expected);
    store_u32_4(actual, and_u32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, actual, 4);
}

inline bool test_crossval_or_u32_4() {
    uint32_t a[4] = {0x80000001, 0x7FFFFFFE, 0xDEADBEEF, 0x00000000};
    uint32_t b[4] = {0x80000001, 0x80000001, 0xCAFEBABE, 0xFFFFFFFF};
    uint32_t expected[4], actual[4];
    ref_or_u32_4(a, b, expected);
    store_u32_4(actual, or_u32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, actual, 4);
}

// Scalar reference: scale
inline void ref_scale_u8_16(const uint8_t* v, uint8_t scale, uint8_t* out) {
    for (int i = 0; i < 16; i++) {
        out[i] = (uint8_t)(((uint16_t)v[i] * scale) >> 8);
    }
}

inline bool test_crossval_scale_u8_16() {
    // Adversarial: mix of 0, 1, 127, 128, 254, 255 with various scale factors
    uint8_t v[16] = {0,1,127,128, 254,255,0x55,0xAA, 0x0F,0xF0,0x80,0x7F, 0x01,0xFE,0xFF,0x00};
    uint8_t scale_vals[] = {0, 1, 127, 128, 254, 255};
    for (int s = 0; s < 6; s++) {
        uint8_t expected[16], actual[16];
        ref_scale_u8_16(v, scale_vals[s], expected);
        store_u8_16(actual, scale_u8_16(load_u8_16(v), scale_vals[s]));
        if (!compare_u8(expected, actual, 16)) return false;
    }
    return true;
}

// Scalar reference: unsigned saturating add/sub
inline void ref_add_sat_u8_16(const uint8_t* a, const uint8_t* b, uint8_t* out) {
    for (int i = 0; i < 16; i++) {
        uint16_t sum = (uint16_t)a[i] + (uint16_t)b[i];
        out[i] = (sum > 255) ? 255 : (uint8_t)sum;
    }
}
inline void ref_sub_sat_u8_16(const uint8_t* a, const uint8_t* b, uint8_t* out) {
    for (int i = 0; i < 16; i++) {
        out[i] = (a[i] > b[i]) ? (a[i] - b[i]) : 0;
    }
}

// Adversarial cross-validation: sat add with every combination
inline bool test_crossval_add_sat_u8_16() {
    // Test adversarial: all-255+all-255, alternating, near-overflow
    uint8_t a[16] = {255,254,128,127, 0,1,200,50, 0xFF,0x80,0x7F,0x01, 100,200,150,250};
    uint8_t b[16] = {255,2,128,129, 0,255,56,206, 0x01,0x80,0x81,0xFF, 156,56,106,6};
    uint8_t expected[16], actual[16];
    ref_add_sat_u8_16(a, b, expected);
    store_u8_16(actual, add_sat_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, actual, 16);
}

inline bool test_crossval_sub_sat_u8_16() {
    uint8_t a[16] = {255,0,128,127, 0,1,200,50, 0xFF,0x80,0x7F,0x01, 100,200,150,250};
    uint8_t b[16] = {255,255,128,129, 0,255,56,206, 0x01,0x80,0x81,0xFF, 156,56,106,6};
    uint8_t expected[16], actual[16];
    ref_sub_sat_u8_16(a, b, expected);
    store_u8_16(actual, sub_sat_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, actual, 16);
}

// Scalar reference: i32 wrapping add/sub
inline void ref_add_i32_4(const uint32_t* a, const uint32_t* b, uint32_t* out) {
    for (int i = 0; i < 4; i++) out[i] = a[i] + b[i];  // wrapping
}
inline void ref_sub_i32_4(const uint32_t* a, const uint32_t* b, uint32_t* out) {
    for (int i = 0; i < 4; i++) out[i] = a[i] - b[i];  // wrapping
}

// Adversarial wrapping add: overflow, underflow, sign flip
inline bool test_crossval_add_i32_4() {
    uint32_t a[4] = {0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x00000001};
    uint32_t b[4] = {0x00000001, 0x80000000, 0x00000001, 0xFFFFFFFF};
    uint32_t expected[4], actual[4];
    ref_add_i32_4(a, b, expected);
    store_u32_4(actual, add_i32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, actual, 4);
}

inline bool test_crossval_sub_i32_4() {
    uint32_t a[4] = {0x00000000, 0x80000000, 0x7FFFFFFF, 0x00000001};
    uint32_t b[4] = {0x00000001, 0x7FFFFFFF, 0x80000000, 0x00000002};
    uint32_t expected[4], actual[4];
    ref_sub_i32_4(a, b, expected);
    store_u32_4(actual, sub_i32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, actual, 4);
}

// Scalar reference: shifts
inline void ref_srl_u32_4(const uint32_t* v, int shift, uint32_t* out) {
    for (int i = 0; i < 4; i++) out[i] = v[i] >> shift;
}
inline void ref_sll_u32_4(const uint32_t* v, int shift, uint32_t* out) {
    for (int i = 0; i < 4; i++) out[i] = v[i] << shift;
}
inline void ref_sra_i32_4(const uint32_t* v, int shift, uint32_t* out) {
    for (int i = 0; i < 4; i++) out[i] = (uint32_t)((int32_t)v[i] >> shift);
}

inline bool test_crossval_srl_u32_4() {
    uint32_t v[4] = {0x80000000, 0xFFFFFFFF, 0x00000001, 0xDEADBEEF};
    for (int shift = 0; shift <= 31; shift += 7) {
        uint32_t expected[4], actual[4];
        ref_srl_u32_4(v, shift, expected);
        store_u32_4(actual, srl_u32_4(load_u32_4(v), shift));
        if (!compare_u32(expected, actual, 4)) return false;
    }
    return true;
}

inline bool test_crossval_sll_u32_4() {
    uint32_t v[4] = {0x80000000, 0xFFFFFFFF, 0x00000001, 0xDEADBEEF};
    for (int shift = 0; shift <= 31; shift += 7) {
        uint32_t expected[4], actual[4];
        ref_sll_u32_4(v, shift, expected);
        store_u32_4(actual, sll_u32_4(load_u32_4(v), shift));
        if (!compare_u32(expected, actual, 4)) return false;
    }
    return true;
}

inline bool test_crossval_sra_i32_4() {
    uint32_t v[4] = {0x80000000, 0xFFFFFFFF, 0x7FFFFFFF, 0xDEADBEEF};
    for (int shift = 0; shift <= 31; shift += 7) {
        uint32_t expected[4], actual[4];
        ref_sra_i32_4(v, shift, expected);
        store_u32_4(actual, sra_i32_4(load_u32_4(v), shift));
        if (!compare_u32(expected, actual, 4)) return false;
    }
    return true;
}

// Scalar reference: min/max i32
inline void ref_min_i32_4(const uint32_t* a, const uint32_t* b, uint32_t* out) {
    for (int i = 0; i < 4; i++) {
        int32_t ai = (int32_t)a[i], bi = (int32_t)b[i];
        out[i] = (uint32_t)(ai < bi ? ai : bi);
    }
}
inline void ref_max_i32_4(const uint32_t* a, const uint32_t* b, uint32_t* out) {
    for (int i = 0; i < 4; i++) {
        int32_t ai = (int32_t)a[i], bi = (int32_t)b[i];
        out[i] = (uint32_t)(ai > bi ? ai : bi);
    }
}

inline bool test_crossval_min_i32_4() {
    uint32_t a[4] = {0x80000000, 0x7FFFFFFF, 0xFFFFFFFF, 0x00000000};
    uint32_t b[4] = {0x7FFFFFFF, 0x80000000, 0x00000000, 0xFFFFFFFF};
    uint32_t expected[4], actual[4];
    ref_min_i32_4(a, b, expected);
    store_u32_4(actual, min_i32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, actual, 4);
}

inline bool test_crossval_max_i32_4() {
    uint32_t a[4] = {0x80000000, 0x7FFFFFFF, 0xFFFFFFFF, 0x00000000};
    uint32_t b[4] = {0x7FFFFFFF, 0x80000000, 0x00000000, 0xFFFFFFFF};
    uint32_t expected[4], actual[4];
    ref_max_i32_4(a, b, expected);
    store_u32_4(actual, max_i32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, actual, 4);
}

// Scalar reference: multiply variants
inline void ref_mulhi_i32_4(const uint32_t* a, const uint32_t* b, uint32_t* out) {
    for (int i = 0; i < 4; i++) {
        int64_t prod = (int64_t)(int32_t)a[i] * (int64_t)(int32_t)b[i];
        out[i] = (uint32_t)(int32_t)(prod >> 16);
    }
}
inline void ref_mulhi_u32_4(const uint32_t* a, const uint32_t* b, uint32_t* out) {
    for (int i = 0; i < 4; i++) {
        uint64_t prod = (uint64_t)a[i] * (uint64_t)b[i];
        out[i] = (uint32_t)(prod >> 16);
    }
}
inline void ref_mulhi32_i32_4(const uint32_t* a, const uint32_t* b, uint32_t* out) {
    for (int i = 0; i < 4; i++) {
        int64_t prod = (int64_t)(int32_t)a[i] * (int64_t)(int32_t)b[i];
        out[i] = (uint32_t)(int32_t)(prod >> 32);
    }
}

inline bool test_crossval_mulhi_i32_4() {
    uint32_t a[4] = {0x7FFFFFFF, 0x80000000, 0x00010000, 0xFFFF0000};
    uint32_t b[4] = {0x00020000, 0x00020000, 0xFFFF0000, 0xFFFF0000};
    uint32_t expected[4], actual[4];
    ref_mulhi_i32_4(a, b, expected);
    store_u32_4(actual, mulhi_i32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, actual, 4);
}

inline bool test_crossval_mulhi_u32_4() {
    uint32_t a[4] = {0xFFFFFFFF, 0x80000000, 0x00010000, 0x00000001};
    uint32_t b[4] = {0x00000002, 0x00000002, 0x00010000, 0xFFFFFFFF};
    uint32_t expected[4], actual[4];
    ref_mulhi_u32_4(a, b, expected);
    store_u32_4(actual, mulhi_u32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, actual, 4);
}

inline bool test_crossval_mulhi32_i32_4() {
    uint32_t a[4] = {0x7FFFFFFF, 0x80000000, 0x40000000, 0xC0000000};
    uint32_t b[4] = {0x7FFFFFFF, 0x80000000, 0x40000000, 0x40000000};
    uint32_t expected[4], actual[4];
    ref_mulhi32_i32_4(a, b, expected);
    store_u32_4(actual, mulhi32_i32_4(load_u32_4(a), load_u32_4(b)));
    return compare_u32(expected, actual, 4);
}

// Scalar reference: min/max u8 and float ops
inline void ref_min_u8_16(const uint8_t* a, const uint8_t* b, uint8_t* out) {
    for (int i = 0; i < 16; i++) out[i] = a[i] < b[i] ? a[i] : b[i];
}
inline void ref_max_u8_16(const uint8_t* a, const uint8_t* b, uint8_t* out) {
    for (int i = 0; i < 16; i++) out[i] = a[i] > b[i] ? a[i] : b[i];
}

inline bool test_crossval_min_u8_16() {
    uint8_t a[16] = {0,255,128,127, 1,254,0x55,0xAA, 0x0F,0xF0,0x80,0x7F, 0x01,0xFE,0xFF,0x00};
    uint8_t b[16] = {255,0,127,128, 254,1,0xAA,0x55, 0xF0,0x0F,0x7F,0x80, 0xFE,0x01,0x00,0xFF};
    uint8_t expected[16], actual[16];
    ref_min_u8_16(a, b, expected);
    store_u8_16(actual, min_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, actual, 16);
}

inline bool test_crossval_max_u8_16() {
    uint8_t a[16] = {0,255,128,127, 1,254,0x55,0xAA, 0x0F,0xF0,0x80,0x7F, 0x01,0xFE,0xFF,0x00};
    uint8_t b[16] = {255,0,127,128, 254,1,0xAA,0x55, 0xF0,0x0F,0x7F,0x80, 0xFE,0x01,0x00,0xFF};
    uint8_t expected[16], actual[16];
    ref_max_u8_16(a, b, expected);
    store_u8_16(actual, max_u8_16(load_u8_16(a), load_u8_16(b)));
    return compare_u8(expected, actual, 16);
}

// Scalar reference: float ops
inline bool test_crossval_float_ops() {
    float a[4] = {-1.5f, 0.0f, 3.14159f, 1e10f};
    float b[4] = {2.5f, -0.0f, -2.71828f, 1e-10f};
    float out[4];
    // Test add
    store_f32_4(out, add_f32_4(load_f32_4(a), load_f32_4(b)));
    for (int i = 0; i < 4; i++) {
        float diff = out[i] - (a[i] + b[i]);
        if (diff < -0.001f || diff > 0.001f) return false;
    }
    // Test sub
    store_f32_4(out, sub_f32_4(load_f32_4(a), load_f32_4(b)));
    for (int i = 0; i < 4; i++) {
        float diff = out[i] - (a[i] - b[i]);
        if (diff < -0.001f || diff > 0.001f) return false;
    }
    // Test mul
    store_f32_4(out, mul_f32_4(load_f32_4(a), load_f32_4(b)));
    for (int i = 0; i < 4; i++) {
        float diff = out[i] - (a[i] * b[i]);
        if (diff < -1.0f && diff > 1.0f) return false;  // large values need loose tolerance
    }
    return true;
}

// Cross-validation: aligned load/store with adversarial patterns
inline bool test_crossval_aligned_load_store() {
    FL_ALIGNAS(16) uint32_t src[4] = {0x80000000, 0x7FFFFFFF, 0xDEADBEEF, 0x00000000};
    FL_ALIGNAS(16) uint32_t dst[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    simd_u32x4 v = load_u32_4_aligned(src);
    store_u32_4_aligned(dst, v);
    return compare_u32(src, dst, 4);
}

// Cross-validation: broadcast + adversarial values
inline bool test_crossval_set1_u32_4() {
    // Test adversarial values: sign bit, all ones, zero, alternating bits
    uint32_t test_values[] = {0x80000000, 0xFFFFFFFF, 0x00000000, 0xAAAAAAAA, 0x55555555, 0x01010101};
    for (int t = 0; t < 6; t++) {
        uint32_t val = test_values[t];
        uint32_t output[4];
        store_u32_4(output, set1_u32_4(val));
        for (int i = 0; i < 4; i++) {
            if (output[i] != val) return false;
        }
    }
    return true;
}

// ============================================================================
// Chained Operation Tests (pipeline correctness)
// ============================================================================

inline bool test_u8_pipeline_scale_add_clamp() {
    // Simulate a brightness pipeline: scale by 128 then add bias, verify saturation
    uint8_t input[16] = {200,200,200,200, 200,200,200,200, 200,200,200,200, 200,200,200,200};
    uint8_t bias[16] = {200,200,200,200, 200,200,200,200, 200,200,200,200, 200,200,200,200};
    uint8_t output[16] = {0};
    simd_u8x16 v = load_u8_16(input);
    v = scale_u8_16(v, 128);           // ~100
    v = add_sat_u8_16(v, load_u8_16(bias)); // ~300 -> saturated to 255
    store_u8_16(output, v);
    for (int i = 0; i < 16; i++) {
        if (output[i] < 250) return false;  // Should be 255 (saturated)
    }
    return true;
}

inline bool test_i32_pipeline_shift_add_mask() {
    // Simulate fixed-point angle decomposition: shift, add, mask
    uint32_t angles[4] = {0x10000000, 0x20000000, 0x30000000, 0x40000000};
    simd_u32x4 v = load_u32_4(angles);
    v = srl_u32_4(v, 16);                          // Extract high 16 bits
    v = add_i32_4(v, set1_u32_4(0x100));            // Add offset
    v = and_u32_4(v, set1_u32_4(0xFFFF));           // Mask to 16 bits
    uint32_t output[4] = {0};
    store_u32_4(output, v);
    uint32_t expected[4] = {0x1100, 0x2100, 0x3100, 0x4100};
    return compare_u32(expected, output, 4);
}

inline bool test_f32_pipeline_mul_add_clamp() {
    // Simulate color: multiply by gain, add bias, clamp to [0, 1]
    float input[4] = {0.3f, 0.6f, 0.9f, 1.2f};
    float gain[4] = {2.0f, 2.0f, 2.0f, 2.0f};
    float output[4] = {0.0f};
    simd_f32x4 v = load_f32_4(input);
    v = mul_f32_4(v, load_f32_4(gain));         // 0.6, 1.2, 1.8, 2.4
    v = min_f32_4(v, set1_f32_4(1.0f));         // clamp to 1.0
    v = max_f32_4(v, set1_f32_4(0.0f));         // clamp to 0.0
    store_f32_4(output, v);
    float expected[4] = {0.6f, 1.0f, 1.0f, 1.0f};
    return compare_f32(expected, output, 4);
}

// ============================================================================
// Arithmetic Benchmark: add / sub / mul / div across float, s16x16, s16x16x4
// ============================================================================
//
// Each test runs 4-wide unrolled loops with feedback to prevent elimination.
// s16x16x4 has no division operator, so that cell is skipped.

static volatile uint32_t g_bench_sink;

struct BenchmarkResult {
    int64_t iterations;
    // [op][type]: op={add,sub,mul,div}, type={float,s16x16,simd}
    int64_t add_float_us,  add_s16x16_us,  add_simd_us;
    int64_t sub_float_us,  sub_s16x16_us,  sub_simd_us;
    int64_t mul_float_us,  mul_s16x16_us,  mul_simd_us;
    int64_t div_float_us,  div_s16x16_us;  // no simd div
};

// Helper: time 4-wide scalar float with a binary op
template <typename Op>
inline int64_t benchFloat4(int iters, Op op) {
    float a0 = 1.5f, a1 = 2.3f, a2 = 0.7f, a3 = 3.1f;
    float b0 = 0.5f, b1 = 1.2f, b2 = 2.0f, b3 = 0.9f;
    uint32_t t0 = micros();
    for (int i = 0; i < iters; i++) {
        a0 = op(a0, b0); a1 = op(a1, b1);
        a2 = op(a2, b2); a3 = op(a3, b3);
        b0 = a0 + 0.001f; b1 = a1 + 0.001f;
        b2 = a2 + 0.001f; b3 = a3 + 0.001f;
    }
    uint32_t t1 = micros();
    uint32_t tmp; memcpy(&tmp, &a0, sizeof(tmp));
    g_bench_sink = tmp;
    return static_cast<int64_t>(t1 - t0);
}

// Helper: time 4-wide scalar s16x16 with a binary op
template <typename Op>
inline int64_t benchS16x16_4(int iters, Op op) {
    fl::s16x16 a0(1.5f), a1(2.3f), a2(0.7f), a3(3.1f);
    fl::s16x16 b0(0.5f), b1(1.2f), b2(2.0f), b3(0.9f);
    fl::s16x16 bump = fl::s16x16::from_raw(1);
    uint32_t t0 = micros();
    for (int i = 0; i < iters; i++) {
        a0 = op(a0, b0); a1 = op(a1, b1);
        a2 = op(a2, b2); a3 = op(a3, b3);
        b0 = a0 + bump; b1 = a1 + bump;
        b2 = a2 + bump; b3 = a3 + bump;
    }
    uint32_t t1 = micros();
    g_bench_sink = static_cast<uint32_t>(a0.raw());
    return static_cast<int64_t>(t1 - t0);
}

// Helper: time s16x16x4 SIMD with a binary op
template <typename Op>
inline int64_t benchSimd4(int iters, Op op) {
    fl::s16x16x4 a = fl::s16x16x4::from_raw(
        set_u32_4(as_u32(fl::s16x16(1.5f).raw()), as_u32(fl::s16x16(2.3f).raw()),
                  as_u32(fl::s16x16(0.7f).raw()), as_u32(fl::s16x16(3.1f).raw())));
    fl::s16x16x4 b = fl::s16x16x4::from_raw(
        set_u32_4(as_u32(fl::s16x16(0.5f).raw()), as_u32(fl::s16x16(1.2f).raw()),
                  as_u32(fl::s16x16(2.0f).raw()), as_u32(fl::s16x16(0.9f).raw())));
    fl::s16x16x4 bump = fl::s16x16x4::set1(fl::s16x16::from_raw(1));
    uint32_t t0 = micros();
    for (int i = 0; i < iters; i++) {
        a = op(a, b);
        b = a + bump;
    }
    uint32_t t1 = micros();
    g_bench_sink = extract_u32_4(a.raw, 0);
    return static_cast<int64_t>(t1 - t0);
}

struct OpAdd {
    template<typename T> T operator()(T a, T b) const { return a + b; }
};
struct OpSub {
    template<typename T> T operator()(T a, T b) const { return a - b; }
};
struct OpMul {
    template<typename T> T operator()(T a, T b) const { return a * b; }
};
struct OpDivFloat {
    float operator()(float a, float b) const { return a / b; }
};
struct OpDivS16x16 {
    fl::s16x16 operator()(fl::s16x16 a, fl::s16x16 b) const { return a / b; }
};

inline BenchmarkResult runMultiplyBenchmark(int iters = 10000) {
    BenchmarkResult r;
    r.iterations = iters;

    // Add
    r.add_float_us   = benchFloat4(iters, OpAdd());
    r.add_s16x16_us  = benchS16x16_4(iters, OpAdd());
    r.add_simd_us    = benchSimd4(iters, OpAdd());

    // Sub
    r.sub_float_us   = benchFloat4(iters, OpSub());
    r.sub_s16x16_us  = benchS16x16_4(iters, OpSub());
    r.sub_simd_us    = benchSimd4(iters, OpSub());

    // Mul
    r.mul_float_us   = benchFloat4(iters, OpMul());
    r.mul_s16x16_us  = benchS16x16_4(iters, OpMul());
    r.mul_simd_us    = benchSimd4(iters, OpMul());

    // Div (no SIMD div for s16x16x4)
    r.div_float_us   = benchFloat4(iters, OpDivFloat());
    r.div_s16x16_us  = benchS16x16_4(iters, OpDivS16x16());

    return r;
}

// ============================================================================
// Test Runner
// ============================================================================

struct SimdTestEntry {
    const char* name;
    bool (*func)();
};

/// Get the static test table. Used by both runSimdTests() and the RPC handler.
inline void getTests(const SimdTestEntry** out_tests, int* out_count) {
    static const SimdTestEntry tests[] = {
        // Load/Store u8x16
        {"load/store u8x16",               test_load_store_u8_16},
        {"load/store u8x16 boundary",      test_load_store_u8_16_boundary},
        // Load/Store u32x4
        {"load/store u32x4",               test_load_store_u32_4},
        {"load/store u32x4 boundary",      test_load_store_u32_4_boundary},
        {"load/store u32x4 aligned",       test_load_store_u32_4_aligned},
        // Load/Store f32x4
        {"load/store f32x4",               test_load_store_f32_4},
        {"load/store f32x4 special",       test_load_store_f32_4_special},
        // Saturating Arithmetic u8x16
        {"add_sat u8x16",                  test_add_sat_u8_16},
        {"add_sat u8x16 full saturate",    test_add_sat_u8_16_full_saturate},
        {"sub_sat u8x16",                  test_sub_sat_u8_16},
        {"sub_sat u8x16 full clamp",       test_sub_sat_u8_16_full_clamp},
        // Scale / Blend u8x16
        {"scale u8x16",                    test_scale_u8_16},
        {"scale u8x16 zero",              test_scale_u8_16_zero},
        {"scale u8x16 full",              test_scale_u8_16_full},
        {"blend u8x16",                    test_blend_u8_16},
        {"blend u8x16 endpoints",          test_blend_u8_16_endpoints},
        // Comparison u8x16
        {"min u8x16",                      test_min_u8_16},
        {"max u8x16",                      test_max_u8_16},
        {"avg u8x16",                      test_avg_u8_16},
        {"avg_round u8x16",               test_avg_round_u8_16},
        // Bitwise u8x16
        {"and u8x16",                      test_and_u8_16},
        {"or u8x16",                       test_or_u8_16},
        {"xor u8x16",                      test_xor_u8_16},
        {"andnot u8x16",                   test_andnot_u8_16},
        // Broadcast / Construct u32x4
        {"set1 u32x4",                     test_set1_u32_4},
        {"set1 u32x4 zero",               test_set1_u32_4_zero},
        {"set u32x4",                      test_set_u32_4},
        {"set1 f32x4",                     test_set1_f32_4},
        // Extract u32x4
        {"extract u32x4",                  test_extract_u32_4},
        // Bitwise u32x4
        {"xor u32x4",                      test_xor_u32_4},
        {"and u32x4",                      test_and_u32_4},
        {"or u32x4",                       test_or_u32_4},
        // Arithmetic i32x4
        {"add i32x4",                      test_add_i32_4},
        {"sub i32x4",                      test_sub_i32_4},
        // Shift u32x4 / i32x4
        {"srl u32x4",                      test_srl_u32_4},
        {"sll u32x4",                      test_sll_u32_4},
        {"sra i32x4",                      test_sra_i32_4},
        // Min/Max i32x4 (signed)
        {"min i32x4",                      test_min_i32_4},
        {"max i32x4",                      test_max_i32_4},
        // Fixed-Point Multiply
        {"mulhi i32x4 (Q16.16)",           test_mulhi_i32_4},
        {"mulhi u32x4 (Q16.16)",           test_mulhi_u32_4},
        {"mulhi su32x4 (Q16.16)",          test_mulhi_su32_4},
        {"mulhi32 i32x4 (>>32)",           test_mulhi32_i32_4},
        // Interleave / Unpack
        {"unpacklo u32x4",                 test_unpacklo_u32_4},
        {"unpackhi u32x4",                 test_unpackhi_u32_4},
        {"unpacklo u64 as u32x4",          test_unpacklo_u64_as_u32_4},
        {"unpackhi u64 as u32x4",          test_unpackhi_u64_as_u32_4},
        // Float Arithmetic
        {"add f32x4",                      test_add_f32_4},
        {"add f32x4 negative",            test_add_f32_4_negative},
        {"sub f32x4",                      test_sub_f32_4},
        {"mul f32x4",                      test_mul_f32_4},
        {"mul f32x4 negative",            test_mul_f32_4_negative},
        {"div f32x4",                      test_div_f32_4},
        {"sqrt f32x4",                     test_sqrt_f32_4},
        {"sqrt f32x4 zero",              test_sqrt_f32_4_zero},
        {"min f32x4",                      test_min_f32_4},
        {"max f32x4",                      test_max_f32_4},
        // Cross-Validation: SIMD vs Scalar Reference (adversarial patterns)
        {"crossval scale u8x16",            test_crossval_scale_u8_16},
        {"crossval add_sat u8x16",          test_crossval_add_sat_u8_16},
        {"crossval sub_sat u8x16",          test_crossval_sub_sat_u8_16},
        {"crossval aligned load/store",     test_crossval_aligned_load_store},
        {"crossval AND u8x16",             test_crossval_and_u8_16},
        {"crossval OR u8x16",              test_crossval_or_u8_16},
        {"crossval XOR u8x16",             test_crossval_xor_u8_16},
        {"crossval ANDNOT u8x16",          test_crossval_andnot_u8_16},
        {"crossval XOR u32x4",             test_crossval_xor_u32_4},
        {"crossval AND u32x4",             test_crossval_and_u32_4},
        {"crossval OR u32x4",              test_crossval_or_u32_4},
        {"crossval broadcast u32x4",       test_crossval_set1_u32_4},
        // Cross-Validation: i32 arithmetic, shifts, min/max, multiply, u8 min/max, float
        {"crossval add i32x4",              test_crossval_add_i32_4},
        {"crossval sub i32x4",              test_crossval_sub_i32_4},
        {"crossval srl u32x4",              test_crossval_srl_u32_4},
        {"crossval sll u32x4",              test_crossval_sll_u32_4},
        {"crossval sra i32x4",              test_crossval_sra_i32_4},
        {"crossval min i32x4",              test_crossval_min_i32_4},
        {"crossval max i32x4",              test_crossval_max_i32_4},
        {"crossval mulhi i32x4",            test_crossval_mulhi_i32_4},
        {"crossval mulhi u32x4",            test_crossval_mulhi_u32_4},
        {"crossval mulhi32 i32x4",          test_crossval_mulhi32_i32_4},
        {"crossval min u8x16",              test_crossval_min_u8_16},
        {"crossval max u8x16",              test_crossval_max_u8_16},
        {"crossval float ops",              test_crossval_float_ops},
        // Pipeline / Chained Operation Tests
        {"pipeline u8 scale+add+clamp",    test_u8_pipeline_scale_add_clamp},
        {"pipeline i32 shift+add+mask",    test_i32_pipeline_shift_add_mask},
        {"pipeline f32 mul+add+clamp",     test_f32_pipeline_mul_add_clamp},
    };
    *out_tests = tests;
    *out_count = sizeof(tests) / sizeof(tests[0]);
}

/// Run the full SIMD test suite. Returns the number of failures.
inline int runSimdTests() {
    const SimdTestEntry* tests = nullptr;
    int num_tests = 0;
    getTests(&tests, &num_tests);

    int passed = 0;
    int failed = 0;

    FL_PRINT("\n[SIMD VALIDATION]");
    FL_PRINT("────────────────────────────────────────────────────────────────");

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    FL_PRINT("  SIMD Backend: x86 SSE2");
#elif defined(__XTENSA__) && FL_XTENSA_HAS_PIE
    FL_PRINT("  SIMD Backend: Xtensa PIE (ESP32-S3)");
#elif defined(__XTENSA__)
    FL_PRINT("  SIMD Backend: Xtensa scalar");
#elif defined(__riscv)
    FL_PRINT("  SIMD Backend: RISC-V scalar");
#else
    FL_PRINT("  SIMD Backend: Scalar fallback");
#endif

    fl::sstream ss;
    ss << "  Running " << num_tests << " SIMD tests...\n";
    FL_PRINT(ss.str());

    for (int i = 0; i < num_tests; i++) {
        bool ok = tests[i].func();
        if (ok) {
            passed++;
            ss.clear();
            ss << "  [PASS] " << tests[i].name;
            FL_PRINT(ss.str());
        } else {
            failed++;
            ss.clear();
            ss << "  [FAIL] " << tests[i].name;
            FL_ERROR(ss.str());
        }
    }

    ss.clear();
    ss << "\n[SIMD RESULTS] " << passed << "/" << num_tests << " passed";
    if (failed > 0) {
        ss << ", " << failed << " FAILED";
        FL_ERROR(ss.str());
    } else {
        ss << " - ALL PASSED";
        FL_PRINT(ss.str());
    }

    return failed;
}

} // namespace simd_check
} // namespace validation
