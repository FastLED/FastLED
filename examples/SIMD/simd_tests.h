// examples/SIMD/simd_tests.h
//
// SIMD test function declarations

#pragma once

#include "test_result.h"

namespace simd_test {

// ============================================================================
// Load/Store Tests
// ============================================================================

void test_load_store_u8_16(TestResult& result);
void test_load_store_u32_4(TestResult& result);
void test_load_store_f32_4(TestResult& result);

// ============================================================================
// Arithmetic Tests
// ============================================================================

void test_add_sat_u8_16(TestResult& result);
void test_sub_sat_u8_16(TestResult& result);
void test_scale_u8_16(TestResult& result);
void test_blend_u8_16(TestResult& result);

// ============================================================================
// Comparison Tests
// ============================================================================

void test_min_u8_16(TestResult& result);
void test_max_u8_16(TestResult& result);
void test_avg_u8_16(TestResult& result);
void test_avg_round_u8_16(TestResult& result);

// ============================================================================
// Bitwise Tests
// ============================================================================

void test_and_u8_16(TestResult& result);
void test_or_u8_16(TestResult& result);
void test_xor_u8_16(TestResult& result);
void test_andnot_u8_16(TestResult& result);

// ============================================================================
// Broadcast Tests
// ============================================================================

void test_set1_u32_4(TestResult& result);
void test_set1_f32_4(TestResult& result);

// ============================================================================
// Floating Point Tests
// ============================================================================

void test_add_f32_4(TestResult& result);
void test_sub_f32_4(TestResult& result);
void test_mul_f32_4(TestResult& result);
void test_div_f32_4(TestResult& result);
void test_sqrt_f32_4(TestResult& result);
void test_min_f32_4(TestResult& result);
void test_max_f32_4(TestResult& result);

} // namespace simd_test
